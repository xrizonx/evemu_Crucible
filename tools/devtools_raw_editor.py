#!/usr/bin/env python3
"""Inspect and safely edit Python 2 marshal literals in EVEmu's devtools.raw.

Pure Python 3; does not execute the embedded client code or require Python 2.
This is a *constant editor*, not a Python-bytecode compiler/decompiler.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import sys
import tempfile
from dataclasses import dataclass, field


class FormatError(ValueError):
    """Invalid/truncated/unsupported Python 2 marshal input."""


@dataclass
class Value:
    tag: str
    text: str | None = None
    members: dict[str, "Value"] = field(default_factory=dict)


@dataclass
class StringRecord:
    path: str
    tag: str
    offset: int
    payload: bytes

    def printable(self) -> str:
        return self.payload.decode("utf-8", "backslashreplace")


class Py2Marshal:
    """Read-only structural Python 2.7 marshal parser, preserving byte offsets."""

    def __init__(self, data: bytes):
        self.data = data
        self.pos = 0
        self.interned: list[Value] = []
        self.strings: list[StringRecord] = []
        self.codes: list[dict] = []
        self.objects = 0

    def take(self, size: int) -> bytes:
        if size < 0 or self.pos + size > len(self.data):
            raise FormatError(f"Truncated marshal object at byte {self.pos} (need {size})")
        buf = self.data[self.pos:self.pos + size]
        self.pos += size
        return buf

    def i32(self) -> int:
        return struct.unpack("<i", self.take(4))[0]

    def parse(self) -> Value:
        root = self.read("$", 0)
        if self.pos != len(self.data):
            raise FormatError(f"Trailing {len(self.data) - self.pos} bytes after marshal root")
        return root

    def read(self, path: str, depth: int) -> Value:
        if depth > 120 or self.objects > 200000:
            raise FormatError("Excessive marshal nesting or object count")
        offset = self.pos
        tag = chr(self.take(1)[0])
        self.objects += 1

        if tag in "0NTFS.":
            return Value(tag)
        if tag == "i":
            self.take(4)
            return Value(tag)
        if tag == "I":
            self.take(8)
            return Value(tag)
        if tag == "l":
            count = abs(self.i32())
            if count > 10000000:
                raise FormatError("Excessive Python 2 long integer length")
            self.take(count * 2)
            return Value(tag)
        if tag in "stu":
            length = self.i32()
            payload = self.take(length)
            record = StringRecord(path, tag, offset, payload)
            self.strings.append(record)
            text = payload.decode("utf-8", "surrogateescape")
            value = Value(tag, text)
            if tag == "t":
                self.interned.append(value)
            return value
        if tag == "R":
            index = self.i32()
            if not 0 <= index < len(self.interned):
                raise FormatError(f"Invalid interned string reference {index} at {offset}")
            return Value(tag, self.interned[index].text)
        if tag in "([<>":
            count = self.i32()
            if not 0 <= count <= 200000:
                raise FormatError(f"Invalid sequence size {count} at {offset}")
            for i in range(count):
                self.read(f"{path}[{i}]", depth + 1)
            return Value(tag)
        if tag == "{":
            for i in range(200000):
                key = self.read(f"{path}.key{i}", depth + 1)
                if key.tag == "0":
                    return Value(tag)
                name = key.text if key.text is not None else f"key{i}"
                self.read(f"{path}.{name}", depth + 1)
            raise FormatError("Unterminated dict / excessive keys")
        if tag == "c":
            arguments, locals_, stack, flags = (self.i32() for _ in range(4))
            fields = {}
            for key in ("co_code", "co_consts", "co_names", "co_varnames",
                        "co_freevars", "co_cellvars", "co_filename", "co_name"):
                fields[key] = self.read(f"{path}.{key}", depth + 1)
            line = self.i32()
            fields["co_lnotab"] = self.read(f"{path}.co_lnotab", depth + 1)
            self.codes.append({
                "path": path, "name": fields["co_name"].text,
                "filename": fields["co_filename"].text,
                "line": line, "arguments": arguments, "locals": locals_,
                "stack": stack, "flags": flags,
            })
            return Value(tag, members=fields)
        if tag in "fx":
            self.take(self.take(1)[0])
            if tag == "x":
                self.take(self.take(1)[0])
            return Value(tag)
        if tag in "gy":
            self.take(8 if tag == "g" else 16)
            return Value(tag)
        raise FormatError(f"Unsupported Python 2 marshal tag 0x{ord(tag):02x} at {offset}: {path}")


@dataclass
class Scope:
    parser: Py2Marshal
    container: StringRecord | None = None


class Archive:
    def __init__(self, data: bytes):
        self.data = data
        outer = Py2Marshal(data)
        root = outer.parse()
        if root.tag != "{":
            raise FormatError("Expected a Python 2 marshal dictionary at archive root")
        self.scopes: dict[str, Scope] = {"outer": Scope(outer)}
        self._find_embedded()

    def _find_embedded(self) -> None:
        for rec in self.scopes["outer"].parser.strings:
            if ".co_consts[" not in rec.path or len(rec.payload) < 100:
                continue
            if len(rec.payload) % 2 or re.fullmatch(rb"[0-9a-fA-F]+", rec.payload) is None:
                continue
            try:
                binary = bytes.fromhex(rec.payload.decode("ascii"))
                if not binary.startswith(b"c"):
                    continue
                parser = Py2Marshal(binary)
                root = parser.parse()
                if root.tag != "c":
                    continue
                name = Path(root.members["co_filename"].text or "embedded.py").stem
                if name in self.scopes:
                    raise FormatError(f"Duplicate embedded module {name}")
                self.scopes[name] = Scope(parser, rec)
            except (ValueError, UnicodeError) as exc:
                raise FormatError(f"Invalid hex-embedded code at {rec.path}: {exc}") from exc

    def record(self, scope: str, path: str) -> StringRecord:
        try:
            records = self.scopes[scope].parser.strings
        except KeyError as exc:
            raise FormatError(f"Unknown scope {scope!r}; choose {', '.join(self.scopes)}") from exc
        found = [s for s in records if s.path == path]
        if len(found) != 1:
            raise FormatError(f"Exactly one string required at {scope}:{path}; found {len(found)}")
        return found[0]


def is_editable_literal(rec: StringRecord) -> bool:
    return (".co_consts[" in rec.path and ".co_code" not in rec.path
            and ".co_lnotab" not in rec.path)


def replace_marshaled_string(data: bytes, record: StringRecord, payload: bytes) -> bytes:
    start = record.offset
    if data[start:start + 1] != record.tag.encode("ascii"):
        raise FormatError("String tag changed while editing")
    size = struct.unpack_from("<i", data, start + 1)[0]
    if size != len(record.payload) or data[start + 5:start + 5 + size] != record.payload:
        raise FormatError("String no longer matches the inspected input")
    if len(payload) > 2**31 - 1:
        raise FormatError("Replacement exceeds Python 2 marshal size limit")
    return (data[:start] + record.tag.encode("ascii") + struct.pack("<i", len(payload))
            + payload + data[start + 5 + size:])


def set_constant(archive: Archive, scope: str, path: str, expected: str,
                 new_text: str, allow_interned: bool) -> bytes:
    rec = archive.record(scope, path)
    if not is_editable_literal(rec):
        raise FormatError("Only co_consts string literals can be modified (never co_code)")
    if rec.tag == "t" and not allow_interned:
        raise FormatError("Interned string affects every R reference; add --allow-interned after review")
    if rec.tag not in "stu":
        raise FormatError("Unsupported editable constant type")
    original_text = rec.payload.decode("utf-8", "surrogateescape")
    if original_text != expected:
        raise FormatError(f"Expected string mismatch; actual is {original_text!r}")
    if "\x00" in new_text:
        raise FormatError("NUL in a text literal is not supported")
    payload = new_text.encode("utf-8", "surrogateescape")
    if scope == "outer":
        result = replace_marshaled_string(archive.data, rec, payload)
    else:
        embedded = archive.scopes[scope]
        if embedded.container is None:
            raise FormatError("Embedded module is missing its parent container")
        child = replace_marshaled_string(embedded.parser.data, rec, payload)
        result = replace_marshaled_string(archive.data, embedded.container, child.hex().encode("ascii"))
    check = Archive(result)
    if check.record(scope, path).payload != payload:
        raise FormatError("Post-write verification failed")
    before_code = {name: [r.payload for r in value.parser.strings if r.path.endswith(".co_code")]
                   for name, value in archive.scopes.items()}
    after_code = {name: [r.payload for r in value.parser.strings if r.path.endswith(".co_code")]
                  for name, value in check.scopes.items()}
    if before_code != after_code:
        raise FormatError("Code byte stream changed: refused to write")
    return result


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def write_safe(destination: Path, data: bytes, overwrite: bool) -> None:
    if destination.exists() and not overwrite:
        raise FormatError(f"Output exists: {destination} (use --force to overwrite)")
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=destination.parent, delete=False) as handle:
            temporary = Path(handle.name)
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, destination)
    finally:
        if temporary and temporary.exists():
            temporary.unlink()


def readable(rec: StringRecord) -> str:
    try:
        return rec.payload.decode("utf-8")
    except UnicodeDecodeError:
        return rec.payload.decode("utf-8", "backslashreplace")


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("file", type=Path, help="Input devtools.raw")
    subs = ap.add_subparsers(dest="command", required=True)
    subs.add_parser("verify", help="Validate complete outer and embedded marshal records")
    ls = subs.add_parser("list", help="List available code objects and string constants")
    ls.add_argument("--scope", help="Filter to outer, insider, devToolsWindow, etc.")
    ls.add_argument("--all-strings", action="store_true", help="Also show identifiers and marshal metadata")
    ls.add_argument("--json", action="store_true", help="Output machine-readable JSON")
    exp = subs.add_parser("extract", help="Export a hex-embedded module as Python 2 raw marshal bytes")
    exp.add_argument("--scope", required=True)
    exp.add_argument("--output", type=Path, required=True)
    exp.add_argument("--force", action="store_true")
    edit = subs.add_parser("replace", help="Replace one literal, preserving Python 2 bytecode")
    edit.add_argument("--scope", required=True)
    edit.add_argument("--path", required=True, help="Exact path from 'list' output")
    edit.add_argument("--expect", help="Expected old UTF-8 text (required unless --expect-file)")
    edit.add_argument("--expect-file", type=Path, help="Read expected old text from UTF-8 file")
    val = edit.add_mutually_exclusive_group(required=True)
    val.add_argument("--value", help="Replacement UTF-8 text")
    val.add_argument("--value-file", type=Path, help="Read replacement UTF-8 text from file")
    output = edit.add_mutually_exclusive_group(required=True)
    output.add_argument("--output", type=Path)
    output.add_argument("--in-place", action="store_true", help="Back up original as .bak first")
    edit.add_argument("--allow-interned", action="store_true")
    edit.add_argument("--force", action="store_true", help="Overwrite an existing --output path")

    args = ap.parse_args(argv)
    try:
        content = args.file.read_bytes()
        archive = Archive(content)
        if args.command == "verify":
            print(f"VALID: {args.file} ({len(content)} bytes; sha256 {sha256(content)})")
            for name, scope in archive.scopes.items():
                print(f"  {name}: {len(scope.parser.codes)} code objects, "
                      f"{len(scope.parser.strings)} strings, {len(scope.parser.data)} bytes")
        elif args.command == "list":
            result = {}
            scopes = [args.scope] if args.scope else archive.scopes.keys()
            for name in scopes:
                if name not in archive.scopes:
                    raise FormatError(f"Unknown scope: {name}")
                parser = archive.scopes[name].parser
                constants = []
                for rec in parser.strings:
                    if rec.path.endswith((".co_code", ".co_lnotab")):
                        continue
                    if not args.all_strings and not is_editable_literal(rec):
                        continue
                    embedded = name == "outer" and any(
                        s.container is rec for s in archive.scopes.values())
                    if embedded and not args.all_strings:
                        continue
                    constants.append({"path": rec.path, "kind": rec.tag, "bytes": len(rec.payload),
                                      "value": readable(rec), "embedded": embedded})
                result[name] = {"code": parser.codes, "strings": constants}
            if args.json:
                print(json.dumps(result, ensure_ascii=False, indent=2))
            else:
                for name, details in result.items():
                    print(f"[{name}] {len(details['code'])} code objects")
                    for code in details["code"]:
                        print(f"  code: {code['filename']}:{code['line']} {code['name']}")
                    for s in details["strings"]:
                        snippet = repr(s["value"])
                        if len(snippet) > 155:
                            snippet = snippet[:152] + "..."
                        print(f"  {s['path']} ({s['kind']}, {s['bytes']} bytes) = {snippet}")
        elif args.command == "extract":
            if args.scope == "outer" or args.scope not in archive.scopes:
                raise FormatError("--scope must name an existing hex-embedded module")
            child = archive.scopes[args.scope].parser.data
            write_safe(args.output, child, args.force)
            print(f"Extracted {args.scope}: {args.output} ({len(child)} bytes)")
        elif args.command == "replace":
            if (args.expect is None) == (args.expect_file is None):
                raise FormatError("Specify exactly one of --expect or --expect-file")
            expected = (args.expect_file.read_text(encoding="utf-8")
                        if args.expect_file else args.expect)
            value = (args.value_file.read_text(encoding="utf-8")
                     if args.value_file else args.value)
            changed = set_constant(archive, args.scope, args.path, expected, value,
                                   args.allow_interned)
            if args.in_place:
                backup = args.file.with_name(args.file.name + ".bak")
                if backup.exists():
                    raise FormatError(f"Backup already exists: {backup}")
                shutil.copy2(args.file, backup)
                target = args.file
                overwrite = True
                print(f"Original preserved as {backup}")
            else:
                target = args.output
                overwrite = args.force
            write_safe(target, changed, overwrite)
            print(f"Wrote {target} ({len(changed)} bytes)")
            print(f"  original sha256: {sha256(content)}")
            print(f"  updated  sha256: {sha256(changed)}")
            print("  Verified: complete marshal parsed; all compiled bytecode unchanged")
        return 0
    except (OSError, UnicodeError, FormatError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
