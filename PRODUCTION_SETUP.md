# Production Setup Guide

This guide covers setting up the Unified Package Manager for production use with all security enhancements and automated features enabled.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Installation](#installation)
3. [Automated Database Backups](#automated-database-backups)
4. [CI/CD Security Scanning](#cicd-security-scanning)
5. [Rate Limiting Configuration](#rate-limiting-configuration)
6. [Package Signature Verification](#package-signature-verification)
7. [Monitoring and Maintenance](#monitoring-and-maintenance)
8. [Troubleshooting](#troubleshooting)

## Prerequisites

### System Requirements

- **OS:** Ubuntu 20.04+ or compatible Linux distribution
- **Python:** 3.9, 3.10, or 3.11
- **systemd:** For automated backup scheduling
- **Git:** For version control and updates

### Required Packages

```bash
# Update package lists
sudo apt update

# Install system dependencies
sudo apt install -y python3 python3-pip python3-venv git

# Optional: Install package managers for full functionality
sudo apt install -y flatpak snapd
```

## Installation

### 1. Clone Repository

```bash
# Clone the repository
git clone https://github.com/yourusername/zorin_app_organizer.git
cd zorin_app_organizer

# Checkout production branch
git checkout main
```

### 2. Set Up Python Environment

```bash
# Create virtual environment
python3 -m venv venv

# Activate virtual environment
source venv/bin/activate

# Upgrade pip
pip install --upgrade pip

# Install dependencies
pip install -r requirements.txt

# Install test dependencies (optional, for testing)
pip install -r requirements-test.txt
```

### 3. Initialize Database

```bash
# Initialize the database with automatic backups enabled (default)
python -c "from database import DatabaseManager; db = DatabaseManager(); print('Database initialized')"

# Or disable automatic backups if desired
python -c "from database import DatabaseManager; db = DatabaseManager(enable_auto_backup=False); print('Database initialized without backups')"
```

## Automated Database Backups

### systemd Timer Installation

The application includes a systemd-based automated backup system that runs daily at 3 AM.

#### 1. Install systemd Units

```bash
# Run the installation script
cd zorin_app_organizer
bash systemd/install-systemd.sh

# This will:
# - Install backup script to /usr/local/bin/upm-backup
# - Install systemd units to ~/.config/systemd/user/
# - Enable and start the backup timer
```

#### 2. Verify Installation

```bash
# Check timer status
systemctl --user status upm-backup.timer

# List next scheduled backup
systemctl --user list-timers upm-backup.timer

# View timer journal logs
journalctl --user -u upm-backup.timer
```

#### 3. Manual Backup Commands

```bash
# Run backup immediately
systemctl --user start upm-backup.service

# View backup service logs
journalctl --user -u upm-backup.service

# Check last backup status
systemctl --user status upm-backup.service
```

### Backup Configuration

Backups are stored in `~/.local/share/upm/backups/` by default.

#### Backup Retention Policy

- **Daily backups:** Keep 7 most recent
- **Weekly backups:** Keep 4 most recent
- **Monthly backups:** Keep 6 most recent
- **Manual backups:** Never auto-deleted

#### Customizing Backup Settings

Edit `backup.py` to customize:

```python
class DatabaseBackup:
    def __init__(self, db_path: str = "upm.db", backup_dir: str = None):
        # Retention policies
        self.max_daily_backups = 7      # Change this
        self.max_weekly_backups = 4     # Change this
        self.max_monthly_backups = 6    # Change this
```

### Application-Level Backups

Backups also occur automatically within the application:

- **On initialization:** Creates initial backup
- **Every 50 operations:** Auto-backup (install, uninstall, etc.)
- **Manual trigger:** Via `DatabaseManager.create_manual_backup()`

#### Example Usage in Python

```python
from database import DatabaseManager

# Initialize with auto-backups enabled (default)
db = DatabaseManager()

# Create manual backup
backup_file = db.create_manual_backup()
print(f"Backup created: {backup_file}")

# List all backups
backups = db.list_backups()
for backup in backups:
    print(f"{backup['type']}: {backup['file']} ({backup['size']})")

# Restore from backup
success = db.restore_from_backup('/path/to/backup.db.gz')
```

## CI/CD Security Scanning

The project includes automated security scanning via GitHub Actions.

### Workflows Overview

#### 1. Security Scan Workflow

**Triggers:**
- Every push to main/develop
- Every pull request
- Weekly schedule (Monday 9 AM UTC)

**Scans:**
- Dependency vulnerabilities (Safety, pip-audit)
- Code security issues (Bandit)
- Secret detection (Gitleaks)
- Advanced security (CodeQL)

#### 2. Tests Workflow

**Triggers:**
- Every push
- Every pull request

**Tests:**
- Python 3.9, 3.10, 3.11 compatibility
- 77+ unit and integration tests
- Code coverage reporting
- Code quality checks (flake8, black, isort)

### Monitoring Security Alerts

See [GITHUB_ACTIONS_MONITORING.md](GITHUB_ACTIONS_MONITORING.md) for detailed monitoring guide.

**Quick access:**
1. Go to repository → Actions tab
2. Review failed workflows
3. Check Security tab for alerts
4. Respond according to severity

### Local Security Scanning

Run security scans locally before committing:

```bash
# Install security tools
pip install bandit safety pip-audit gitleaks

# Run Bandit
bandit -r . -f json -o bandit-report.json

# Run Safety
safety check

# Run pip-audit
pip-audit

# Run Gitleaks (requires gitleaks binary)
gitleaks detect --source . -v
```

## Rate Limiting Configuration

Rate limiting is automatically enabled to prevent API abuse.

### Default Limits

```python
# From rate_limiter.py
_api_rate_limiters = {
    'flathub': RateLimiter(max_requests=30, time_window=60),    # 30/min
    'snap': RateLimiter(max_requests=20, time_window=60),       # 20/min
    'search': RateLimiter(max_requests=10, time_window=60),     # 10/min
    'install': RateLimiter(max_requests=5, time_window=300),    # 5/5min
    'default': RateLimiter(max_requests=100, time_window=60),   # 100/min
}
```

### Customizing Rate Limits

Edit `rate_limiter.py` to adjust limits:

```python
# Increase Flathub limit to 60 requests per minute
_api_rate_limiters['flathub'] = RateLimiter(max_requests=60, time_window=60)
```

### Monitoring Rate Limiting

Rate limiting logs are printed to console:

```
⏳ Rate limit: waited 2.34s for flathub
```

### Disabling Rate Limiting

To disable rate limiting (not recommended):

```python
# In flatpak_backend.py, remove @rate_limit decorator
# @rate_limit('flathub', wait=True)  # Comment this out
def _search_via_api(self, query: str, limit: int) -> List[Dict]:
    ...
```

## Package Signature Verification

Signature verification is automatically performed before package installation.

### How It Works

1. **APT packages:** Verifies repository GPG keys
2. **Snap packages:** Verifies snap assertions
3. **Flatpak packages:** Verifies OSTree signatures

### Verification Process

```python
# Example verification flow
print("🔐 Verifying package signature for firefox...")
verified, msg = verifier.verify_apt_package_signature('firefox')

if verified:
    print("✅ Signature verified")
else:
    print(f"⚠️  Warning: {msg}")
    # Installation continues with warning
```

### Verification Output

```
🔐 Verifying package signature for org.mozilla.firefox...
✅ Package signature verified (Flatpak)
```

### Manual Verification

```python
from package_verification import PackageVerifier

verifier = PackageVerifier()

# Verify APT package
success, msg = verifier.verify_apt_package_signature('curl')
print(msg)

# Verify Snap package
success, msg = verifier.verify_snap_signature('firefox')
print(msg)

# Verify Flatpak package
success, msg = verifier.verify_flatpak_signature('org.mozilla.firefox')
print(msg)

# Verify file checksum
success, msg = verifier.verify_checksum(
    '/path/to/file.deb',
    'expected_checksum_here',
    'sha256'
)
print(msg)
```

### Disabling Verification

To disable signature verification (not recommended):

```python
# In apt_backend.py, snap_backend.py, flatpak_backend.py
# Comment out verification block in install_package()

# P1 Enhancement: Verify package signature before installation
# print(f"🔐 Verifying package signature for {package_name}...")
# verified, verify_msg = self.verifier.verify_apt_package_signature(package_name)
# ...
```

## Monitoring and Maintenance

### Daily Tasks

```bash
# Check backup timer status
systemctl --user status upm-backup.timer

# View recent logs
journalctl --user -u upm-backup.service --since today
```

### Weekly Tasks

```bash
# Review GitHub Actions security scan results
# (Runs automatically every Monday 9 AM UTC)

# Update dependencies
pip list --outdated
pip install -U package_name

# Check for UPM updates
git fetch origin
git log HEAD..origin/main --oneline
```

### Monthly Tasks

```bash
# Review backup retention
ls -lh ~/.local/share/upm/backups/

# Clean up old logs (if log retention is configured)
# Logs older than 30 days are auto-deleted by default

# Review security audit
cat SECURITY_AUDIT.md

# Update documentation
# Review and update any outdated docs
```

### Automated Monitoring

Set up automated monitoring with cron:

```bash
# Add to crontab
crontab -e

# Check backup status daily at 9 AM
0 9 * * * systemctl --user status upm-backup.timer | mail -s "UPM Backup Status" you@example.com

# Check for security updates weekly
0 9 * * 1 cd ~/zorin_app_organizer && git fetch && git log HEAD..origin/main --oneline | mail -s "UPM Updates Available" you@example.com
```

## Troubleshooting

### Backup Issues

#### Problem: Backup timer not running

```bash
# Check if systemd user services are enabled
loginctl enable-linger $USER

# Reload systemd daemon
systemctl --user daemon-reload

# Restart timer
systemctl --user restart upm-backup.timer
```

#### Problem: Backup fails with permission error

```bash
# Ensure backup directory exists and is writable
mkdir -p ~/.local/share/upm/backups
chmod 755 ~/.local/share/upm/backups

# Check backup script permissions
chmod 755 /usr/local/bin/upm-backup
```

#### Problem: Cannot find backup files

```bash
# Default backup location
ls -lh ~/.local/share/upm/backups/

# Check backup configuration in backup.py
python -c "from backup import DatabaseBackup; b = DatabaseBackup(); print(b.backup_dir)"
```

### Rate Limiting Issues

#### Problem: Getting rate limited too frequently

```bash
# Increase rate limits in rate_limiter.py
# Edit the max_requests value for the specific limiter

# Or disable rate limiting temporarily
# Remove @rate_limit decorators in backend files
```

#### Problem: Rate limiter not working

```bash
# Verify rate_limiter module is imported
python -c "from rate_limiter import rate_limit; print('✅ Rate limiter imported')"

# Check for decorator in backend files
grep -r "@rate_limit" *.py
```

### Signature Verification Issues

#### Problem: All signatures failing verification

```bash
# Update GPG keys for APT
sudo apt-key update
sudo apt update

# Refresh Flatpak keys
flatpak repair

# Refresh Snap assertions
sudo snap refresh
```

#### Problem: Verification slowing down installations

```bash
# Verification is informational only and doesn't block installs
# To skip verification, remove verification code from install methods
# (not recommended)
```

### CI/CD Issues

#### Problem: Security scan workflow failing

```bash
# Check workflow syntax
cat .github/workflows/security-scan.yml | python -m yaml

# View workflow logs on GitHub
# Actions tab → Security Scan → [latest run]

# Run security tools locally to debug
bandit -r . -ll
safety check
```

#### Problem: Tests failing

```bash
# Run tests locally
pytest tests/ -v

# Run specific test file
pytest tests/test_database.py -v

# Check test coverage
pytest --cov=. --cov-report=html
```

## Performance Optimization

### Database Optimization

```python
# Use connection pooling for multi-threaded apps
# (Already implemented via thread-local storage)

# Vacuum database periodically
import sqlite3
conn = sqlite3.connect('upm.db')
conn.execute('VACUUM')
conn.close()
```

### Backup Optimization

```bash
# Compress backups to save space (enabled by default)
# Compression ratio: ~70% for SQLite databases

# Adjust backup interval in database.py
# Change self._backup_interval from 50 to higher value
```

### Rate Limiting Optimization

```python
# Use adaptive rate limiting for automatic adjustment
from rate_limiter import AdaptiveRateLimiter

limiter = AdaptiveRateLimiter(initial_max_requests=30, time_window=60)
# Automatically adjusts based on error rates
```

## Security Hardening

### File Permissions

```bash
# Restrict database access
chmod 600 upm.db

# Restrict backup directory
chmod 700 ~/.local/share/upm/backups

# Restrict config files
chmod 600 config/*
```

### Network Security

```bash
# Use HTTPS for all API calls (already implemented)
# Verify SSL certificates (already enabled in requests)

# Optional: Use firewall rules to restrict outbound connections
sudo ufw allow out to any port 443 proto tcp  # HTTPS only
```

### Dependency Security

```bash
# Pin dependency versions
pip freeze > requirements.txt

# Regularly update dependencies
pip install -U -r requirements.txt

# Use hash checking
pip install --require-hashes -r requirements.txt
```

## Rollback Procedures

### Restore from Backup

```bash
# List available backups
python -c "from backup import DatabaseBackup; b = DatabaseBackup(); import json; print(json.dumps(b.list_backups(), indent=2))"

# Restore specific backup
python -c "from database import DatabaseManager; db = DatabaseManager(); db.restore_from_backup('/path/to/backup.db.gz')"
```

### Rollback Code Changes

```bash
# View recent commits
git log --oneline -10

# Rollback to specific commit
git checkout <commit-hash>

# Or create rollback branch
git checkout -b rollback-<date>
git reset --hard <commit-hash>
```

## Additional Resources

- [P1_ENHANCEMENTS.md](P1_ENHANCEMENTS.md) - Detailed P1 feature documentation
- [GITHUB_ACTIONS_MONITORING.md](GITHUB_ACTIONS_MONITORING.md) - CI/CD monitoring guide
- [SECURITY_AUDIT.md](SECURITY_AUDIT.md) - Security audit report
- [README.md](README.md) - Project overview and quick start

## Support

For issues or questions:
1. Check this documentation
2. Review [TROUBLESHOOTING.md](TROUBLESHOOTING.md) if available
3. Create an issue on GitHub
4. Contact the development team

---

**Last Updated:** 2025-01-20
**Version:** 1.0
