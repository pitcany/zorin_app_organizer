PolySynaptic
============

PolySynaptic is a graphical package management tool based on GTK and APT.
PolySynaptic enables you to install, upgrade and remove software packages in
a user-friendly way.

PolySynaptic is a fork of [Synaptic](https://github.com/mvo5/synaptic).

Besides these basic functions the following features are provided:
 * Search and filter the list of available packages
 * Perform smart system upgrades
 * Fix broken package dependencies
 * Edit the list of used repositories (sources.list)
 * Download the latest changelog of a package
 * Configure packages through the debconf system
 * Browse all available documentation related to a package (dwww is required)

Usage
---------
By default polysynaptic uses pkexec to obtain root privileges needed.

```bash
# Run with pkexec (recommended)
polysynaptic-pkexec

# Or with sudo
sudo polysynaptic
```

PolySynaptic is used very much like apt/apt-get. Aside from a graphical interface, another key difference is it lets you stage several changes and only applies package changes when you click apply.

A simple upgrade
```
sudo apt update
sudo apt upgrade
Review changes and press yes
```

Do an upgrade in polysynaptic:
 1. Click Reload
 2. Note how Installed (upgradeable) appears in the Filter list
 3. Click Mark All Upgrades
 4. Mark additional Required Changes may appear
 5. Click Mark
 6. Click Apply
 7. Summary appears, Click Apply.

Filters
--------
PolySynaptic displays the main package list according to the filter you selected. The most simple filter is of course "All packages". But there are much more filters than that :) You can view the predefined filters and make your own filters by clicking on "Filters" above the main package list.

Selecting Multiple Packages
----------------------------
You can select more than one package at a time. You have to use Shift or Ctrl to select multiple packages. If you click on an action (install/upgrade/remove) for multiple packages, the action will be performed for each package.

Multiple Sources
----------------
On a Debian system, you can have more than one "release" in your sources.list file. You can choose which one to use in the "Distribution" tab in the preferences dialog.

Keybindings
------------
Global keybindings:
* Alt-K  keep
* Alt-I  install
* Alt-R  remove
* Alt-U  Update individual package
* Alt-L  Update Package List
* Alt-G  upgrade
* Alt-D  DistUpgrade
* Alt-P  proceed
* Ctrl-F find

Command-line options
---------------------
PolySynaptic supports the following command-line options:
 '-f <filename>' or "--filter-file <filename>" = give an alternative filter file
 '-i <int>' or "--initial-filter <int>" = start with filter nr. <int>
 '-r' = open repository screen on startup
 '-o <option>' or "--option <option>" = set a polysynaptic/apt option (expert only)
 '--set-selections' = feed packages inside polysynaptic (format is like
                      dpkg --get-selections)
 '--non-interactive' = non-interactive mode (this will also prevent saving
                       of configuration options)

Credits
-------
Synaptic was developed by Alfredo K. Kojima from Connectiva. Michael Vogt
completed the port to GTK and added many new features.
