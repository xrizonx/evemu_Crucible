# Configuration Files

This directory contains default configuration assets copied to the server's `etc/` directory during installation.

## devtools.raw

`devtools.raw` is a binary Python 2 marshal archive containing compiled client DevTools code (Bootstrap and hex-embedded Insider/DevTools UI modules). It is **not** a normal text file.

Use the cross-platform, Python 3 standard-library [DevTools RAW editor](../../tools/devtools_raw_editor.py) to verify, inspect, extract embedded Python 2 marshal modules, or replace selected string constants while preserving compiled instructions.

See [Windows, Linux and Debian Docker usage instructions](../../docs/DEVTOOLS_RAW_EDITOR.md). The Debian Docker Compose deployment uses the bind-mounted file at `./config/devtools.raw`, not this template once the destination exists. Keep a backup before editing or deploying.
