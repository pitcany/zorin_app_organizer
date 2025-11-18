from setuptools import setup, find_packages

setup(
    name="upm",
    version="0.1.0",
    description="Unified Package Manager for Zorin OS",
    packages=find_packages(),
    install_requires=[
        "python-apt>=2.0.0",
    ],
    extras_require={
        "dev": [
            "pytest>=7.0.0",
            "pytest-cov>=4.0.0",
        ],
    },
    entry_points={
        "console_scripts": [
            "upm=upm.cli:main",
        ],
    },
    python_requires=">=3.10",
)
