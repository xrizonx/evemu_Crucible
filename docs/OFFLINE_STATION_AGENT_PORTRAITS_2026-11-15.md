# Offline station-agent portraits — 15 November 2026

## Confirmed source problem

The 2012 Crucible client requests agent portraits from the local HTTP image service as `/Character/<agentID>_50.jpg`. The server previously first looked for `image_cache/Character/<agentID>_50.jpg` (or `_512.jpg`), then redirected a missing portrait to the external `images.evetech.net` service. Historical agents are not guaranteed to exist on that modern service, and the Crucible client may not follow HTTPS redirects.

The repository does **not** contain a verified, redistributable archive of the authentic historical NPC portraits. Neither `chrNPCCharacters` nor `chrPortraitData` can be assumed to contain complete JPEG binaries. The patch does not claim to have reconstructed historical character likenesses.

## Code changes

1. For a Character request whose ID passes the existing Crucible `IsAgent` check, `ImageServerConnection` returns the ordinary local `Character/<agentID>_<size>.jpg` first. The existing `_512.jpg` fallback remains intact.
2. When no local agent JPEG is present, serve a small, **compiled-in original placeholder JPEG** with HTTP 200 and `Content-Type: image/jpeg`; no CDN redirect or internet access is needed.
3. Missing player portraits and unrelated NPC/static image handling are unchanged. The agent fallback does not trigger for generic character IDs.
4. Bundling the fallback in the binary means Windows UCRT64, Linux and Docker can serve it without requiring a runtime file, Python imaging library or external asset download.
5. An offline-only Python import tool at `tools/agent_portraits/import_local.py` copies existing, appropriately licensed agent JPEG files to the local cache. It accepts `<id>_50.jpg`, `<id>_512.jpg`, or `<id>.jpg`; the unsuffixed variant is installed as `_512.jpg`. It never fetches images from the network and does not overwrite existing files unless asked.

## Adding real historical images from your own licensed archive

Export station agents from the **same database used by the server**:

```sql
SELECT DISTINCT a.agentID
FROM agtAgents AS a
INNER JOIN staStations AS s ON s.stationID = a.locationID
ORDER BY a.agentID;
```

Save the agent IDs as a UTF-8 text file (`agents.txt`; one number per line). Place your legitimately obtained historical portrait JPEGs in a local `originals` directory; name each file `<agentID>_50.jpg` or `<agentID>_512.jpg`. Then from the repository root, where your image cache is located:

```powershell
python tools/agent_portraits/import_local.py --source originals --ids agents.txt --output image_cache/Character --dry-run
python tools/agent_portraits/import_local.py --source originals --ids agents.txt --output image_cache/Character
```

If your server uses `<imageDir>../image_cache/</imageDir>` relative to its executable's working directory, point `--output` at that resolved `Character` directory, not necessarily the repository root. Existing JPGs take priority and are not replaced by the placeholder. Restarting may be unnecessary because the image server reads local files on each request; you may need to clear the client HTTP image cache or revisit the station.

## Verification

- Windows/Linux/Docker compile and test results must be checked on the new branch; the earlier green master CI does not validate this patch.
- Linux CI runs a standard-library-only regression suite for the local portrait import tool.
- In-game: open a station with agents while blocking internet access, capture the image-service HTTP response to `/Character/<id>_50.jpg`; confirm **200 image/jpeg** (not 301 or 404), and that the displayed local file wins over the built-in placeholder.
- Verify real player character images remain untouched; a missing player image must not be substituted with an NPC agent placeholder.
- After installing genuine local historical portraits, check multiple station agents with full and missing `chrNPCCharacters` metadata.

## Limitations

The compiled-in fallback is one generic fictional silhouette (64 px) shared by agents whose actual JPEGs are absent; it **does not** make every NPC agent appear historically authentic or unique. To show the true 2012 artwork, a complete legally distributable portrait archive mapped to agent IDs must be supplied separately. The image server may still redirect non-agent static categories to the modern image service; the change only guarantees offline **agent** portraits.
