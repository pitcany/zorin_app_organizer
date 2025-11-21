"""
Rate limiting module for Unified Package Manager
Prevents API abuse and DoS attacks through request throttling
"""

import time
import threading
from datetime import datetime, timezone, timedelta
from collections import deque
from typing import Dict, Optional, Callable, Any
from functools import wraps


class RateLimiter:
    """
    Token bucket rate limiter for API calls

    Implements a thread-safe token bucket algorithm for rate limiting.
    """

    def __init__(self, max_requests: int, time_window: int):
        """
        Initialize rate limiter

        Args:
            max_requests: Maximum number of requests allowed in time window
            time_window: Time window in seconds
        """
        self.max_requests = max_requests
        self.time_window = time_window
        self._requests = deque()
        self._lock = threading.Lock()

    def is_allowed(self, key: str = "default") -> bool:
        """
        Check if request is allowed

        Args:
            key: Identifier for the rate limit (e.g., IP address, API endpoint)

        Returns:
            True if request is allowed, False otherwise
        """
        with self._lock:
            now = time.time()
            cutoff = now - self.time_window

            # Remove expired requests
            while self._requests and self._requests[0] < cutoff:
                self._requests.popleft()

            # Check if under limit
            if len(self._requests) < self.max_requests:
                self._requests.append(now)
                return True

            return False

    def wait_if_needed(self, key: str = "default") -> float:
        """
        Wait until request is allowed

        Args:
            key: Identifier for the rate limit

        Returns:
            Time waited in seconds
        """
        start = time.time()

        while not self.is_allowed(key):
            time.sleep(0.1)  # Sleep 100ms between checks

        return time.time() - start

    def get_wait_time(self, key: str = "default") -> float:
        """
        Get estimated wait time until next request is allowed

        Args:
            key: Identifier for the rate limit

        Returns:
            Wait time in seconds, or 0 if request is immediately allowed
        """
        with self._lock:
            if len(self._requests) < self.max_requests:
                return 0.0

            # Calculate time until oldest request expires
            now = time.time()
            oldest = self._requests[0]
            wait = (oldest + self.time_window) - now

            return max(0.0, wait)

    def reset(self):
        """Reset rate limiter (clear all tracked requests)"""
        with self._lock:
            self._requests.clear()


class MultiRateLimiter:
    """
    Manages multiple rate limiters for different keys (e.g., per-endpoint)
    """

    def __init__(self, default_max_requests: int = 100, default_time_window: int = 60):
        """
        Initialize multi-rate limiter

        Args:
            default_max_requests: Default max requests per window
            default_time_window: Default time window in seconds
        """
        self.default_max_requests = default_max_requests
        self.default_time_window = default_time_window
        self._limiters: Dict[str, RateLimiter] = {}
        self._lock = threading.Lock()

    def get_limiter(self, key: str) -> RateLimiter:
        """
        Get or create rate limiter for a specific key

        Args:
            key: Identifier for the rate limiter

        Returns:
            RateLimiter instance
        """
        with self._lock:
            if key not in self._limiters:
                self._limiters[key] = RateLimiter(
                    self.default_max_requests,
                    self.default_time_window
                )
            return self._limiters[key]

    def is_allowed(self, key: str) -> bool:
        """Check if request is allowed for a specific key"""
        return self.get_limiter(key).is_allowed(key)

    def wait_if_needed(self, key: str) -> float:
        """Wait until request is allowed for a specific key"""
        return self.get_limiter(key).wait_if_needed(key)

    def reset(self, key: Optional[str] = None):
        """
        Reset rate limiter(s)

        Args:
            key: Specific key to reset, or None to reset all
        """
        with self._lock:
            if key is None:
                self._limiters.clear()
            elif key in self._limiters:
                del self._limiters[key]


# Global rate limiter instances
_api_rate_limiters = {
    'flathub': RateLimiter(max_requests=30, time_window=60),    # 30 requests per minute
    'snap': RateLimiter(max_requests=20, time_window=60),       # 20 requests per minute
    'search': RateLimiter(max_requests=10, time_window=60),     # 10 searches per minute
    'install': RateLimiter(max_requests=5, time_window=300),    # 5 installs per 5 minutes
    'default': RateLimiter(max_requests=100, time_window=60),   # 100 requests per minute
}


def rate_limit(limiter_key: str = 'default', wait: bool = False):
    """
    Decorator for rate-limiting functions

    Args:
        limiter_key: Key for the rate limiter to use
        wait: If True, wait until allowed; if False, raise exception

    Example:
        @rate_limit('flathub', wait=True)
        def fetch_flathub_data():
            ...
    """
    def decorator(func: Callable) -> Callable:
        @wraps(func)
        def wrapper(*args, **kwargs) -> Any:
            limiter = _api_rate_limiters.get(limiter_key, _api_rate_limiters['default'])

            if wait:
                # Wait until allowed
                wait_time = limiter.wait_if_needed()
                if wait_time > 0:
                    print(f"⏳ Rate limit: waited {wait_time:.2f}s for {limiter_key}")

            else:
                # Check if allowed, raise exception if not
                if not limiter.is_allowed():
                    wait_time = limiter.get_wait_time()
                    raise RateLimitExceeded(
                        f"Rate limit exceeded for {limiter_key}. "
                        f"Please wait {wait_time:.1f}s before retrying."
                    )

            return func(*args, **kwargs)

        return wrapper
    return decorator


class RateLimitExceeded(Exception):
    """Exception raised when rate limit is exceeded"""
    pass


class AdaptiveRateLimiter:
    """
    Adaptive rate limiter that adjusts limits based on success/failure rates

    Automatically reduces rate limits when detecting errors or service issues.
    """

    def __init__(self, initial_max_requests: int = 100, time_window: int = 60):
        """
        Initialize adaptive rate limiter

        Args:
            initial_max_requests: Initial maximum requests per window
            time_window: Time window in seconds
        """
        self.time_window = time_window
        self.min_requests = max(1, initial_max_requests // 10)  # Minimum 10% of initial
        self.max_requests = initial_max_requests
        self.current_max = initial_max_requests

        self._limiter = RateLimiter(self.current_max, time_window)
        self._success_count = 0
        self._error_count = 0
        self._lock = threading.Lock()

        # Adaptation parameters
        self.error_threshold = 0.3  # 30% error rate triggers reduction
        self.success_threshold = 0.95  # 95% success rate allows increase
        self.sample_size = 20  # Evaluate every 20 requests

    def is_allowed(self, key: str = "default") -> bool:
        """Check if request is allowed"""
        return self._limiter.is_allowed(key)

    def record_success(self):
        """Record a successful request"""
        with self._lock:
            self._success_count += 1
            self._adapt_if_needed()

    def record_error(self):
        """Record a failed request"""
        with self._lock:
            self._error_count += 1
            self._adapt_if_needed()

    def _adapt_if_needed(self):
        """Adapt rate limit based on success/error rates"""
        total = self._success_count + self._error_count

        if total < self.sample_size:
            return

        error_rate = self._error_count / total

        # High error rate - reduce limit
        if error_rate > self.error_threshold:
            new_max = max(self.min_requests, self.current_max // 2)
            if new_max != self.current_max:
                self.current_max = new_max
                self._limiter = RateLimiter(self.current_max, self.time_window)
                print(f"⚠️  Rate limit reduced to {self.current_max} req/{self.time_window}s "
                      f"(error rate: {error_rate:.1%})")

        # Low error rate - increase limit
        elif error_rate < (1 - self.success_threshold):
            new_max = min(self.max_requests, int(self.current_max * 1.5))
            if new_max != self.current_max:
                self.current_max = new_max
                self._limiter = RateLimiter(self.current_max, self.time_window)
                print(f"✅ Rate limit increased to {self.current_max} req/{self.time_window}s "
                      f"(error rate: {error_rate:.1%})")

        # Reset counters
        self._success_count = 0
        self._error_count = 0


def adaptive_rate_limit(limiter: AdaptiveRateLimiter):
    """
    Decorator for adaptive rate limiting with automatic error tracking

    Args:
        limiter: AdaptiveRateLimiter instance

    Example:
        flathub_limiter = AdaptiveRateLimiter(initial_max_requests=30, time_window=60)

        @adaptive_rate_limit(flathub_limiter)
        def fetch_flathub_data():
            ...
    """
    def decorator(func: Callable) -> Callable:
        @wraps(func)
        def wrapper(*args, **kwargs) -> Any:
            # Check rate limit
            if not limiter.is_allowed():
                wait_time = limiter._limiter.get_wait_time()
                raise RateLimitExceeded(
                    f"Rate limit exceeded. Please wait {wait_time:.1f}s before retrying."
                )

            # Execute function and track success/error
            try:
                result = func(*args, **kwargs)
                limiter.record_success()
                return result
            except Exception as e:
                limiter.record_error()
                raise

        return wrapper
    return decorator


# Example usage and testing
if __name__ == '__main__':
    print("Testing Rate Limiter...")

    # Test basic rate limiter
    limiter = RateLimiter(max_requests=5, time_window=10)

    print("\n1. Testing basic rate limiter (5 requests per 10 seconds):")
    for i in range(7):
        if limiter.is_allowed():
            print(f"   Request {i+1}: ✅ Allowed")
        else:
            wait_time = limiter.get_wait_time()
            print(f"   Request {i+1}: ❌ Denied (wait {wait_time:.1f}s)")

    # Test decorator
    print("\n2. Testing rate limit decorator:")

    @rate_limit('test', wait=False)
    def test_function():
        print("   Function executed ✅")
        return "Success"

    # Replace with test limiter
    _api_rate_limiters['test'] = RateLimiter(max_requests=3, time_window=5)

    for i in range(5):
        try:
            test_function()
        except RateLimitExceeded as e:
            print(f"   {e}")

    print("\n✅ Rate limiter tests complete")
