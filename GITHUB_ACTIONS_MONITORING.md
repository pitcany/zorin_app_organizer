# GitHub Actions Security Monitoring Guide

This guide explains how to monitor and respond to security alerts from the automated CI/CD workflows in the Unified Package Manager project.

## Table of Contents

1. [Overview](#overview)
2. [Accessing Workflow Runs](#accessing-workflow-runs)
3. [Security Scan Workflow](#security-scan-workflow)
4. [Test Workflow](#test-workflow)
5. [Understanding Security Alerts](#understanding-security-alerts)
6. [Responding to Alerts](#responding-to-alerts)
7. [Configuring Notifications](#configuring-notifications)
8. [Best Practices](#best-practices)

## Overview

The project uses two main GitHub Actions workflows:

1. **Security Scan Workflow** (`.github/workflows/security-scan.yml`)
   - Runs on: Push, Pull Request, Weekly schedule (Monday 9 AM UTC)
   - Scans: Dependencies, code security, secrets

2. **Tests Workflow** (`.github/workflows/tests.yml`)
   - Runs on: Push, Pull Request
   - Tests: Multi-version Python compatibility, code quality

## Accessing Workflow Runs

### Via GitHub Web Interface

1. Navigate to your repository on GitHub
2. Click the **Actions** tab at the top
3. You'll see a list of all workflow runs

### Filter by Workflow

- Click on a specific workflow name in the left sidebar:
  - "Security Scan"
  - "Tests"

### View Specific Run

1. Click on any workflow run to see details
2. Each job's status is shown with ✅ (success), ❌ (failure), or ⏸️ (in progress)

## Security Scan Workflow

### Jobs Overview

The Security Scan workflow consists of 4 parallel jobs:

#### 1. Dependency Scanning

**Tools:** Safety, pip-audit

**What it checks:**
- Known security vulnerabilities in Python dependencies
- CVE (Common Vulnerabilities and Exposures) database matches
- Outdated packages with security fixes available

**Viewing results:**
```bash
# Navigate to: Actions → Security Scan → [Run] → dependency-scan
```

**Example output:**
```
✅ Safety scan completed - 0 vulnerabilities found
✅ pip-audit completed - 0 vulnerabilities found
```

**Action required if failed:**
- Review the vulnerability report
- Check CVE severity (Critical, High, Medium, Low)
- Update vulnerable packages in `requirements.txt`
- Re-run the workflow

#### 2. Code Security (Bandit)

**Tool:** Bandit

**What it checks:**
- Common security issues in Python code
- SQL injection vulnerabilities
- Command injection risks
- Hardcoded secrets
- Insecure use of crypto
- Path traversal vulnerabilities

**Viewing results:**
```bash
# Navigate to: Actions → Security Scan → [Run] → code-security
```

**Example output:**
```
Run started:2025-01-20 09:00:00

Test results:
>> Issue: [B201:flask_debug_true] Flask app with debug=True
   Severity: High
   Confidence: High
   Location: app.py:42

Total issues (by severity):
    High: 1
    Medium: 2
    Low: 10
```

**Action required if failed:**
- Review each issue carefully
- High/Critical severity issues should be fixed immediately
- Medium issues should be addressed in the next update
- Low severity issues can be evaluated for context

#### 3. Secret Detection (Gitleaks)

**Tool:** Gitleaks

**What it checks:**
- API keys in code
- Passwords in plaintext
- OAuth tokens
- Private keys
- AWS credentials
- Database connection strings

**Viewing results:**
```bash
# Navigate to: Actions → Security Scan → [Run] → secret-detection
```

**Example output:**
```
○
    │╲
    │ ○
    ○ ░
    ░    gitleaks

Finding:     SECRET_KEY = "sk-1234567890abcdef"
Secret:      sk-1234567890abcdef
RuleID:      generic-api-key
Entropy:     4.5
File:        config.py
Line:        15
Commit:      a1b2c3d4
Author:      user@example.com
Date:        2025-01-20
```

**Action required if failed:**
- **IMMEDIATELY** rotate/revoke any exposed secrets
- Remove secrets from code
- Use environment variables or secret management services
- Update `.gitleaksignore` for false positives
- Consider using `git filter-branch` to remove from history

#### 4. Advanced Security (CodeQL)

**Tool:** GitHub CodeQL

**What it checks:**
- Complex code patterns that may indicate vulnerabilities
- Data flow analysis
- Control flow analysis
- Semantic security issues

**Viewing results:**
```bash
# Navigate to: Actions → Security Scan → [Run] → codeql
# Also visible in: Security → Code scanning alerts
```

**Action required if failed:**
- Review alerts in the Security tab
- Click on each alert for detailed explanation
- Follow GitHub's remediation guidance
- Mark as false positive if applicable (with justification)

## Test Workflow

### Jobs Overview

The Tests workflow runs on Python 3.9, 3.10, and 3.11:

#### Test Job

**What it checks:**
- Unit test pass rate
- Integration test results
- Code coverage
- Code quality (flake8, black, isort)

**Viewing results:**
```bash
# Navigate to: Actions → Tests → [Run] → test-python-3.x
```

**Example output:**
```
============================= test session starts ==============================
collected 77 items

tests/test_database.py::test_connection PASSED
tests/test_database.py::test_sql_injection PASSED
...
tests/test_security.py::test_command_injection PASSED

============================== 77 passed in 4.56s ===============================

Coverage report:
Name                    Stmts   Miss  Cover
-------------------------------------------
database.py               145      5    97%
apt_backend.py           201     12    94%
flatpak_backend.py       189      8    96%
-------------------------------------------
TOTAL                    1245     42    97%
```

**Action required if failed:**
- Review failed tests
- Check error messages in logs
- Fix failing tests
- Ensure code coverage remains above 90%

## Understanding Security Alerts

### Severity Levels

#### Critical
- **Impact:** Immediate exploitation possible
- **Examples:** Remote code execution, SQL injection, authentication bypass
- **Response time:** Fix within 24 hours
- **Action:** Immediate patch required

#### High
- **Impact:** Significant security risk
- **Examples:** Cross-site scripting (XSS), privilege escalation
- **Response time:** Fix within 7 days
- **Action:** High priority fix

#### Medium
- **Impact:** Moderate security risk
- **Examples:** Information disclosure, DoS vulnerabilities
- **Response time:** Fix within 30 days
- **Action:** Include in next release

#### Low
- **Impact:** Minor security concern
- **Examples:** Verbose error messages, missing security headers
- **Response time:** Fix when convenient
- **Action:** Technical debt to address

### Common Alert Types

#### 1. Dependency Vulnerabilities

**Example:**
```
Package: requests
Installed: 2.25.0
Vulnerability: CVE-2023-32681
Severity: Medium
Fix: Upgrade to 2.31.0+
```

**Response:**
1. Update `requirements.txt`: `requests>=2.31.0`
2. Run `pip install -r requirements.txt`
3. Test application
4. Commit and push changes

#### 2. Code Security Issues

**Example:**
```
Issue: Use of exec() detected
Location: utils.py:45
Severity: High
Recommendation: Avoid using exec(); use safe alternatives
```

**Response:**
1. Review the code context
2. Replace unsafe pattern with secure alternative
3. Add test to prevent regression
4. Re-run security scan

#### 3. Secret Leaks

**Example:**
```
Secret: API_KEY exposed in commit a1b2c3d4
File: config.py
Line: 10
```

**Response:**
1. **IMMEDIATELY** revoke the exposed credential
2. Generate new credentials
3. Remove from code (use environment variables)
4. Update documentation
5. Consider rewriting git history for sensitive secrets

## Responding to Alerts

### Step-by-Step Response Process

#### 1. Assessment (5-10 minutes)
```bash
# Check all recent alerts
1. Go to: Actions tab
2. Review failed workflows
3. Prioritize by severity
4. Assess impact on production
```

#### 2. Triage (10-30 minutes)
```bash
# For each alert:
- Is it a true positive or false positive?
- What is the actual risk?
- Is it exploitable in our context?
- What is the recommended fix?
```

#### 3. Remediation (varies)
```bash
# Create fix branch
git checkout -b fix/security-alert-CVE-XXXX

# Make necessary changes
# - Update dependencies
# - Fix code issues
# - Remove secrets

# Test changes
pytest tests/
python -m bandit -r .

# Commit and push
git add .
git commit -m "fix: Address security vulnerability CVE-XXXX"
git push origin fix/security-alert-CVE-XXXX
```

#### 4. Verification (10-20 minutes)
```bash
# Create PR and verify:
1. Create pull request
2. Wait for CI/CD checks
3. Verify security scans pass
4. Review code changes
5. Merge if all checks pass
```

#### 5. Documentation (5-10 minutes)
```bash
# Update security documentation
1. Add to CHANGELOG.md
2. Update SECURITY_AUDIT.md if needed
3. Document any configuration changes
```

## Configuring Notifications

### Email Notifications

1. Go to: **Settings** → **Notifications** → **Actions**
2. Enable: "Send notifications for failed workflows"
3. Choose: "Only failures" or "All workflows"

### Slack Integration (Optional)

1. Install GitHub app in Slack workspace
2. Subscribe to repository: `/github subscribe owner/repo`
3. Configure alerts: `/github subscribe owner/repo workflows:{name:"Security Scan"}`

### GitHub Mobile App

1. Install GitHub mobile app
2. Enable push notifications for:
   - Workflow failures
   - Security alerts
   - Dependabot alerts

## Best Practices

### 1. Regular Monitoring

- **Daily:** Check for failed workflows
- **Weekly:** Review security scan results (automatic on Mondays)
- **Monthly:** Audit dependency versions and update as needed

### 2. Proactive Security

```bash
# Run security checks locally before pushing
make security-check

# Or manually:
pip-audit
bandit -r . -f json -o bandit-report.json
gitleaks detect --source . -v
```

### 3. Dependency Management

```bash
# Keep dependencies updated
pip list --outdated

# Update regularly
pip install -U package_name

# Use dependabot (already configured in .github/dependabot.yml)
```

### 4. False Positive Management

If you encounter false positives:

**For Bandit:**
```python
# Add inline comment to suppress
def safe_function():
    exec(user_input)  # nosec B102 - Input is validated by validate_input()
```

**For Gitleaks:**
```bash
# Add to .gitleaksignore
**/test_data.json
```

**For CodeQL:**
- Mark as "False positive" in Security tab
- Add justification comment

### 5. Security Posture Dashboard

Create a simple monitoring dashboard:

```bash
# Check overall security status
echo "=== Security Status ==="
echo "Last Security Scan: $(gh run list -w security-scan.yml -L 1 --json conclusion,createdAt -q '.[0]')"
echo "Last Test Run: $(gh run list -w tests.yml -L 1 --json conclusion,createdAt -q '.[0]')"
echo "Open Security Alerts: $(gh api /repos/:owner/:repo/code-scanning/alerts | jq 'length')"
```

## Troubleshooting

### Workflow Not Running

**Check:**
1. Workflow file syntax (use YAML validator)
2. Repository permissions (Actions enabled)
3. Branch protection rules

### Scans Timing Out

**Solutions:**
1. Increase timeout in workflow file
2. Use caching for dependencies
3. Split into smaller jobs

### Too Many False Positives

**Actions:**
1. Tune tool configurations
2. Update ignore files
3. Use inline suppressions with comments
4. Consider tool alternatives

## Additional Resources

- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [Bandit Documentation](https://bandit.readthedocs.io/)
- [Gitleaks Documentation](https://github.com/gitleaks/gitleaks)
- [CodeQL Documentation](https://codeql.github.com/docs/)
- [Safety Documentation](https://pyup.io/safety/)
- [pip-audit Documentation](https://github.com/pypa/pip-audit)

## Contact

For security concerns or questions:
- Create an issue in the repository
- Contact the security team
- Follow responsible disclosure practices

---

**Last Updated:** 2025-01-20
**Version:** 1.0
