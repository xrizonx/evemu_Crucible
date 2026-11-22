#!/usr/bin/env python3
"""Offline tests for the Python 2 devtools.raw reader and constant editor."""
import io
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from contextlib import redirect_stdout

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from devtools_raw_editor import Archive, FormatError, main, set_constant


def s(tag: bytes, payload: bytes) -> bytes:
    return tag + struct.pack('<i', len(payload)) + payload


def seq(*items: bytes) -> bytes:
    return b'(' + struct.pack('<i', len(items)) + b''.join(items)


def code(filename: bytes, name: bytes, constants: list[bytes]) -> bytes:
    data = b'c' + struct.pack('<iiii', 0, 0, 1, 64)
    data += s(b's', b'\x64\x00\x00\x53')
    data += seq(*constants)
    for unused in range(4):
        data += seq()
    return data + s(b's', filename) + s(b's', name) + struct.pack('<i', 1) + s(b's', b'\x00\x01')


def fixture() -> bytes:
    nested = code(b'devToolsWindow.py', b'<module>', [s(b's', b'Dungeon Editor'), s(b't', b'click')])
    wrapper = code(b'devtools.py', b'Bootstrap', [s(b't', nested.hex().encode()), s(b's', b'def evemuLoad():\n\tpass')])
    return b'{' + s(b't', b'Bootstrap') + seq(wrapper) + b'0'


class MarshalEditorTests(unittest.TestCase):
    def setUp(self):
        self.data = fixture()
        self.archive = Archive(self.data)

    def test_recognizes_modules(self):
        self.assertEqual(set(self.archive.scopes), {'outer', 'devToolsWindow'})
        self.assertEqual(len(self.archive.scopes['outer'].parser.codes), 1)
        self.assertEqual(len(self.archive.scopes['devToolsWindow'].parser.codes), 1)

    def test_round_trip_nested_string_replacement_preserves_code(self):
        path = '$.co_consts[0]'
        data = set_constant(self.archive, 'devToolsWindow', path, 'Dungeon Editor', 'Edited Dungeon Tools', False)
        other = Archive(data)
        self.assertEqual(other.record('devToolsWindow', path).printable(), 'Edited Dungeon Tools')
        self.assertEqual(other.scopes['devToolsWindow'].parser.codes, self.archive.scopes['devToolsWindow'].parser.codes)
        self.assertEqual(other.record('outer', '$.Bootstrap[0].co_consts[1]').printable(), 'def evemuLoad():\n\tpass')
        self.assertEqual(self.archive.record('devToolsWindow', path).printable(), 'Dungeon Editor')

    def test_outer_source_edit(self):
        data = set_constant(self.archive, 'outer', '$.Bootstrap[0].co_consts[1]',
                            'def evemuLoad():\n\tpass', 'def evemuLoad():\n\treturn', False)
        self.assertEqual(Archive(data).record('outer', '$.Bootstrap[0].co_consts[1]').printable(),
                         'def evemuLoad():\n\treturn')

    def test_rejects_bytecode_and_interned_by_default(self):
        with self.assertRaises(FormatError):
            set_constant(self.archive, 'outer', '$.Bootstrap[0].co_code', '', '', False)
        with self.assertRaises(FormatError):
            set_constant(self.archive, 'devToolsWindow', '$.co_consts[1]', 'click', 'open', False)
        data = set_constant(self.archive, 'devToolsWindow', '$.co_consts[1]', 'click', 'open', True)
        self.assertEqual(Archive(data).record('devToolsWindow', '$.co_consts[1]').printable(), 'open')

    def test_mismatched_expectation_and_invalid_bytes(self):
        with self.assertRaises(FormatError):
            set_constant(self.archive, 'devToolsWindow', '$.co_consts[0]', 'wrong', 'right', False)
        with self.assertRaises(FormatError):
            Archive(self.data[:-1])
        with self.assertRaises(FormatError):
            Archive(b'{' + s(b't', b'name') + b'\xff' + b'0')

    def test_cli_verify_replace_and_extract(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'devtools.raw'
            target = Path(directory) / 'devtools.edited.raw'
            module = Path(directory) / 'devToolsWindow.marshal'
            source.write_bytes(self.data)
            with redirect_stdout(io.StringIO()):
                self.assertEqual(main([str(source), 'verify']), 0)
                self.assertEqual(main([str(source), 'replace', '--scope', 'devToolsWindow',
                                       '--path', '$.co_consts[0]', '--expect', 'Dungeon Editor',
                                       '--value', 'New Editor', '--output', str(target)]), 0)
                self.assertEqual(main([str(target), 'extract', '--scope', 'devToolsWindow',
                                       '--output', str(module)]), 0)
            self.assertEqual(Archive(target.read_bytes()).record('devToolsWindow', '$.co_consts[0]').printable(), 'New Editor')
            self.assertEqual(module.read_bytes()[0:1], b'c')
            self.assertEqual(source.read_bytes(), self.data)


if __name__ == '__main__':
    unittest.main()
