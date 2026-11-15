#!/usr/bin/env python3
"""Import locally available, appropriately licensed NPC agent JPEG portraits.

No network calls, no image downloads, no dependency beyond Python 3.
For missing portraits, the image server itself supplies its bundled offline
placeholder. This script only installs real files to avoid overwriting them
with copies of that generic stand-in.
"""
import argparse
import pathlib
import re
import shutil
import sys

MIN_AGENT_ID = 3_000_000
MAX_AGENT_ID = 4_000_000
FILENAME = re.compile(r"^(\d+)(?:_(50|512))?\.jpe?g$", re.IGNORECASE)


def read_agent_ids(path):
    ids = set()
    if path is None:
        return None
    for number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#") or line.lower() in ("agentid", "agent_id"):
            continue
        value = line.split(",", 1)[0].strip()
        try:
            agent_id = int(value)
        except ValueError as exc:
            raise ValueError(f"{path}:{number}: invalid agent ID {value!r}") from exc
        if not MIN_AGENT_ID <= agent_id < MAX_AGENT_ID:
            raise ValueError(f"{path}:{number}: ID {agent_id} is not a Crucible NPC agent")
        ids.add(agent_id)
    return ids


def valid_jpeg(path):
    if not path.is_file() or path.stat().st_size < 4:
        return False
    with path.open("rb") as handle:
        return handle.read(2) == b"\xff\xd8" and (
            handle.seek(-2, 2) is None or handle.read(2) == b"\xff\xd9"
        )


def import_portraits(source, output, allowed=None, overwrite=False, dry_run=False):
    if not source.is_dir():
        raise ValueError(f"portrait source directory not found: {source}")
    if source.resolve() == output.resolve():
        raise ValueError("source and output must be different directories")

    found = installed = skipped = 0
    for file in sorted(source.iterdir()):
        match = FILENAME.match(file.name)
        if match is None or not file.is_file():
            continue
        agent_id = int(match.group(1))
        if not MIN_AGENT_ID <= agent_id < MAX_AGENT_ID:
            continue
        if allowed is not None and agent_id not in allowed:
            continue
        if not valid_jpeg(file):
            print(f"SKIP invalid JPEG: {file}", file=sys.stderr)
            skipped += 1
            continue
        found += 1
        size = match.group(2) or "512"
        destination = output / f"{agent_id}_{size}.jpg"
        if destination.exists() and not overwrite:
            skipped += 1
            continue
        if not dry_run:
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(file, destination)
        installed += 1
        print(f"{'WOULD INSTALL' if dry_run else 'INSTALLED'} {destination}")
    print(f"Agent JPEGs found: {found}, imported: {installed}, skipped: {skipped}")
    if allowed is not None:
        print(f"Agent IDs requested: {len(allowed)}; missing originals use the bundled offline placeholder.")
    return installed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=pathlib.Path, required=True,
                        help="Local directory containing licensed agent JPG files")
    parser.add_argument("--output", type=pathlib.Path, default=pathlib.Path("image_cache/Character"),
                        help="Configured imageDir/Character directory")
    parser.add_argument("--ids", type=pathlib.Path,
                        help="Optional text/CSV file of station agent IDs")
    parser.add_argument("--overwrite", action="store_true",
                        help="Replace existing local originals; otherwise leave them intact")
    parser.add_argument("--dry-run", action="store_true",
                        help="Preview which local images would be copied")
    args = parser.parse_args()
    try:
        allowed = read_agent_ids(args.ids)
        import_portraits(args.source, args.output, allowed, args.overwrite, args.dry_run)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))


if __name__ == "__main__":
    main()
