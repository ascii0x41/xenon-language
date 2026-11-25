
#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
import platform
import re
import shutil
import sys
from pathlib import Path


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
# Environment cleanup
# ==================================================

def remove_windows_environment():
    import winreg

    with winreg.OpenKey(
        winreg.HKEY_CURRENT_USER,
        "Environment",
        0,
        winreg.KEY_READ | winreg.KEY_WRITE
    ) as key:

        try:
            winreg.DeleteValue(key, "XENON_HOME")
        except FileNotFoundError:
            pass

        try:
            path, value_type = winreg.QueryValueEx(key, "Path")
        except FileNotFoundError:
            return

        # Remove only Xenon's bin entry.
        entries = [
            entry.strip()
            for entry in path.split(";")
            if entry.strip()
        ]

        entries = [
            entry for entry in entries
            if Path(entry).name.lower() != "bin"
            or Path(entry).parent.name.lower() != "xenon"
        ]

        winreg.SetValueEx(
            key,
            "Path",
            0,
            value_type,
            ";".join(entries)
        )

    print("  Removed Windows environment variables.")


def remove_unix_environment():
    shell = Path(os.environ.get("SHELL", "/bin/bash")).name

    if shell == "fish":
        config = Path.home() / ".config" / "fish" / "config.fish"
    else:
        config_name = ".zshrc" if shell == "zsh" else ".bashrc"
        config = Path.home() / config_name

    if not config.exists():
        return

    content = config.read_text(encoding="utf-8")

    start_marker = "# >>> Xenon configuration >>>"
    end_marker = "# <<< Xenon configuration <<<"

    pattern = re.compile(
        re.escape(start_marker) + r".*?" +
        re.escape(end_marker) + r"\n?",
        re.DOTALL
    )

    updated = pattern.sub("", content)

    if updated != content:
        config.write_text(updated, encoding="utf-8")
        print(f"  Updated shell configuration: {config}")


def remove_environment(system):
    print("\nRemoving environment configuration...")

    if system == "Windows":
        remove_windows_environment()
    else:
        remove_unix_environment()


# ==================================================
# Installation removal
# ==================================================

def remove_installation(install_dir, complete):
    print("\nRemoving Xenon installation...")

    if not install_dir.exists():
        print("  Installation directory does not exist.")
        return

    if complete:
        shutil.rmtree(install_dir)
        print(f"  Removed entire installation: {install_dir}")
        return

    # Remove compiler executable.
    bin_dir = install_dir / "bin"

    if bin_dir.exists():
        for name in ("xec", "xec.exe"):
            executable = bin_dir / name

            if executable.exists():
                executable.unlink()
                print(f"  Removed {executable}")

        # Remove bin only if empty.
        try:
            bin_dir.rmdir()
        except OSError:
            pass

    # Remove configuration.
    config_dir = install_dir / "config"

    if config_dir.exists():
        shutil.rmtree(config_dir)
        print("  Removed Xenon configuration.")

    # Preserve user environments and standard library.
    print("  Preserved standard library.")
    print("  Preserved user environments.")

    # Remove installation directory only if empty.
    try:
        install_dir.rmdir()
    except OSError:
        pass


# ==================================================
# Confirmation
# ==================================================

def confirm_uninstall(complete):
    if complete:
        print("\nWARNING: Complete uninstall selected.")
        print("This will permanently delete:")
        print("  - Xenon compiler")
        print("  - Standard library")
        print("  - All environments")
        print("  - Configuration")
        print("  - Other files inside the installation directory")
    else:
        print("\nXenon will be uninstalled.")
        print("Your standard library and environments will be preserved.")

    answer = input("\nContinue? [y/N]: ").strip().lower()

    return answer in ("y", "yes")


# ==================================================
# Main
# ==================================================

def main():
    parser = argparse.ArgumentParser(
        description="Uninstall the Xenon compiler."
    )

    parser.add_argument(
        "--prefix",
        type=Path,
        help="Custom Xenon installation directory."
    )

    parser.add_argument(
        "--complete",
        action="store_true",
        help="Remove all Xenon files, including environments."
    )

    parser.add_argument(
        "--yes",
        action="store_true",
        help="Skip confirmation prompts."
    )

    args = parser.parse_args()

    try:
        system = detect_platform()

        install_dir = (
            args.prefix.expanduser().resolve()
            if args.prefix
            else default_install_dir(system)
        )

        print("================================")
        print("       Xenon Uninstaller")
        print("================================")

        print(f"\nOperating system: {system}")
        print(f"Installation: {install_dir}")

        if not install_dir.exists():
            print("\nXenon does not appear to be installed here.")
            return

        if not args.yes and not confirm_uninstall(args.complete):
            print("\nUninstallation cancelled.")
            return

        remove_environment(system)
        remove_installation(install_dir, args.complete)

        print("\nXenon was successfully uninstalled.")

        print(
            "\nRestart your terminal for environment changes "
            "to take effect."
        )

    except (OSError, RuntimeError) as error:
        print(f"\nUninstallation failed: {error}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
