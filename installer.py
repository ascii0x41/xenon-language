
#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent
BUILD_DIR = PROJECT_ROOT / "build-installer"

PROJECT_NAME = "Xenon"
MIN_CMAKE_VERSION = (3, 20)


# ==================================================
# Platform detection
# ==================================================

def detect_platform():
    system = platform.system()

    if system not in ("Windows", "Linux", "Darwin"):
        raise RuntimeError(f"Unsupported operating system: {system}")

    return system


def default_install_dir(system):
    home = Path.home()

    if system == "Windows":
        return Path(os.environ.get("LOCALAPPDATA", home)) / "Xenon"

    if system == "Darwin":
        return home / "Library" / "Application Support" / "Xenon"

    return home / ".local" / "share" / "xenon"


# ==================================================
# Utilities
# ==================================================

def run(command, **kwargs):
    print(f"\n> {' '.join(map(str, command))}")

    subprocess.run(
        command,
        check=True,
        **kwargs
    )


def find_executable(*names):
    for name in names:
        path = shutil.which(name)

        if path:
            return path

    return None


# ==================================================
# Dependency checks
# ==================================================

def check_dependencies(system):
    print("\nChecking dependencies...")

    cmake = find_executable("cmake")

    if not cmake:
        raise RuntimeError(
            "CMake was not found. Install CMake 3.20 or newer."
        )

    result = subprocess.run(
        [cmake, "--version"],
        capture_output=True,
        text=True,
        check=True
    )

    match = re.search(r"cmake version (\d+)\.(\d+)", result.stdout)

    if not match:
        raise RuntimeError("Could not determine the CMake version.")

    version = (int(match.group(1)), int(match.group(2)))

    if version < MIN_CMAKE_VERSION:
        raise RuntimeError(
            f"CMake 3.20+ is required. Found {version[0]}.{version[1]}."
        )

    print(f"  CMake: {version[0]}.{version[1]}")

    # CMake performs the definitive compiler detection during
    # configuration. This check gives a more helpful early error.
    if system == "Windows":
        compiler = find_executable("cl", "clang-cl", "g++", "clang++")
    else:
        compiler = find_executable("c++", "g++", "clang++")

    if not compiler:
        raise RuntimeError(
            "No C++ compiler was found. Install GCC, Clang, "
            "or MSVC and ensure it is available in your environment."
        )

    print(f"  C++ compiler: {compiler}")

    return cmake


# ==================================================
# Build
# ==================================================

def configure_build(cmake, install_dir):
    print("\nConfiguring Xenon...")

    BUILD_DIR.mkdir(parents=True, exist_ok=True)

    run([
        cmake,
        "-S", str(PROJECT_ROOT),
        "-B", str(BUILD_DIR),
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_INSTALL_PREFIX=" + str(install_dir),
        "-DXENON_BUILD_TOOLS=OFF",
    ])


def build_xenon(cmake):
    print("\nBuilding Xenon...")

    run([
        cmake,
        "--build", str(BUILD_DIR),
        "--config", "Release",
        "--target", "xec",
        "--parallel",
    ])


def install_xenon(cmake):
    print("\nInstalling Xenon...")

    run([
        cmake,
        "--install", str(BUILD_DIR),
        "--config", "Release",
    ])


# ==================================================
# Runtime files
# ==================================================

def install_runtime_files(install_dir):
    print("\nInstalling runtime files...")

    install_dir.mkdir(parents=True, exist_ok=True)

    # Standard library is updated on every installation.
    stdlib_src = PROJECT_ROOT / "stdlib"
    stdlib_dst = install_dir / "stdlib"

    if stdlib_src.is_dir():
        shutil.copytree(
            stdlib_src,
            stdlib_dst,
            dirs_exist_ok=True
        )
        print(f"  Installed standard library: {stdlib_dst}")
    else:
        print("  Warning: stdlib directory not found.")

    # Environments are initialized only once.
    # Never overwrite environments created by the user.
    environments_src = PROJECT_ROOT / "environments"
    environments_dst = install_dir / "environments"

    if environments_src.is_dir():
        if not environments_dst.exists():
            shutil.copytree(
                environments_src,
                environments_dst
            )
            print(f"  Initialized environments: {environments_dst}")
        else:
            print("  Existing environments preserved.")
    else:
        environments_dst.mkdir(parents=True, exist_ok=True)
        print("  Created empty environments directory.")

    (install_dir / "config").mkdir(exist_ok=True)


# ==================================================
# Environment configuration
# ==================================================

def configure_windows_environment(install_dir):
    import winreg

    bin_dir = str(install_dir / "bin")

    key_path = r"Environment"

    with winreg.OpenKey(
        winreg.HKEY_CURRENT_USER,
        key_path,
        0,
        winreg.KEY_READ | winreg.KEY_WRITE
    ) as key:

        winreg.SetValueEx(
            key,
            "XENON_HOME",
            0,
            winreg.REG_EXPAND_SZ,
            str(install_dir)
        )

        try:
            path, value_type = winreg.QueryValueEx(key, "Path")
        except FileNotFoundError:
            path, value_type = "", winreg.REG_EXPAND_SZ

        entries = [
            entry.strip()
            for entry in path.split(";")
            if entry.strip()
        ]

        if bin_dir.lower() not in [entry.lower() for entry in entries]:
            entries.append(bin_dir)

        winreg.SetValueEx(
            key,
            "Path",
            0,
            value_type,
            ";".join(entries)
        )

    print("  Updated user environment variables.")


def configure_unix_environment(install_dir, system):
    shell = Path(os.environ.get("SHELL", "/bin/bash")).name

    if shell == "fish":
        config = Path.home() / ".config" / "fish" / "config.fish"

        block = (
            "\n# >>> Xenon configuration >>>\n"
            f"set -gx XENON_HOME '{install_dir}'\n"
            f"fish_add_path '{install_dir / 'bin'}'\n"
            "# <<< Xenon configuration <<<\n"
        )

    else:
        config_name = ".zshrc" if shell == "zsh" else ".bashrc"
        config = Path.home() / config_name

        block = (
            "\n# >>> Xenon configuration >>>\n"
            f'export XENON_HOME="{install_dir}"\n'
            f'export PATH="{install_dir / "bin"}:$PATH"\n'
            "# <<< Xenon configuration <<<\n"
        )

    config.parent.mkdir(parents=True, exist_ok=True)

    existing = config.read_text(encoding="utf-8") if config.exists() else ""

    start_marker = "# >>> Xenon configuration >>>"
    end_marker = "# <<< Xenon configuration <<<"

    if start_marker in existing and end_marker in existing:
        pattern = re.compile(
            re.escape(start_marker) + r".*?" +
            re.escape(end_marker),
            re.DOTALL
        )

        updated = pattern.sub(block.strip(), existing)
    else:
        updated = existing.rstrip() + "\n" + block

    config.write_text(updated, encoding="utf-8")

    print(f"  Updated shell configuration: {config}")


def configure_environment(install_dir, system):
    print("\nConfiguring environment...")

    if system == "Windows":
        configure_windows_environment(install_dir)
    else:
        configure_unix_environment(install_dir, system)

    # Also make the variables available to this installer process.
    os.environ["XENON_HOME"] = str(install_dir)
    os.environ["PATH"] = (
        str(install_dir / "bin") +
        os.pathsep +
        os.environ.get("PATH", "")
    )


# ==================================================
# Verification
# ==================================================

def verify_installation(install_dir):
    print("\nVerifying installation...")

    executable_name = "xec.exe" if os.name == "nt" else "xec"
    executable = install_dir / "bin" / executable_name

    if not executable.is_file():
        raise RuntimeError(
            f"Compiler executable was not found: {executable}"
        )

    run([str(executable), "--version"])

    print("\nXenon was successfully installed!")


# ==================================================
# Main
# ==================================================

def main():
    parser = argparse.ArgumentParser(
        description="Build and install the Xenon compiler."
    )

    parser.add_argument(
        "--prefix",
        type=Path,
        help="Custom Xenon installation directory."
    )

    args = parser.parse_args()

    try:
        print("================================")
        print("       Xenon Installer")
        print("================================")

        system = detect_platform()

        print(f"\nOperating system: {system}")
        print(f"Architecture: {platform.machine()}")

        install_dir = (
            args.prefix.expanduser().resolve()
            if args.prefix
            else default_install_dir(system)
        )

        print(f"Installation directory: {install_dir}")

        cmake = check_dependencies(system)

        configure_build(cmake, install_dir)
        build_xenon(cmake)
        install_xenon(cmake)

        install_runtime_files(install_dir)
        configure_environment(install_dir, system)

        verify_installation(install_dir)

        print("\nRestart your terminal to use Xenon.")
        print(f"XENON_HOME: {install_dir}")

    except (RuntimeError, subprocess.CalledProcessError, OSError) as error:
        print(f"\nInstallation failed: {error}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
