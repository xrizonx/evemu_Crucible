#!/usr/bin/env python3
"""Offline regression tests for the portrait import tool (stdlib only)."""
import base64
import pathlib
import re
import tempfile
import unittest

from import_local import import_portraits, read_agent_ids, valid_jpeg


class AgentPortraitImportTest(unittest.TestCase):
    def test_compiled_fallback_is_valid_local_jpeg(self):
        cpp = pathlib.Path(__file__).resolve().parents[2] / (
            "src/eve-server/imageserver/ImageServer.cpp"
        )
        text = cpp.read_text(encoding="utf-8")
        match = re.search(r'encoded\s*=\s*"([A-Za-z0-9+/=]+)"', text)
        self.assertIsNotNone(match)
        jpeg = base64.b64decode(match.group(1), validate=True)
        self.assertGreater(len(jpeg), 500)
        self.assertEqual(jpeg[:2], b"\xff\xd8")
        self.assertEqual(jpeg[-2:], b"\xff\xd9")
        self.assertIn(b"JFIF", jpeg[:32])

    def test_import_preserves_original_and_ignores_player_ids(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            source = root / "originals"
            source.mkdir()
            dest = root / "image_cache" / "Character"
            # A minimal JPEG with valid SOI/EOI markers. The import validates
            # file framing; the browser and the actual image server decode
            # should be verified separately with real JPEGs.
            agent = source / "3000001_50.jpg"
            agent.write_bytes(b"\xff\xd8\xff\xd9")
            (source / "90000001_50.jpg").write_bytes(b"\xff\xd8\xff\xd9")
            self.assertTrue(valid_jpeg(agent))
            self.assertEqual(import_portraits(source, dest), 1)
            self.assertEqual((dest / "3000001_50.jpg").read_bytes(), agent.read_bytes())
            self.assertFalse((dest / "90000001_50.jpg").exists())
            agent.write_bytes(b"\xff\xd8New\xff\xd9")
            self.assertEqual(import_portraits(source, dest), 0)
            self.assertEqual((dest / "3000001_50.jpg").read_bytes(), b"\xff\xd8\xff\xd9")

    def test_ids_filter_and_invalid_jpeg(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            source = root / "originals"
            source.mkdir()
            (source / "3000002.jpg").write_bytes(b"\xff\xd8Valid\xff\xd9")
            (source / "3000003.jpg").write_bytes(b"bad-jpeg")
            ids = root / "ids.txt"
            ids.write_text("agentID\n3000002\n3000003\n", encoding="utf-8")
            self.assertEqual(read_agent_ids(ids), {3000002, 3000003})
            output = root / "cache"
            self.assertEqual(import_portraits(source, output, read_agent_ids(ids)), 1)
            self.assertTrue((output / "3000002_512.jpg").is_file())
            self.assertFalse((output / "3000003_512.jpg").exists())

    def test_rejects_non_agents_in_id_list(self):
        with tempfile.TemporaryDirectory() as temp:
            ids = pathlib.Path(temp) / "ids.txt"
            ids.write_text("90000001\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                read_agent_ids(ids)


if __name__ == "__main__":
    unittest.main()
