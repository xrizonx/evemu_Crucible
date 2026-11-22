# Editing EVEmu Crucible devtools.raw on Windows and Linux

**Cross-platform tool:** [tools/devtools_raw_editor.py](../tools/devtools_raw_editor.py), Python 3.9+ standard library only. It works offline and does not require GitHub Actions, Python 2, CMake, or Docker.

## What this file actually contains

The 10,117-byte repository sample at [utils/config/devtools.raw](../utils/config/devtools.raw) is **not plain text**. It is a **Python 2.x marshal dictionary** with a `Bootstrap` compiled-code entry. Several constants in that code contain **hex-encoded additional Python 2 marshal code objects**. The actual source inspection shows:

| Tool scope | Compiled filename | Functions/classes and code objects |
|---|---|---|
| `outer` | `devtools.py` | `Bootstrap`, 1 code object |
| `insider` | `insider.py` | `insider`, `OnSessionChanged`, `Show`, 5 code objects |
| `devToolsWindow` | `devToolsWindow.py` | `DevWindow`, `ApplyAttributes`, UI/Dungeon/Camera button callbacks, 6 code objects |
| `devToolsConsole` | `devToolsConsole.py` | `ConsoleWindow`, `write`, `ApplyAttributes`, `EnterPressed`, 5 code objects |

The server method `DevToolsProviderService::GetLoader` in `src/eve-server/admin/DevToolsProviderService.cpp` reads **`EVEMU_ROOT/etc/devtools.raw` as binary** and returns those bytes to the EVE client. The client interprets/loads the Python 2 code. The file is not a SQL database, texture, or conventional script.

## What the editor can and cannot change

`verify` checks the complete outer marshal structure and embedded modules without executing the code. `list` reports the compiled code objects and constant paths. `extract` writes an embedded module's **raw Python 2 marshal code** to a separate file. `replace` edits one selected **string literal** (including nested embedded modules), updates length fields and hex encoding correctly, and verifies that **all compiled instruction bytes remain unchanged**.

This is deliberately **not** a Python-source decompiler or bytecode editor. It will not rewrite Python functions, imports, opcodes, argument counts, `co_code`, or `co_lnotab`. For full source edits you would need the original compatible Python 2 sources or a suitable Python 2 bytecode decompiler, then recompile and repackage with matching semantics. Recovering the exact original Python source cannot be guaranteed. Edited strings can still break client behavior if code treats them as imports, identifiers, or executable snippets.

`t` interned strings may be shared by `R` references, so replacement requires explicit `--allow-interned`. Always retain an original backup.

## Debian / Ubuntu / Linux examples

From the root of the Git checkout:

```bash
python3 tools/devtools_raw_editor.py utils/config/devtools.raw verify
python3 tools/devtools_raw_editor.py utils/config/devtools.raw list --scope devToolsWindow
python3 tools/devtools_raw_editor.py utils/config/devtools.raw list --json > devtools-inspection.json
```

For example, change a DevTools window's **visible label** `Dungeon Editor` without changing code instructions:

```bash
python3 tools/devtools_raw_editor.py utils/config/devtools.raw replace \
  --scope devToolsWindow \
  --path '$.co_consts[3].co_consts[5].co_consts[7]' \
  --expect 'Dungeon Editor' \
  --value 'Dungeon Tools' \
  --output devtools.edited.raw

python3 tools/devtools_raw_editor.py devtools.edited.raw verify
```

Paths are from the **current repository version**. Use the exact path displayed by `list` for your file. For multiline source snippets (for example, the outer `evemuLoad` script string) use `--expect-file old.txt --value-file new.txt` instead of shell arguments. The editor will validate marshal integrity, **not** Python 2 syntax of a new executable string.

To export a compiled module for external Python 2 disassembly:

```bash
python3 tools/devtools_raw_editor.py utils/config/devtools.raw extract \
  --scope devToolsConsole --output devToolsConsole.marshal
```

The exported file is still **compiled Python 2 marshal**, not source code; Python 3 cannot load it as a native Python 3 code object.

### Deploy in this repository's Debian Docker Compose environment

`docker-compose.yml` bind-mounts **`./config:/app/etc`**. Startup copies the built-in template from `/src/utils/config/devtools.raw` into `/app/etc/devtools.raw` **only if the destination doesn't exist**. Editing the repository's `utils/config/devtools.raw` alone does not update an existing `config/devtools.raw`.

First stop EVEmu. If the active file exists, back it up before replacing it:

```bash
docker compose stop server
mkdir -p config
cp -p config/devtools.raw config/devtools.raw.bak
cp devtools.edited.raw config/devtools.raw
chmod 644 config/devtools.raw
docker compose up -d server
```

If `config/devtools.raw` does not exist, back up the repository's `utils/config/devtools.raw` first, then copy the edited file to `config/devtools.raw`. No image rebuild or GitHub CI is needed for a bind-mounted file. **Do not use `docker compose down -v`**, which removes persistent data volumes including MariaDB.

For standalone Linux, the active file is `EVEMU_ROOT/etc/devtools.raw`; stop the server, back it up, copy in the new verified file, ensure it's readable by the server user, then restart the server/client.

## Windows 10/11 PowerShell examples

Install Python 3, then from the repository root:

```powershell
py -3 tools\devtools_raw_editor.py utils\config\devtools.raw verify
py -3 tools\devtools_raw_editor.py utils\config\devtools.raw list --scope devToolsWindow

py -3 tools\devtools_raw_editor.py utils\config\devtools.raw replace `
  --scope devToolsWindow `
  --path '$.co_consts[3].co_consts[5].co_consts[7]' `
  --expect 'Dungeon Editor' `
  --value 'Dungeon Tools' `
  --output devtools.edited.raw

py -3 tools\devtools_raw_editor.py devtools.edited.raw verify
```

Stop the native Windows EVEmu server, back up the file under its configured `EVEMU_ROOT\etc` directory, and copy the verified replacement to `etc\devtools.raw`. Restart the server and game client to reload it.

## Extra safeguards and testing

- `--output` refuses to overwrite an existing file; supply `--force` deliberately if necessary.
- `--in-place` writes a `.bak` backup and refuses to overwrite an existing backup.
- `--expect` (or `--expect-file`) must match the previous string exactly; the tool does not perform ambiguous global find/replace.
- Unknown marshal types, invalid `R` references, malformed lengths, and trailing bytes are rejected.
- The tool verifies the rewritten archive and compares all embedded compiled bytecode before writing.
- The tool **never executes** the compiled Python code and does **not** certify that edited client strings are semantically valid.
- Avoid arbitrary programs sourced from untrusted `.raw` files; these carry client-executable code.

Run the offline regression suite (no CI build or EVE client required):

```bash
python3 -m unittest discover -s tools/tests -p 'test_devtools_raw_editor.py' -v
```

The tests cover outer/nested parsing, replacement of nested literals, preserving compiled instructions, rejecting bytecode mutation, interned string protection, malformed-file rejection, and CLI extraction.

### Existing tool alternatives

Version-compatible Python bytecode tools (e.g. `xdis` / `uncompyle6`) may be useful for **disassembly or partial source recovery**, but are not guaranteed to reconstruct these compiled modules completely, and their input format may require additional wrapping of the exported marshal bytes. Generic EVE bulkdata/cache tools do not automatically support this Python 2 marshal + hex-embedded-code layout.

**CI/CD status:** Automatic GitHub Actions triggers remain disabled. The Python tool works locally without Actions.
