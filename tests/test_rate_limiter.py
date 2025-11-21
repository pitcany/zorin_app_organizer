"""
Tests for rate limiting functionality
"""

import pytest
import time
from rate_limiter import (
    RateLimiter,
    MultiRateLimiter,
    AdaptiveRateLimiter,
    RateLimitExceeded,
    rate_limit,
    _api_rate_limiters
)


class TestRateLimiter:
    """Tests for RateLimiter class"""

    @pytest.mark.unit
    def test_basic_rate_limiting(self):
        """Test basic rate limiting functionality"""
        limiter = RateLimiter(max_requests=3, time_window=1)

        # First 3 requests should be allowed
        assert limiter.is_allowed() == True
        assert limiter.is_allowed() == True
        assert limiter.is_allowed() == True

        # 4th request should be denied
        assert limiter.is_allowed() == False

    @pytest.mark.unit
    def test_rate_limit_reset(self):
        """Test that rate limit resets after time window"""
        limiter = RateLimiter(max_requests=2, time_window=1)

        # Use up limit
        assert limiter.is_allowed() == True
        assert limiter.is_allowed() == True
        assert limiter.is_allowed() == False

        # Wait for reset
        time.sleep(1.1)

        # Should be allowed again
        assert limiter.is_allowed() == True

    @pytest.mark.unit
    def test_get_wait_time(self):
        """Test wait time calculation"""
        limiter = RateLimiter(max_requests=2, time_window=5)

        # Use up limit
        limiter.is_allowed()
        limiter.is_allowed()

        # Get wait time
        wait_time = limiter.get_wait_time()

        assert wait_time > 0
        assert wait_time <= 5

    @pytest.mark.unit
    def test_reset(self):
        """Test manual reset"""
        limiter = RateLimiter(max_requests=2, time_window=10)

        # Use up limit
        limiter.is_allowed()
        limiter.is_allowed()
        assert limiter.is_allowed() == False

        # Reset
        limiter.reset()

        # Should be allowed again
        assert limiter.is_allowed() == True


class TestMultiRateLimiter:
    """Tests for MultiRateLimiter class"""

    @pytest.mark.unit
    def test_per_key_limiting(self):
        """Test that different keys have separate limits"""
        multi_limiter = MultiRateLimiter(default_max_requests=2, default_time_window=10)

        # Use up limit for key1
        assert multi_limiter.is_allowed('key1') == True
        assert multi_limiter.is_allowed('key1') == True
        assert multi_limiter.is_allowed('key1') == False

        # key2 should still be allowed
        assert multi_limiter.is_allowed('key2') == True
        assert multi_limiter.is_allowed('key2') == True

    @pytest.mark.unit
    def test_reset_specific_key(self):
        """Test resetting a specific key"""
        multi_limiter = MultiRateLimiter(default_max_requests=1, default_time_window=10)

        # Use up limits
        multi_limiter.is_allowed('key1')
        multi_limiter.is_allowed('key2')

        # Reset only key1
        multi_limiter.reset('key1')

        # key1 should be allowed, key2 should not
        assert multi_limiter.is_allowed('key1') == True
        assert multi_limiter.is_allowed('key2') == False


class TestRateLimitDecorator:
    """Tests for rate_limit decorator"""

    @pytest.mark.unit
    def test_decorator_blocks_excess_calls(self):
        """Test that decorator blocks excess calls"""
        # Create a test limiter
        _api_rate_limiters['test_decorator'] = RateLimiter(max_requests=2, time_window=10)

        @rate_limit('test_decorator', wait=False)
        def test_function():
            return "success"

        # First 2 calls should succeed
        assert test_function() == "success"
        assert test_function() == "success"

        # 3rd call should raise exception
        with pytest.raises(RateLimitExceeded):
            test_function()

    @pytest.mark.unit
    def test_decorator_with_wait(self):
        """Test decorator with wait=True"""
        _api_rate_limiters['test_wait'] = RateLimiter(max_requests=2, time_window=1)

        call_times = []

        @rate_limit('test_wait', wait=True)
        def test_function():
            call_times.append(time.time())
            return "success"

        # Make 3 calls - 3rd should wait
        test_function()
        test_function()
        test_function()

        # All calls should succeed
        assert len(call_times) == 3


class TestAdaptiveRateLimiter:
    """Tests for AdaptiveRateLimiter class"""

    @pytest.mark.unit
    def test_adaptation_on_errors(self):
        """Test that limiter reduces on high error rate"""
        limiter = AdaptiveRateLimiter(initial_max_requests=100, time_window=60)
        limiter.sample_size = 10  # Small sample for testing

        initial_max = limiter.current_max

        # Record high error rate
        for i in range(7):
            limiter.record_error()
        for i in range(3):
            limiter.record_success()

        # Limit should be reduced
        assert limiter.current_max < initial_max

    @pytest.mark.unit
    def test_adaptation_on_success(self):
        """Test that limiter increases on high success rate"""
        limiter = AdaptiveRateLimiter(initial_max_requests=50, time_window=60)
        limiter.sample_size = 10
        limiter.current_max = 25  # Start at half capacity

        # Record high success rate
        for i in range(19):
            limiter.record_success()
        for i in range(1):
            limiter.record_error()

        # Limit should be increased
        assert limiter.current_max > 25

    @pytest.mark.unit
    def test_min_max_bounds(self):
        """Test that limiter respects min/max bounds"""
        limiter = AdaptiveRateLimiter(initial_max_requests=100, time_window=60)
        limiter.sample_size = 10

        # Record all errors
        for i in range(20):
            limiter.record_error()

        # Should not go below minimum
        assert limiter.current_max >= limiter.min_requests

        # Record all successes
        limiter.current_max = 50
        for i in range(20):
            limiter.record_success()

        # Should not go above maximum
        assert limiter.current_max <= limiter.max_requests


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
