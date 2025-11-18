#!/usr/bin/env python3
"""
Setup script for Unified Package Manager (UPM)
"""

from setuptools import setup, find_packages
import os

# Read the README file
with open("README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

# Read requirements
with open("requirements.txt", "r", encoding="utf-8") as fh:
    requirements = [line.strip() for line in fh if line.strip() and not line.startswith("#")]

setup(
    name="zorin-unified-package-manager",
    version="1.0.0",
    author="Zorin App Organizer Team",
    author_email="support@zorinos.com",
    description="Unified package manager for Zorin OS supporting APT, Snap, and Flatpak",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/pitcany/zorin_app_organizer",
    project_urls={
        "Bug Tracker": "https://github.com/pitcany/zorin_app_organizer/issues",
        "Documentation": "https://github.com/pitcany/zorin_app_organizer/blob/main/README.md",
        "Source Code": "https://github.com/pitcany/zorin_app_organizer",
    },
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: End Users/Desktop",
        "Topic :: System :: Software Distribution",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Operating System :: POSIX :: Linux",
        "Environment :: X11 Applications :: Qt",
    ],
    packages=find_packages(),
    py_modules=[
        'main',
        'database',
        'logger',
        'apt_backend',
        'snap_backend',
        'flatpak_backend',
    ],
    python_requires=">=3.6",
    install_requires=requirements,
    entry_points={
        "console_scripts": [
            "upm=main:main",
            "unified-package-manager=main:main",
        ],
        "gui_scripts": [
            "upm-gui=main:main",
        ],
    },
    include_package_data=True,
    data_files=[
        # Desktop entry
        ('share/applications', ['data/upm.desktop']),
        # Icon
        ('share/icons/hicolor/scalable/apps', ['data/upm.svg']),
        # Documentation
        ('share/doc/unified-package-manager', ['README.md']),
    ],
    zip_safe=False,
    keywords=['package-manager', 'apt', 'snap', 'flatpak', 'zorin', 'ubuntu', 'gui'],
    platforms=['Linux'],
)
