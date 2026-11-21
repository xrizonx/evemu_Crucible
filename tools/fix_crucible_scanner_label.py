#!/usr/bin/env python3
"""Repair the asteroid localization call in matching Crucible scanner source."""
import argparse
from pathlib import Path
import re


ASTEROID_LABEL = rb"""(?P<prefix>^[ \t]*entryname\s*=\s*localization\.GetByLabel\(\s*(['"])UI/Inventory/SlimItemNames/SlimAsteroid\2\s*,\s*)(?P<keyword>asteroidType\s*=\s*)?(?P<value>typeinfo\.name\s*\))"""
ASTEROID_CALL = re.compile(ASTEROID_LABEL, re.MULTILINE)


def repair(source: bytes) -> bytes:
    matches = list(ASTEROID_CALL.finditer(source))
    if len(matches) != 1:
        raise ValueError("Expected exactly one recognized scanner asteroid localization call.")
    match = matches[0]
    if match.group("keyword"):
        return source
    start = match.start("value")
    return source[:start] + b"asteroidType=" + source[start:]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="scanner.py in your offline client source")
    parser.add_argument("--apply", action="store_true", help="write the change and keep a .bak copy")
    args = parser.parse_args()
    if args.source.name != "scanner.py":
        parser.error("Select a scanner.py source file.")
    try:
        source = args.source.read_bytes()
        updated = repair(source)
        if source == updated:
            print("The recognized localization call already supplies asteroidType.")
        elif args.apply:
            backup = args.source.with_suffix(".py.bak")
            with backup.open("xb") as copy:
                copy.write(source)
            args.source.write_bytes(updated)
            print(f"Patched {args.source}. Backup saved as {backup}.")
        else:
            print("Matched the positional asteroid argument. Pass --apply to patch this source.")
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
