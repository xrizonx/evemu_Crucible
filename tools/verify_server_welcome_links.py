#!/usr/bin/env python3
"""Guard the server's Local-chat welcome against stale upstream links.

This is a source-level regression check, not a simulated Crucible client.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/eve-server/chat/LSCChannel.cpp").read_text(encoding="utf-8")
begin = source.find("void LSCChannel::SendServerMOTD(Client* pClient)")
end = source.find("bool LSCChannel::IsJoined(", begin)
assert begin >= 0 and end > begin, "Server MOTD implementation was moved"
welcome = source[begin:end]

url_base = "https://github.com/xrizonx/evemu_Crucible"
urls = [
    url_base,
    url_base + "/commits/master",
    url_base + "/blob/master/PATCH_NOTES.md",
    url_base + "/actions/workflows/build-release.yml",
    url_base + "/issues",
]
for url in urls:
    assert url in welcome, f"Missing current GitHub link in login welcome: {url}"
for label in ("Welcome to EVEmu Crucible - Rizonx.", "Server Version:", "Revision Date:",
              "Uptime:", "Current Population:", "Character Options:"):
    assert label in welcome, f"Lost login information: {label}"
assert "evemu.dev" not in welcome and "discord.gg" not in welcome, "Stale external link in login welcome"
assert "httpGet" not in welcome and "curl" not in welcome, "Login must remain offline"
print("PASS: Local chat welcome links to live master changes, patch notes, builds and issues.")
