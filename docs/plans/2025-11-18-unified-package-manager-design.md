# Unified Package Manager (UPM) - Design Document

**Date:** 2025-11-18
**Target Platform:** Zorin OS (Ubuntu-based, GNOME desktop)
**Primary User:** Power users who actively use APT, Flatpak, and Snap

---

## Overview

The Unified Package Manager (UPM) provides a single interface for managing packages across APT, Flatpak, and Snap. It solves the friction of switching between multiple package management tools while providing historical tracking and audit capabilities not available in existing tools.

### Core Value Propositions

1. **Unified Interface** - Search, install, and remove packages from all three package managers in one place
2. **Historical Tracking** - Complete audit log of all package operations with queryable history
3. **Rich Metadata** - Leverage Flathub and Snap Store APIs for screenshots, ratings, and detailed descriptions
4. **Power User Focused** - Transparent about package sources, no hidden magic, explicit control

---

## User Stories

### Primary Workflows (Equal Priority)

1. **Search & Install** - "I need X, let me find and install it" across all package managers
2. **Inventory Management** - "What's installed and where did it come from?" with historical snapshots
3. **Audit & Logs** - "What did I install last month?" with exportable logs and filtering

### User Profile

- Comfortable with all three package managers (APT, Flatpak, Snap)
- Uses each for specific purposes: APT for system packages, Flatpak for desktop apps, Snap for self-contained services
- Frequently installs packages via terminal alongside GUI tools
- Values transparency over hand-holding
- Wants historical tracking for auditing and analysis

---

## Architecture

### High-Level Design

```
┌─────────────────────────────────────────────┐
│         GUI Layer (GTK4 + Libadwaita)       │
│  Search | Installed | Logs | Settings       │
└─────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────┐
│          Business Logic Layer               │
│  Package Operations | Database Ops          │
│  API Integration | Preference Management    │
└─────────────────────────────────────────────┘
                    ↓
┌──────────────────┬──────────────┬───────────┐
│   APT Adapter    │   Flatpak    │   Snap    │
│  (python-apt)    │   Adapter    │  Adapter  │
│                  │  (CLI+API)   │ (CLI+API) │
└──────────────────┴──────────────┴───────────┘
                    ↓
┌─────────────────────────────────────────────┐
│      Data Layer (SQLite Database)           │
│  action_log | package_snapshots | prefs     │
└─────────────────────────────────────────────┘
```

### Technology Stack

**GUI Framework:** GTK4 + Libadwaita
- Native look on GNOME/Zorin OS
- Modern styling with Libadwaita
- Better desktop integration than Qt

**Core Language:** Python 3.10+

**Key Libraries:**
- `python-apt` - APT operations (no CLI parsing needed)
- `PyGObject` - GTK bindings
- `requests` - API calls to Flathub/Snap Store
- `sqlite3` - Built-in Python database

**Package Manager Integration:**
- APT: python-apt library (native, robust)
- Flatpak: CLI for operations + Flathub API for metadata
- Snap: CLI for operations + Snap Store API for metadata

---

## Database Schema

**Location:** `~/.local/share/upm/upm.db`

### Tables

#### 1. action_log
Append-only audit trail of all operations.

```sql
CREATE TABLE action_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp DATETIME NOT NULL,
    action_type TEXT NOT NULL,  -- 'install', 'remove', 'sync'
    package_name TEXT NOT NULL,
    package_manager TEXT NOT NULL,  -- 'apt', 'flatpak', 'snap'
    version TEXT,
    success BOOLEAN NOT NULL,
    error_message TEXT,
    user_note TEXT
);

CREATE INDEX idx_action_log_timestamp ON action_log(timestamp);
CREATE INDEX idx_action_log_package ON action_log(package_name);
```

#### 2. package_snapshots
Point-in-time captures of system state via user-triggered sync.

```sql
CREATE TABLE package_snapshots (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    snapshot_timestamp DATETIME NOT NULL,
    package_name TEXT NOT NULL,
    package_manager TEXT NOT NULL,
    version TEXT NOT NULL,
    install_date DATETIME  -- best-effort from package manager
);

CREATE INDEX idx_snapshots_timestamp ON package_snapshots(snapshot_timestamp);
```

#### 3. user_prefs
Settings persistence (key-value store with JSON values for flexibility).

```sql
CREATE TABLE user_prefs (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL  -- JSON-encoded
);

-- Initial values:
-- flathub_enabled: true
-- snap_enabled: true
-- ppa_enabled: true
-- show_success_notifications: true
-- show_error_notifications: true
-- log_retention_days: 30
```

#### 4. api_cache
Performance optimization for API responses.

```sql
CREATE TABLE api_cache (
    cache_key TEXT PRIMARY KEY,  -- hash of API endpoint + query
    response_data TEXT NOT NULL,  -- JSON response
    cached_at DATETIME NOT NULL,
    expires_at DATETIME NOT NULL  -- 24-hour TTL
);

CREATE INDEX idx_cache_expires ON api_cache(expires_at);
```

### Database Operations

- **Sync Strategy:** User-triggered via "Sync Now" button (not automatic)
  - Simplifies threading/race conditions
  - Power user has explicit control
  - Can add automatic sync later if needed

- **Log Retention:** Configurable (default 30 days), automatically purges old entries

- **Cache Cleanup:** On startup, delete expired API cache entries

---

## Package Manager Adapters

Each adapter implements a common interface but uses the best integration method.

### Common Interface

```python
class PackageManagerAdapter(ABC):
    @abstractmethod
    def search(self, query: str) -> List[PackageSearchResult]:
        """Search for packages matching query"""

    @abstractmethod
    def install(self, package_id: str, progress_callback=None) -> bool:
        """Install package, return success status"""

    @abstractmethod
    def remove(self, package_id: str, progress_callback=None) -> bool:
        """Remove package, return success status"""

    @abstractmethod
    def list_installed(self) -> List[PackageInfo]:
        """List all installed packages"""

    @abstractmethod
    def get_details(self, package_id: str) -> PackageDetails:
        """Get rich metadata for package"""
```

### APT Adapter (python-apt)

**Why python-apt:** Mature library, no CLI parsing, proper error handling, progress callbacks, transaction safety.

```python
search() -> Uses apt.Cache(), filters by description/name
install() -> cache[pkg].mark_install(), cache.commit()
remove() -> cache[pkg].mark_delete(), cache.commit()
list_installed() -> [pkg for pkg in cache if pkg.is_installed]
get_details() -> Rich data from python-apt (dependencies, size, description)
```

**Privilege Escalation:** python-apt handles PolicyKit authentication automatically.

### Flatpak Adapter (Hybrid: CLI + API)

**Why hybrid:** CLI is reliable for operations, Flathub API provides rich metadata (screenshots, ratings).

```python
search() -> subprocess: flatpak search --columns=name,description,application
install() -> subprocess: flatpak install -y flathub <app-id>
remove() -> subprocess: flatpak uninstall -y <app-id>
list_installed() -> subprocess: flatpak list --app --columns=application,name,version
get_details() -> Flathub API: https://flathub.org/api/v2/appstream/<app-id>
```

**API Endpoints:**
- Search metadata: `https://flathub.org/api/v2/appstream/<app-id>`
- Includes: screenshots, ratings, detailed description, homepage, license

**Privilege Escalation:** Wrap commands with `pkexec` when needed.

### Snap Adapter (Hybrid: CLI + API)

**Why hybrid:** Same reasoning as Flatpak.

```python
search() -> subprocess: snap find <query> (parse table output)
install() -> subprocess: snap install <pkg>
remove() -> subprocess: snap remove <pkg>
list_installed() -> subprocess: snap list
get_details() -> Snap API: https://api.snapcraft.io/v2/snaps/info/<name>
```

**API Endpoints:**
- Snap info: `https://api.snapcraft.io/v2/snaps/info/<snap-name>`
- Includes: screenshots, ratings, description, publisher info

**Privilege Escalation:** Wrap commands with `pkexec` when needed.

### Adapter Error Handling

- Check if package manager is installed/running on initialization
- Disable adapter if unavailable (gray out in UI)
- Capture stderr and exit codes from subprocess calls
- Translate common errors into user-friendly messages

---

## User Interface Design

### Main Window Structure

Four-tab layout using GTK Stack/StackSwitcher:

1. **Search Tab**
2. **Installed Tab**
3. **Logs Tab**
4. **Settings Tab**

### 1. Search Tab

**Layout:**
```
┌─────────────────────────────────────────────┐
│  [Search box]                      [Search] │
├─────────────────────────────────────────────┤
│  ▼ APT Results (12 found)                   │
│    • firefox - Mozilla Firefox browser      │
│      Version: 120.0  [Install] [Details]    │
│    • gimp - Image manipulation program      │
│      Version: 2.10   [Install] [Details]    │
├─────────────────────────────────────────────┤
│  ▼ Flatpak Results (5 found)                │
│    • org.mozilla.Firefox                    │
│      Firefox - Web browser                  │
│      [Install] [Details]                    │
├─────────────────────────────────────────────┤
│  ▼ Snap Results (8 found)                   │
│    • firefox - Mozilla Firefox              │
│      Version: latest/stable [Install]       │
└─────────────────────────────────────────────┘
```

**Behavior:**
- Search queries all enabled package managers simultaneously
- Results grouped by source (APT, Flatpak, Snap sections)
- Sections are collapsible
- Each result shows: icon, name, short description, version
- [Details] button opens side panel with:
  - Screenshots (from API if available)
  - Full description
  - Dependencies (APT only)
  - Install size
  - Homepage, license
  - Install button

**Why grouped:** Power users want transparency about sources. No false equivalence between `firefox` from different package managers.

### 2. Installed Tab

**Layout:**
```
┌─────────────────────────────────────────────┐
│  [Filter: _______]           [🔄 Sync Now]  │
│  Last sync: 2025-11-18 14:23:15             │
├──────────┬─────────┬─────────┬──────────────┤
│ Package  │ Source  │ Version │ Installed    │
├──────────┼─────────┼─────────┼──────────────┤
│ firefox  │ APT     │ 120.0   │ 2025-11-01   │
│ gimp     │ Flatpak │ 2.10.34 │ 2025-10-15   │
│ spotify  │ Snap    │ 1.2.26  │ 2025-11-10   │
└─────────────────────────────────────────────┘
```

**Behavior:**
- Shows all installed packages from all sources
- Unified list (source is just a column)
- Filter/search bar to narrow results
- Click row to see details, [Remove] button
- **[Sync Now]** button: queries all package managers, updates database
  - Shows progress dialog during sync
  - Updates "Last sync" timestamp
  - Logs sync operation

**Why unified:** For inventory management, source doesn't matter as much. User wants to see everything installed.

### 3. Logs Tab

**Layout:**
```
┌─────────────────────────────────────────────┐
│  Filter: [Date range] [Action] [Source]     │
│  [Export Logs]                              │
├───────────┬────────┬─────────┬────────┬─────┤
│ Timestamp │ Action │ Package │ Source │ Status│
├───────────┼────────┼─────────┼────────┼─────┤
│ Nov 18... │ Install│ firefox │ APT    │ ✓    │
│ Nov 18... │ Sync   │ -       │ All    │ ✓    │
│ Nov 17... │ Remove │ slack   │ Snap   │ ✓    │
│ Nov 17... │ Install│ gimp    │ Flatpak│ ✗    │
└─────────────────────────────────────────────┘
```

**Behavior:**
- Scrollable table of all logged actions
- Click row to see full details (error messages, notes)
- Filter by: date range, action type, package manager, success/failure
- [Export Logs] button: save as CSV or JSON
- Shows both manual operations and sync operations

**Why exportable:** Power users want to analyze data externally, generate reports, or backup audit trail.

### 4. Settings Tab

**Layout:**
```
┌─────────────────────────────────────────────┐
│  Repository Sources                         │
│  ☑ Enable Flathub                           │
│  ☑ Enable Snap Store                        │
│  ☑ Enable custom PPAs (add/manage later)    │
│                                             │
│  Notifications                              │
│  ☑ Show success notifications               │
│  ☑ Show error notifications                 │
│                                             │
│  Database                                   │
│  Log retention: [30 days ▼]                 │
│  [Clear Old Logs]                           │
└─────────────────────────────────────────────┘
```

**Behavior:**
- Toggle switches for enabling/disabling package sources
- Disabled sources are hidden from search
- Settings persist to database immediately
- Future expansion: list/manage individual PPAs, Flatpak remotes

**Why simple for MVP:** Repository management is complex. Start with on/off toggles, expand later if needed.

---

## Error Handling & Edge Cases

### Network Failures (API calls)

**Scenario:** Flathub or Snap Store API is unreachable.

**Handling:**
- Wrap all API calls in try/except with 5-second timeout
- On failure: fall back to CLI-only mode (basic info from `flatpak search`/`snap find`)
- Use cached data if available (show warning: "⚠ Using cached data")
- Show status indicator in UI: "⚠ API unavailable"

### Package Manager Unavailable

**Scenario:** Flatpak or Snap not installed, or snapd not running.

**Handling:**
- On startup, check which package managers are available:
  ```python
  apt_available = check_python_apt()
  flatpak_available = shutil.which('flatpak') is not None
  snap_available = shutil.which('snap') and snapd_running()
  ```
- Disable unavailable sources in UI (gray out, show tooltip)
- In Settings, show warning if user tries to enable unavailable source

### Installation Failures

**Common Cases:**

1. **Insufficient disk space**
   - Parse error message, extract space requirements
   - Show: "Not enough space. Need 2.5 GB, have 1.2 GB free"

2. **Package not found**
   - Show: "Package removed from repository or name changed"
   - Suggest: "Try searching for similar packages"

3. **Dependency conflicts (APT)**
   - Show conflicting packages
   - Suggest: "Run 'sudo apt --fix-broken install' in terminal"

4. **Already installed**
   - Detect from error message
   - Offer: "Package already installed. Reinstall?" button

**All failures:**
- Log to database with full error message
- Show user-friendly notification
- Provide actionable suggestions when possible

### Database Corruption

**Scenario:** SQLite database file is corrupted.

**Handling:**
```python
try:
    db.execute("SELECT 1")
except sqlite3.DatabaseError:
    backup_path = db_path + '.backup-' + timestamp
    shutil.move(db_path, backup_path)
    init_new_database()
    show_warning(f"Database reset. Backup saved to {backup_path}")
```

### Interrupted Operations

**Scenario:** Process killed during install/remove (power loss, SIGKILL).

**Handling:**
- APT: dpkg maintains state, runs recovery on next operation
- Flatpak/Snap: Operations are atomic (either complete or rollback)
- On next startup: check if package managers have pending transactions
- Show warning if incomplete transactions detected

### Sync Conflicts

**Scenario:** User installed package in terminal, but earlier install attempt through UPM failed.

**Handling:**
- Sync operation queries actual system state (source of truth)
- Database is updated to reflect reality
- Logs show both failed attempt and successful sync
- No conflict - actual system state always wins

---

## Security Considerations

### Privilege Escalation

**Principle:** Never run entire application as root.

**Implementation:**
- APT: python-apt uses PolicyKit automatically (system prompts user)
- Flatpak: Wrap commands with `pkexec flatpak install ...`
- Snap: Wrap commands with `pkexec snap install ...`
- User sees system authentication dialog, enters password per operation

### Input Validation

**Risk:** Command injection via malicious package names.

**Mitigation:**
```python
ALLOWED_PACKAGE_CHARS = re.compile(r'^[a-zA-Z0-9._-]+$')

def validate_package_name(name: str) -> bool:
    return bool(ALLOWED_PACKAGE_CHARS.match(name))
```
- Reject package names with shell metacharacters (`;`, `|`, `&`, `$`, etc.)
- Apply before passing to subprocess

### Third-Party Repository Warnings

**Risk:** Unverified packages from Flathub, PPAs.

**Mitigation:**
- On first enable of Flathub: show one-time disclaimer
  - "Flathub apps are not verified by Zorin OS. Install at your own risk."
  - User must acknowledge before enabling
- Log repository source for all installed packages (audit trail)
- Future: show package signatures, publisher info

### API Security

**Best Practices:**
- Use HTTPS for all API calls (Flathub, Snap Store use HTTPS)
- Validate JSON responses before parsing (check structure)
- Set reasonable timeouts (5s) to prevent hanging
- Don't trust API data blindly (sanitize before display)

---

## Performance Optimizations

### Lazy Loading

**Search Results:**
- Query package managers in parallel (threads)
- Show APT results first (fastest), then Flatpak/Snap as they complete
- Don't fetch API details until user clicks package
- Load icons/screenshots asynchronously

**Implementation:**
```python
# Pseudocode
def search(query):
    apt_thread = Thread(target=apt_adapter.search, args=(query,))
    flatpak_thread = Thread(target=flatpak_adapter.search, args=(query,))
    snap_thread = Thread(target=snap_adapter.search, args=(query,))

    apt_thread.start()
    flatpak_thread.start()
    snap_thread.start()

    # Update UI as results arrive
```

### API Caching

**Strategy:**
- Cache Flathub/Snap API responses for 24 hours
- Cache key: `hash(api_endpoint + query_params)`
- Store in `api_cache` table
- On cache hit: return immediately (no network call)
- On cache miss: fetch from API, store in cache

**Cache Cleanup:**
- On startup: delete expired entries (`WHERE expires_at < NOW()`)
- Vacuum database monthly

### Database Performance

**Indexes:**
```sql
CREATE INDEX idx_action_log_timestamp ON action_log(timestamp);
CREATE INDEX idx_action_log_package ON action_log(package_name);
CREATE INDEX idx_snapshots_timestamp ON package_snapshots(snapshot_timestamp);
CREATE INDEX idx_cache_expires ON api_cache(expires_at);
```

**Log Retention:**
- Configurable (default 30 days)
- Auto-purge old entries: `DELETE FROM action_log WHERE timestamp < DATE('now', '-30 days')`

### Async Operations

**Install/Remove:**
- Run in background thread to keep GUI responsive
- Show progress spinner
- Can queue multiple operations (install A, install B, remove C)
- Process queue sequentially

**Implementation:**
```python
# Pseudocode
operation_queue = Queue()

def install_package(package_id):
    operation_queue.put(('install', package_id))
    process_queue()  # runs in background thread
```

### Startup Optimization

**Goals:** Fast application launch.

**Strategy:**
- Don't sync on startup (user-triggered only)
- Defer adapter initialization until first use
- Load GUI first, initialize adapters lazily
- Defer API health checks until first search

---

## Development Plan

### Phase 1: Core Infrastructure (MVP Foundation)

**Goal:** Working CLI-based prototype with all adapters.

- [ ] Set up project structure
- [ ] Implement database schema and operations
- [ ] Build APT adapter (python-apt)
- [ ] Build Flatpak adapter (CLI only, no API yet)
- [ ] Build Snap adapter (CLI only, no API yet)
- [ ] CLI interface for testing (`upm search`, `upm install`, `upm list`)
- [ ] Unit tests for adapters
- [ ] Integration tests on test VM

**Deliverable:** Command-line UPM that can search/install/remove packages.

### Phase 2: GUI Implementation

**Goal:** Full GTK4 interface.

- [ ] Set up GTK4 + Libadwaita
- [ ] Implement Search tab
- [ ] Implement Installed tab
- [ ] Implement Logs tab
- [ ] Implement Settings tab
- [ ] Wire up adapters to GUI
- [ ] Progress indicators for operations
- [ ] Notifications for success/failure

**Deliverable:** Functional GUI application.

### Phase 3: API Integration

**Goal:** Rich metadata from Flathub and Snap Store.

- [ ] Implement Flathub API client
- [ ] Implement Snap Store API client
- [ ] Add API caching layer
- [ ] Display screenshots in package details
- [ ] Display ratings and reviews
- [ ] Error handling and fallbacks

**Deliverable:** Enhanced package details with rich metadata.

### Phase 4: Polish & Testing

**Goal:** Production-ready application.

- [ ] User testing with power users
- [ ] Performance profiling and optimization
- [ ] Comprehensive error handling
- [ ] Documentation (user guide, developer docs)
- [ ] Package for distribution (deb, AppImage, or Flatpak)
- [ ] Desktop entry and icon

**Deliverable:** Shippable application.

---

## Installation & Distribution

### Development Setup

```bash
# Clone repository
git clone <repo-url>
cd zorin_app_organizer

# Create virtual environment
python3 -m venv venv
source venv/bin/activate

# Install dependencies
pip install -r requirements.txt

# Install system dependencies (GTK4, Libadwaita)
sudo apt install libgtk-4-dev libadwaita-1-dev \
                 python3-gi python3-gi-cairo \
                 gir1.2-gtk-4.0 gir1.2-adw-1 \
                 python3-apt

# Run application
python3 -m upm.gui
```

### Distribution Options

1. **Python Package (pip)**
   - `pip install --user .`
   - Install desktop entry manually
   - Requires users to have development dependencies

2. **Debian Package (.deb)**
   - Package with `dpkg-deb` or `fpm`
   - Includes all dependencies
   - Desktop entry auto-installed
   - Best for Zorin OS users

3. **AppImage**
   - Self-contained, no installation
   - Portable across distributions
   - Larger file size

4. **Flatpak**
   - Ironic but fitting
   - Sandboxed, automatic updates
   - Needs permission to access package managers

**Recommendation:** Start with .deb package for Zorin OS users.

---

## Future Enhancements (Post-MVP)

### Near-Term

- [ ] Full PPA management (add, remove, list)
- [ ] Multiple Flatpak remote support (beyond Flathub)
- [ ] Package comparison view (compare versions across sources)
- [ ] Scheduled sync (auto-sync daily)
- [ ] Export installed package list (for reproducible setups)

### Medium-Term

- [ ] AppImage support
- [ ] Rollback functionality (reinstall previous version)
- [ ] Package update notifications
- [ ] Bulk operations (install from list file)
- [ ] Search filters (license, category, install size)

### Long-Term

- [ ] Plugin system for additional package managers
- [ ] Integration with system backup tools
- [ ] Multi-machine sync (install same packages on multiple systems)
- [ ] Web interface (manage packages remotely)

---

## Non-Goals

These are explicitly out of scope:

- System package management (kernel, drivers) - leave to system tools
- Automatic updates - user controls when to update
- Package recommendations - no AI/ML suggestions
- Snap/Flatpak replacement - coexist with existing tools
- Repository hosting - only consume existing repos

---

## Success Metrics

**How we'll know if UPM is successful:**

1. **Adoption:** Power users prefer UPM over switching between tools
2. **Reliability:** Zero data loss, accurate historical tracking
3. **Performance:** Search results in < 2 seconds, UI always responsive
4. **Trust:** Users feel confident in package operations, understand what's happening

---

## Appendix: API Reference

### Flathub API

**Base URL:** `https://flathub.org/api/v2/`

**Endpoints:**
- `appstream/<app-id>` - Full app metadata
  - Returns: JSON with description, screenshots, ratings, license, homepage

**Example:**
```bash
curl https://flathub.org/api/v2/appstream/org.mozilla.firefox
```

### Snap Store API

**Base URL:** `https://api.snapcraft.io/v2/`

**Endpoints:**
- `snaps/info/<snap-name>` - Snap metadata
  - Requires header: `Snap-Device-Series: 16`
  - Returns: JSON with description, screenshots, publisher, channels

**Example:**
```bash
curl -H 'Snap-Device-Series: 16' \
     https://api.snapcraft.io/v2/snaps/info/firefox
```

---

## Conclusion

This design provides a solid foundation for a power-user-focused unified package manager. Key architectural decisions prioritize:

- **Transparency:** Grouped search results, explicit sync control
- **Reliability:** python-apt for APT, hybrid approach for Flatpak/Snap
- **Simplicity:** User-triggered sync, minimal MVP feature set
- **Extensibility:** Clean adapter pattern, pluggable architecture

The phased development plan allows for incremental delivery with a working CLI tool in Phase 1 and full GUI by Phase 2.
