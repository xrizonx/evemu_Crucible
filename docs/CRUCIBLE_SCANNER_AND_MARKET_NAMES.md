# Scanner and market display names

## Wallet market journal

EVEmu's seeded NPC buyer uses owner ID 4000000. The wallet formats its journal entries from the owner IDs in each row. The configuration service now returns the name **Trader Joe** for that virtual owner, including requests for just the NPC and requests that also contain real characters or corporations. This resolves existing and new journal rows through the same owner lookup. The change requires a server restart with the updated binary and a fresh client session if the client cached the missing name. Journal rows, market orders, escrow and tax amounts keep their original values.

## Directional scan

When the scanner renders a remote asteroid record, the relevant reference client builds its display name by passing the type name in a positional argument to localization. The label expects a named `asteroidType` value. This accounts for an unresolved `{asteroidType}` token while the Type column can already show the correct ore. EVEmu sends the actual item and type IDs. The client formats the Name column itself.

For an offline client with matching editable scanner source, check the source from the repository root:

```powershell
python tools/fix_crucible_scanner_label.py 'C:\path\to\offline-client-source\scanner.py'
```

Run the same command with `--apply` to supply the named localization argument. The tool creates a `scanner.py.bak` backup and rejects unknown or ambiguous calls. Rebuild the client script bundle using your offline client's normal build process. A client that already supplies the named argument needs its language resources checked separately.

## In-game verification

- With a fresh session, open an old journal entry for a seeded NPC bid and complete a new sale against one. Both descriptions should show Trader Joe, and the sale and tax figures should match the wallet entries.
- Scan a belt containing Scordite and Veldspar with the updated offline client source integrated. Confirm both names resolve, including when the asteroid appears as a remote scan result. Confirm the Type column, distances and context menus still work.
