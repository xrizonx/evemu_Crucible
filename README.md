# EVEmu Crucible — maintained Windows, Linux and Docker fork

<p align="center">
  <img src="docs/assets/readme/hero-banner.jpg" alt="EVEmu Crucible — maintained open-source server emulator, Windows, Linux and Docker" width="100%" />
</p>

<p align="center">
  <a href="https://github.com/xrizonx/evemu_Crucible/actions/workflows/build-release.yml"><img alt="master CI" src="https://github.com/xrizonx/evemu_Crucible/actions/workflows/build-release.yml/badge.svg?branch=master" /></a>
  <img alt="Windows" src="https://img.shields.io/badge/Windows-UCRT64-0078D6?logo=windows&logoColor=white" />
  <img alt="Linux" src="https://img.shields.io/badge/Linux-GCC-FCC624?logo=linux&logoColor=black" />
  <img alt="Docker" src="https://img.shields.io/badge/Docker-Build-2496ED?logo=docker&logoColor=white" />
</p>

[Source](https://github.com/xrizonx/evemu_Crucible) · [CI builds](https://github.com/xrizonx/evemu_Crucible/actions/workflows/build-release.yml) · [Report a bug / Issues](https://github.com/xrizonx/evemu_Crucible/issues) · [Full patch notes](PATCH_NOTES.md) · [Stability worklog](STABILITY_WORKLOG.md) · [Database guide](sql/README.md)

EVEmu is an educational, work-in-progress EVE Online server emulator based on [upstream EVEmu Crucible](https://github.com/EvEmu-Project/evemu_Crucible). It is intended for local development and testing, not public or production hosting.

**Active development and builds: `master`.** The Windows instructions below match this fork's GitHub Actions **Windows x86_64/UCRT64** package. If you already have an EVEmu installation, **back up its database and settings before upgrading**.

## Quick navigation

- [Latest verified fixes and change history](#november-2026-maintainer-handover--features-fixes-and-stability)
- [Open issue tracker and known limitations](#issue-tracker--outstanding-investigations)
- [What has changed](#changes-in-this-fork)
- [Download and install on Windows](#windows-11-and-windows-10-x64-setup)
- [MariaDB setup](#3-install-and-configure-mariadb)
- [EVEDBTool and database initialization](#4-configure-evedbtool)
- [Server XML and running EVEmu](#6-configure-the-server)
- [EVEDBTool command reference](#8-evedbtool-commands)
- [Build from master](#9-build-master-locally-on-windows)
- [Upgrade and troubleshoot](#10-upgrading-an-existing-server)
- [Docker/Linux](#docker-and-linux)
- [Outstanding client and SQL test backlog](#outstanding-test-backlog--crucible-client-server-sql-and-database-20-november-2026)

## Changes in this fork

For the complete chronological changes, see [PATCH_NOTES.md](PATCH_NOTES.md), [STABILITY_WORKLOG.md](STABILITY_WORKLOG.md), and the [master commit history](https://github.com/xrizonx/evemu_Crucible/commits/master). Early modernization changes (see the [November 2026 maintainer handover](#november-2026-maintainer-handover--features-fixes-and-stability) below for the newer changes through merged [PR #44](https://github.com/xrizonx/evemu_Crucible/pull/44)):

- **Windows and packaging:** modern MSYS2 UCRT64/GCC, CMake/Boost and Windows linker/API compatibility fixes; runtime DLLs bundled **next to `bin\eve-server.exe`**, including `libmariadb.dll`, `libtinyxml-0.dll` and `libz-1.dll`.
- **CI and tests:** Linux GCC, Windows UCRT64, Docker builds, CMake/CTest, weapon-formula tests, ccache, downloadable platform packages and checksums.
- **Database tools:** patched EVEDBTool 0.0.6 compiled for Windows/Linux and included with `base`, `migrations` and `dungeons`; safer market seed handling, dry run, region selection and force-reseeding safeguards.
- **Missions:** Blockade mission and agent acceptance/completion fixes, with ongoing mission-data/UI work. See [mission guide](docs/missions/MIGRATION_GUIDE.md).
- **Drones and targeting:** engage-command decoding, NPC acquisition and control range, aggressive/passive settings, focus fire, orbit/return/combat state transitions, lock callbacks, stale target pruning and entity-lifetime guards.
- **Weapons and fitted modules:** turret optimal/falloff/tracking, missile explosion velocity/radius damage, target validity checks, online/offline state handling, smartbomb AoE, bomb detonation/AoE and defender-missile logic.
- **General stability:** crash-path null checks, module/item safety, expired timer corrections, iterator safety and log cleanup.

**Test limitation:** passing Linux, Windows and Docker CI builds does not prove drone/weapon/mission mechanics are bug-free in a running game. New smartbomb, bomb, defender missile and drone behavior require in-game tests. Report runtime faults with the exact build SHA, logs, reproduction and crash dump.

## November 2026 maintainer handover — features, fixes and stability

> **Snapshot: 19 November 2026.** The latest reviewed merged feature is [PR #44](https://github.com/xrizonx/evemu_Crucible/pull/44), merge commit [`3fb1e546`](https://github.com/xrizonx/evemu_Crucible/commit/3fb1e546a9af386e29086e28dd95063dd00a7224). The corrected PR workflow [#44516480943](https://github.com/xrizonx/evemu_Crucible/actions/runs/44516480943) **passed Windows UCRT64 GCC, Linux GCC, Docker and the MariaDB NPC seed integration job**. A successful source build and source-invariant/CTest run do **not** prove every game mechanic or migration has been tested with a live client and full production-sized database. This document describes the audited/recorded changes, not a claimed line-by-line certification of the whole repository.

### Recent merged changes — what they do

| Area | Merged change | What was fixed or introduced | Remaining verification |
| --- | --- | --- | --- |
| **High-volume mineral/ore NPC buys** | [PR #44](https://github.com/xrizonx/evemu_Crucible/pull/44) | Published, market-listed mineral **group 18** and asteroid/ore **category 25** static NPC buy orders target **100,000,000 units per type/station** subject to the 10T ISK notional escrow ceiling. Sampled mineral, Veldspar and escrow-capped cases now run in isolated MariaDB CI. | Staging full SDE, real-client sale at various stack sizes, order counts/storage and latency |
| **Market performance** | [PR #44](https://github.com/xrizonx/evemu_Crucible/pull/44) | Material/mineral price initialization consolidates one-per-type `invTypes` queries into one bounded base-price fetch; historical **1.05× material / 1.15× mineral** modifiers are retained. | Measure database and memory behavior on large catalogs; evaluate indexing and remaining per-row writes |
| **Docker production compilation** | [PR #44](https://github.com/xrizonx/evemu_Crucible/pull/44) | Container server now compiles with CMake **Release**, consistent with Windows/Linux Release CI, rather than Debug. | Compare runtime throughput and verify diagnostics strategy for dedicated Debug images |
| **Compiler repair** | [PR #44](https://github.com/xrizonx/evemu_Crucible/pull/44) | Removed the extra C++ brace left near `MarketDB::GetMineralPrices()`; the first run failed Windows/Linux/Docker but corrected CI then passed. A source guard helps catch accidental helper-boundary regressions. | None for this specific syntax regression beyond successful CI |
| **Warp, gate/docking navigation** | [PR #43](https://github.com/xrizonx/evemu_Crucible/pull/43) | Warp-to-zero targets **2,400 m surface clearance** inside the **2,500 m** jump/docking threshold; ship and object radii counted once, autopilot standoff retained. Warp alignment/throttle, ordered landing position+velocity+stop+FX, station/gate approach overshoot, gate transfer state, cloak and invulnerability timer handling hardened. | Occasional **missing client warp tunnel** remains reported; investigate [#45](https://github.com/xrizonx/evemu_Crucible/issues/45). Live operator had verified normal warp/jump/dock; this is not a guarantee for every flight |
| **Mining drones** | [PR #42](https://github.com/xrizonx/evemu_Crucible/pull/42) | Improved range-based asteroid engagement, mass/speed fallbacks, approach/orbit/return movement updates and keeping mined ore payload until returning to controller. | End-to-end AU-scale overview distance, drone ore transfer and recall on live client [#48](https://github.com/xrizonx/evemu_Crucible/issues/48) |
| **DED/Officer expedition** | [PR #41](https://github.com/xrizonx/evemu_Crucible/pull/41) | Excluded known untargetable **Domination Grigori** while retaining the existing True Sansha exclusion, other valid officers and expedition composition. | Fresh mission generation, targetability, kill completion and loot checks |
| **Sovereignty/IHub/POS, industry, movement** | [PR #40](https://github.com/xrizonx/evemu_Crucible/pull/40), [PR #39](https://github.com/xrizonx/evemu_Crucible/pull/39) and associated merged work | DEV/ADMIN `/sovindex5` override, raw-point width migrations, effective index values, infrastructure-upgrade validation, tower and forcefield handling, security/ownership hardening, SBU counting, structure lifecycle cleanup, tractor movement, Crucible static attributes and research/copying safeguards. | Full functional upgrade/forcefield and activity-driven index behavior [#49](https://github.com/xrizonx/evemu_Crucible/issues/49); industry/POS jobs [#47](https://github.com/xrizonx/evemu_Crucible/issues/47) |
| **NPC market buy settlement** | [PR #35](https://github.com/xrizonx/evemu_Crucible/pull/35) and later fixes | Introduced separate static NPC buy seed, 100% `basePrice` pricing, supported pseudo-owner **4000000**, station-local range **0** compatible with unsigned MariaDB `orderRange`, scoped legacy NPC bid migration, quantity/type/station checks and stored-order-price payout. | Real player inventory/ISK reconciliation, rollback/failure injection, high-volume escrow checks [#46](https://github.com/xrizonx/evemu_Crucible/issues/46) |

### Older fixes retained on master

The fork also contains earlier combat and gameplay improvements recorded in [full patch notes](PATCH_NOTES.md), [stability worklog](STABILITY_WORKLOG.md) and [upstream issue triage](ISSUE_TRIAGE.md). Important retained areas include:

- **Weapon and ship combat:** turret transversal versus radial velocity, optimal/falloff/tracking, missile explosion radius/velocity, shield remote transfer, online/offline fitted modules, smartbombs, bombs, defender missiles, damage and target/entity lifetime safety.
- **Missions and agents:** synthetic Blockade and DED/Officer security missions, mission objective accounting and spawn cleanup, agent journal and data access guards, objective persistence and reward paths. In-game turn-in/reward behavior requires independent verification.
- **Ships, modules and inventory:** station/cargo refresh, drag/drop item ID compatibility, manufacturing/research access restrictions, zero/invalid material amounts, non-player entity handling and safeguards against missing inventory/station objects.
- **Sovereignty and POS:** TCU claim/FILETIME persistence, SBU distinct-gate rules, secure POS commands, tower onlining persistence, silo type capacity, bridge link checks, ownership and flag validation, cleanup of new structure metadata orphans. Existing orphan rows are **not** automatically deleted.
- **Server stability:** malformed packet/input guards, invalid orbit geometry/speed fallback, stale targets, crash-path null checks, iterator invalidation, move/update packet sequencing and lower idle stop-packet churn.
- **Compatibility and packaging:** MSYS2 UCRT64 Windows fixes for Boost, Winsock/DbgHelp and BFD; bundled required runtime DLLs, compiled EVEDBTool, Linux package, Docker image and cross-platform regression checks. The source is **Crucible-era**; do not use the current retail EVE client.

### CI/CD status, scope and how to download builds

- Workflow: [EVEmu Build and Release](https://github.com/xrizonx/evemu_Crucible/actions/workflows/build-release.yml), triggered on `master` pushes, pull requests to `master`, manual dispatch and release tags.
- **Windows**: MSYS2 **UCRT64 GCC**, CMake/Ninja Release, CTest/source invariants, patched Go EVEDBTool (`evedbtool.exe`), runtime DLL dependency collection, downloadable **`evemu-crucible-windows-x86_64.zip`** and SHA256.
- **Linux**: GCC Release + CTest/source invariants, patched Go EVEDBTool, staged tools/config/SQL, downloadable **`evemu-crucible-linux-x86_64.tar.gz`** and SHA256.
- **Docker**: Debian 12 multi-stage build with optimized Release server and compiled EVEDBTool, verified by CI build. **MariaDB** job runs isolated migrations, seed insert/reseed and unsigned-order-range fixtures with explicit tests for large mineral/ore quantities, price, escrow and player order preservation.
- CI employs `ccache` and Ninja where configured. **Temporary Actions artifacts are not the same as tagged GitHub Releases**; artifacts expire after their configured retention. The release publishing job is intentionally skipped for normal PR or branch runs.
- Evidence: [successful corrected PR #44 CI](https://github.com/xrizonx/evemu_Crucible/actions/runs/44516480943) and [successful PR #43 gameplay-related CI](https://github.com/xrizonx/evemu_Crucible/actions/runs/44473762678). Select a **successful run for your intended SHA**, ideally the latest `master` run, before deployment.

### Important market upgrade instructions — 100 million units

**The new quantity applies to newly inserted static NPC buy orders; a code update does not change existing database rows.** Run EVEDBTool from the packaged `share/evemu/sql` directory (or the `sql` directory in a source build), using the correct credentials in the private `evedb.yaml`.

1. **Stop the server and market bot.** Back up MariaDB and settings, verify the backup can be restored, and inspect `seed-regions` and `seed-saturation`. Saturation samples **stations**, not quantity per item; the script does not seed omitted regions/stations.
2. **Preview the read-only SQL output**: Windows `./evedbtool.exe seed -buy-only -force -dryrun` (PowerShell can use `.\\evedbtool.exe`); Linux `./evedbtool seed -buy-only -force -dryrun`. Inspect deletion scope and legacy NPC buy normalization carefully.
3. **Only when intentionally rebuilding existing static NPC buys**, run `./evedbtool.exe seed -buy-only -force` on Windows (or `./evedbtool seed -buy-only -force` on Linux). This replaces seed-managed buy orders in configured regions; it does **not** delete player orders or run the separate NPC sell seed. An ordinary non-force `seed -buy-only` is for first-time installation and does not adjust existing static buy quantities.
4. Run the [read-only 100M mineral/ore audit](sql/diagnostics/2026-11-19-mineral-ore-npc-buy-100m-audit.sql) and the [broader 100%-base-price NPC buy audit](sql/diagnostics/2026-11-17-npc-buy-orders-100pct-audit.sql). Check duplicate station/type bids, prices, volumes, min volume and escrow; then verify partial/full stack selling and player wallet changes in the compatible game client.

**Market constraints:** all static NPC bids must have `ownerID=4000000`, `bid=1`, `isCorp=0`, `orderRange=0`, `duration=250`. 100M units are a **maximum target**, not a universal guarantee: `LEAST(standardQuantity, FLOOR(10000000000000 / referencePrice))` limits expensive orders, with the existing one-unit minimum where eligible. Market pricing is exactly the database's positive static `invTypes.basePrice`, not a live regional market average. **Never use general `seed -force` merely to refresh buys**; it also refreshes NPC corporation sell orders.

### Stability validation checklist

For operator acceptance after upgrading the server and migrations, use the **same build SHA** for all runs and retain logs:

| Subsystem | Minimum client/database acceptance |
| --- | --- |
| Login, station, images | Log in/out and re-enter station, load NPC agent portraits and hangars, verify cargo contents and image service without crash |
| Ship warp/jump/dock | Repeat manual and autopilot warp, verify **visible warp tunnel**, gate surface arrival, 2,500m activation, docking and jump-cloak timing on small/large hulls |
| Combat/NPCs | Warp to belt/anomaly; verify NPC spawn, orbit, targeting, correct damage, no repeating Destiny errors, no stale entity crashes |
| Mining drones | Lock asteroid, deploy, approach/orbit/harvest, return to moving ship, recall early, confirm ore reaches cargo |
| Market | Search orders under load; buy/sell partial stacks, exactly-once wallet credit and order decrement, 100M mineral/ore static bids, seed idempotence |
| Missions | Accept Blockade and DED/Officer, check fleet composition/targetability, complete all kills, journal progress, ISK reward exactly once and pocket cleanup |
| Sovereignty/POS | Apply current SQL migrations first; test DEV-only `/sovindex5`, IHub upgrades and persistence, online towers and visual forcefield, authorized roles |
| Industry | Station BPO ME/PE, copying and delivery; reject invalid permissions/runs and invention **without charging resources**; POS/remote jobs remain incomplete |

A green CI matrix proves that covered build/test paths passed; it **does not certify production readiness** or all possible client interactions.

## Issue tracker — outstanding investigations

> **Issue tracker is accepting bug reports:** [Open a GitHub issue](https://github.com/xrizonx/evemu_Crucible/issues/new) · [View current issues](https://github.com/xrizonx/evemu_Crucible/issues). Please search for duplicates first. Include the `BUILD-INFO.txt` SHA and platform (Windows UCRT64, Linux or Docker), MariaDB version, client build, exact steps to reproduce, expected/actual behavior, relevant server and client logs, SQL migration status and screenshots/packet traces when applicable. **Never post passwords, session secrets, player personal information or unredacted production database dumps.**

**Open at documentation update (19 November 2026):** the fork's issue search returned no previous open issues; five **new investigation issues** were filed to track specific reported symptoms and engineering risks. Their existence does not mean every root cause is confirmed.

| Tracking issue | Status / suspected or documented problem | Next evidence needed |
| --- | --- | --- |
| [#45 — intermittent missing warp tunnel](https://github.com/xrizonx/evemu_Crucible/issues/45) | **Reported intermittent behavior; root cause unconfirmed.** Ship may appear at arrival with no warp animation. Bubble membership and Destiny event delivery are investigation targets, not established causes. | Repeated warp runs with Destiny packet trace and server logs across gate transitions and hull sizes |
| [#46 — market settlement atomicity](https://github.com/xrizonx/evemu_Crucible/issues/46) | **Known architecture risk**, not a demonstrated player exploit: order, wallet, item and history operations may not all commit or roll back atomically. | Fault-injected transaction/retry test with wallet/inventory/order reconciliation |
| [#47 — invention/POS jobs/industry delivery](https://github.com/xrizonx/evemu_Crucible/issues/47) | **Documented incomplete:** invention intentionally disabled; POS/remote job workflow and transaction-safe job delivery remain incomplete. | Full job install, cost, restart, delivery/rollback tests and authentic T2 output model |
| [#48 — station images, cargo and mining drone return](https://github.com/xrizonx/evemu_Crucible/issues/48) | **Unverified gameplay/stability:** historical 50px agent thumbnail/client station crash, cargo refresh and ore return need live re-testing. | Crash dumps, image HTTP traces, item location/quantity before and after drone return |
| [#49 — IHub upgrades and POS/forcefield](https://github.com/xrizonx/evemu_Crucible/issues/49) | **Incomplete/needs verification:** functional upgrade services, forcefield visibility, tower restart and genuine activity-driven SOV indices. | Client UI screenshots, tower state and authoritative database checks after restart |

**Additional watchlist (not automatically filed as confirmed defects):** upstream [pod-death crash #154](https://github.com/EvEmu-Project/evemu_Crucible/issues/154), [scanner crash #269](https://github.com/EvEmu-Project/evemu_Crucible/issues/269), [target-painted docking crash #39](https://github.com/EvEmu-Project/evemu_Crucible/issues/39), [outpost construction crash #315](https://github.com/EvEmu-Project/evemu_Crucible/issues/315); review [upstream issue triage](ISSUE_TRIAGE.md) before assuming a historic upstream report still affects this fork. Any reproducible bug should become its own issue with evidence.

# Windows 11 and Windows 10 x64 setup

<p align="center">
  <img src="docs/assets/readme/windows-setup-banner.jpg" alt="Windows Setup — Install, Configure, Run" width="100%" />
</p>

You do **not** need Git, Go, a compiler or Docker to run the prebuilt Windows ZIP. You **do** need a Windows MariaDB server and the packaged SQL assets.

## 1. Download the Windows server

1. Open the [master build workflow](https://github.com/xrizonx/evemu_Crucible/actions/workflows/build-release.yml), select a **successful run on master**, and download the `evemu-crucible-windows-x86_64` artifact.
2. GitHub may wrap the package in an outer ZIP. Extract that first, then extract `evemu-crucible-windows-x86_64.zip` to a convenient directory, such as `C:\EVEmu`.
3. Verify `BUILD-INFO.txt` reports the commit and `Windows x86_64 UCRT64`. The Actions artifact retention is 14 days; tagged release packages (when published) are on the [Releases page](https://github.com/xrizonx/evemu_Crucible/releases).

The unpacked layout should include:

~~~text
C:\EVEmu\
  BUILD-INFO.txt
  bin\
    eve-server.exe
    libmariadb.dll
    libtinyxml-0.dll
    libz-1.dll
    ...other runtime DLLs...
  etc\
    devtools.raw
  share\
    evemu\
      sql\
        evedbtool.exe
        evedb.yaml.example
        base\                   (*.sql.gz files)
        migrations\
        dungeons\
      utils\
        config\
          eve-server.xml
          log.ini
          MarketBot.xml
          devtools.raw
      tools\
~~~

**Do not move the DLLs away from `bin\eve-server.exe`.** The compressed SQL data, migration scripts and `evedbtool.exe` must remain together under `share\evemu\sql`.

**Editing `devtools.raw`:** This is compiled Python 2 marshal data, not plain text. Use the [cross-platform DevTools RAW inspector/editor](docs/DEVTOOLS_RAW_EDITOR.md) for safe string-constant edits, module inspection and Windows/Linux/Debian Docker deployment instructions. The active Docker file is `./config/devtools.raw`; the source `utils/config/devtools.raw` is only a template.

## 2. Prepare Windows folders and configuration templates

On a **new installation**, open PowerShell:

~~~powershell
Set-Location C:\EVEmu
New-Item -ItemType Directory -Force -Path .\etc, .\logs, .\server_cache, .\image_cache | Out-Null
Copy-Item .\share\evemu\utils\config\eve-server.xml .\etc\eve-server.xml
Copy-Item .\share\evemu\utils\config\log.ini .\etc\log.ini
Copy-Item .\share\evemu\utils\config\MarketBot.xml .\etc\MarketBot.xml
Copy-Item .\share\evemu\utils\config\devtools.raw .\etc\devtools.raw
~~~

These copy commands **replace existing config files**. Do not run them on an upgraded installation without first backing up and comparing your working settings.

The server's default compiled root is `..`, so it expects `..\etc\eve-server.xml` **relative to the working directory**. Run the executable **from `C:\EVEmu\bin`**. Running it from the package root can make its XML and log paths fail.

## 3. Install and configure MariaDB

<p align="center">
  <img src="docs/assets/readme/database-tools-banner.jpg" alt="Database and Tools — MariaDB, EVEDBTool and migrations" width="100%" />
</p>

1. Install 64-bit MariaDB Server for Windows from [mariadb.org/download](https://mariadb.org/download/). The Docker image uses MariaDB 11.8; your Windows MariaDB installation is separate.
2. During installation, create a strong **root password**, enable the **MariaDB Windows service**, and use TCP/IP on `3306` (or use your chosen port consistently everywhere).
3. Start/check the service (its name may differ): `Get-Service *maria*` in PowerShell. If MariaDB is not on PATH, open `mariadb.exe` from the installed MariaDB `bin` directory.
4. Open a terminal and connect as MariaDB administrator:

~~~powershell
mariadb -u root -p
~~~

At the `MariaDB>` SQL prompt, create a dedicated local database and account. Replace `REPLACE_WITH_STRONG_PASSWORD` with your **own** secret:

~~~sql
CREATE DATABASE IF NOT EXISTS evemu
  CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER IF NOT EXISTS 'evemu'@'localhost'
  IDENTIFIED BY 'REPLACE_WITH_STRONG_PASSWORD';
CREATE USER IF NOT EXISTS 'evemu'@'127.0.0.1'
  IDENTIFIED BY 'REPLACE_WITH_STRONG_PASSWORD';
GRANT ALL PRIVILEGES ON evemu.* TO 'evemu'@'localhost';
GRANT ALL PRIVILEGES ON evemu.* TO 'evemu'@'127.0.0.1';
FLUSH PRIVILEGES;
EXIT;
~~~

Test the dedicated user's TCP connection:

~~~powershell
mariadb --host=127.0.0.1 --port=3306 --user=evemu -p evemu
~~~

Use its new password and type `EXIT;` afterward. Keep the MariaDB port local rather than opening it to the internet. Existing databases should **not** be recreated; use their current database name and credentials.

## 4. Configure EVEDBTool

The **patched Windows** database tool is `C:\EVEmu\share\evemu\sql\evedbtool.exe`. In PowerShell:

~~~powershell
Set-Location C:\EVEmu\share\evemu\sql
Copy-Item .\evedb.yaml.example .\evedb.yaml
notepad .\evedb.yaml
~~~

For the fresh MariaDB setup above, use:

~~~yaml
log-level: Info
db-host: 127.0.0.1
db-port: "3306"
db-user: evemu
db-pass: REPLACE_WITH_STRONG_PASSWORD
db-database: evemu
migrations-dir: migrations
base-dir: base
dungeon-dir: dungeons
seed-regions:
  - Derelik
seed-saturation: 80
~~~

Your `db-pass` must match MariaDB and `eve-server.xml`; your `db-database` must match the database you actually created (the bundled template originally uses `EvE`, so update it to `evemu` for this example). The `base`, `migrations` and `dungeons` paths are **relative to the current `sql` directory**. Always run EVEDBTool from that directory. Keep `evedb.yaml` private and out of Git.

**Source checkout without `evedbtool.exe`?** Install Git and Go **1.23+**, then run `sql\get_evedbtool.bat`. It clones upstream EVEDBTool tag 0.0.6, adds the fork's `tools/evedbtool/command_seed.go` and builds `sql\evedbtool.exe`. The standard Actions ZIP **already includes this patched executable**; no additional download is required.

## 5. Initialize or upgrade the database

**Choose the correct procedure. Never perform the new-install steps on your populated server database.** Stop the server before running schema or seed changes.

### A. Fresh and empty `evemu` database

In `C:\EVEmu\share\evemu\sql`:

~~~powershell
.\evedbtool.exe install
.\evedbtool.exe status
~~~

`install` reads the `base\*.sql.gz` database assets **when there are no tables** and then applies the included migrations. Check its output for errors; `status` lists migration history. You do not need to unpack the `*.sql.gz` files manually.

Optionally import the bundled dungeon JSON files:

~~~powershell
.\evedbtool.exe dungeon apply
.\evedbtool.exe dungeon list
~~~

Optionally seed NPC market **sell and buy** orders in the regions specified by `evedb.yaml`. All newly seeded NPC buy orders offer **100% of published item `invTypes.basePrice`**, with no security modifier. First review the generated SQL:

~~~powershell
.\evedbtool.exe seed -dryrun
.\evedbtool.exe seed
~~~

Normal initial seeding has separate NPC sell and buy entries in `seed_migrations`. On an existing database, run `evedbtool.exe seed -buy-only -dryrun` followed by `evedbtool.exe seed -buy-only` after a backup. Do **not** use `seed -force` for an initial installation or an existing-data buy-only upgrade. See [NPC buy seed verification](docs/NPC_BUY_MARKET_100_PERCENT_2026-11-17.md).

### B. Existing EVEmu database (accounts, characters and inventory retained)

1. Stop `eve-server.exe` and back up the database, `etc` config, and `evedb.yaml`. Example PowerShell backup with MariaDB's `mariadb-dump` utility:

~~~powershell
New-Item -ItemType Directory -Force C:\EVEmu\backups | Out-Null
mariadb-dump --host=127.0.0.1 --port=3306 --user=evemu --password --result-file="C:\EVEmu\backups\evemu-before-update.sql" evemu
~~~

The dump command prompts for a password. Change `evemu` in the final argument if using a different database name. Check that the file is nonempty and that your backup can be restored.

2. Copy your **existing**, private `evedb.yaml` into the new `sql` directory; confirm it points to the same database as the server.
3. Inspect **before** applying migrations:

~~~powershell
Set-Location C:\EVEmu\share\evemu\sql
.\evedbtool.exe status
.\evedbtool.exe up -dryrun
~~~

4. Review the pending migration SQL for schema/data changes. If appropriate and with a verified backup, apply:

~~~powershell
.\evedbtool.exe up
.\evedbtool.exe status
~~~

**Never use `skip` just to bypass migration errors**. If you applied each pending migration manually with a SQL client, follow the [manual migration process](sql/README.md) before recording it as applied with `skip`. `install` is for first-time installation, not the routine way to upgrade an active database.

The separate `sql/mission_data` staging files are **not** automatically installed by this procedure and should only be applied under the [mission migration process](docs/missions/MIGRATION_GUIDE.md).

## 6. Configure the server

Edit `C:\EVEmu\etc\eve-server.xml`. Replace the bundled `database_host` / `database_username` / `database_password` / `database_name` / `database_port` placeholders with the same settings used in MariaDB and `evedb.yaml`:

~~~xml
<database>
    <host>127.0.0.1</host>
    <username>evemu</username>
    <password>REPLACE_WITH_STRONG_PASSWORD</password>
    <db>evemu</db>
    <port>3306</port>
    <ssl>false</ssl>
    <compress>false</compress>
    <useSocket>false</useSocket>
    <autoReconnect>true</autoReconnect>
</database>
~~~

Keep other optional database tags from the actual template. Check the file paths and network configuration:

~~~xml
<files>
    <logDir>../logs/</logDir>
    <logSettings>../etc/log.ini</logSettings>
    <cacheDir>../server_cache/</cacheDir>
    <imageDir>../image_cache/</imageDir>
</files>
<net>
    <port>26000</port>
    <imageServer>127.0.0.1</imageServer>
    <imageServerPort>26001</imageServerPort>
</net>
~~~

- Game login uses TCP port **26000**; the image server uses **26001**.
- `imageServer=127.0.0.1` is only suitable for the EVE client running on the **same PC**. For another computer on the LAN, use an address reachable from that client and allow the two ports through Windows Firewall. Do not open MariaDB port 3306 for game access.
- The client needs to be compatible with the **Crucible-era** protocol; do not expect the current retail client to connect.
- `etc\log.ini` controls logging; `logs`, `server_cache` and `image_cache` should exist and be writable.
- **Complete agent portrait experiment rollback (16 November 2026):** the added NPC-agent portrait helper, importer and fallback JPEG are deleted; the entire HTTP image handler is restored to its **pre-PR #17** version. Player portraits and other image categories follow their historical behavior (including existing external redirects for missing images). The client crash has **not yet been resolved**; see [rollback and crash-triage instructions](docs/AGENT_PORTRAIT_FULL_REMOVAL_2026-11-16.md). Agent missions, market containers and assembled containers remain unchanged.
- **Crucible legacy image compatibility (16 November 2026):** a subsequent focused patch disables the **pre-existing modern HTTPS redirect only for missing `Character` images** (including NPC agent 50px portraits); existing valid local player/agent JPEGs still work. Empty or obviously non-JPEG cached files are rejected, and external redirects for corporation/alliance logos and type icons/renders are preserved. See [current image-response diagnostic](docs/CRUCIBLE_IMAGE_REDIRECT_CRASH_TRIAGE_2026-11-16.md). The station-load crash still needs real client verification.
- **50px station-agent image compatibility trial:** after confirming 404 still coincided with a client crash, the server now serves an exact 50×50, baseline 322-byte neutral JPEG for legacy 3-million-range agent thumbnails, with fully framed HTTP/1.0 200 headers. No original CCP portrait is bundled. Agent journal and cargo container crash/data-loss guards are included. See the [station crash diagnostic runbook](docs/CRUCIBLE_STATION_AGENT_50PX_CLIENT_CRASH_TRIAGE_2026-11-16.md); actual in-client stability is not yet proven.
- `etc\MarketBot.xml` provides market bot settings. The server XML's `<TraderJoe>` option controls that feature. `etc\devtools.raw` is also bundled.
- Edit XML with a plain-text editor; preserve the rest of the template.

## 7. Run and verify EVEmu

Start the MariaDB service, then in **PowerShell**:

~~~powershell
Set-Location C:\EVEmu\bin
.\eve-server.exe
~~~

Watch its startup output for successful configuration parsing, database connection, static data load, and the TCP/image service ports. In a second terminal:

~~~powershell
Test-NetConnection 127.0.0.1 -Port 26000
Test-NetConnection 127.0.0.1 -Port 26001
~~~

A listening port is not an in-game test. Connect with a compatible EVE client; new usernames are normally created automatically on first login. Stop the server cleanly using the console or Ctrl+C before modifying database schema.

If the server cannot open `eve-server.xml`, check your **current working directory**: it must be `C:\EVEmu\bin` with default `EVEMU_ROOT=..`. If it cannot find DLLs, extract the complete build ZIP and keep its bundled DLLs alongside `eve-server.exe`. If database authentication fails, check MariaDB service, permissions and matching XML/YAML credentials.

## 8. EVEDBTool commands

Run all commands from `C:\EVEmu\share\evemu\sql`:

| Command | Meaning |
| --- | --- |
| `.\evedbtool.exe status` | Show migration versions recorded in the database. |
| `.\evedbtool.exe install` | For a **new empty database**: load base tables and migrate. |
| `.\evedbtool.exe up -dryrun` | Print pending migration SQL without changing data. |
| `.\evedbtool.exe up` | Apply pending migrations after review and backup. |
| `.\evedbtool.exe skip` | Record migration versions **without executing SQL**. Use ONLY after the SQL was applied and verified manually. |
| `.\evedbtool.exe seed -dryrun` | Preview market seed SQL. |
| `.\evedbtool.exe seed` | Initial market seed, if not already recorded. |
| `.\evedbtool.exe seed -force -dryrun` | Preview a potentially destructive NPC market rebuild. |
| `.\evedbtool.exe seed -buy-only -dryrun` | Preview only the static NPC buy seed (including scoped legacy NPC buy normalization). |
| `.\evedbtool.exe seed -buy-only` | Add the buy seed to a previously sell-seeded installation when no buy migration was applied. |
| `.\evedbtool.exe seed -buy-only -force -dryrun` | Preview deletion/recreation of seed-managed NPC buys only. |
| `.\evedbtool.exe seed -buy-only -force` | **Destructive static NPC buy refresh** in configured regions; back up first. Does not force rebuild NPC sells. |
| `.\evedbtool.exe seed -force` | **Destructive NPC market refresh** of eligible corporation sell orders **and** seed-managed static NPC buys in configured regions; back up first. Player orders are retained by the implemented owner/range filters. |
| `.\evedbtool.exe dungeon apply` | Import supplied dungeon definitions from `dungeons`. |
| `.\evedbtool.exe dungeon list` | List installed dungeons. |
| `.\evedbtool.exe dungeon list-factions` | Display faction IDs. |
| `.\evedbtool.exe dungeon list-archetypes` | Display archetype IDs. |

`seed-regions` accepts region names or numeric IDs, and `seed-saturation` accepts 1–100. The **buy seed** creates up to **100,000,000 units per mineral (group 18) and asteroid/ore (category 25) item** at each selected station, 550,000 units for charges/ammunition, and 550 units for other published market types. Every static NPC buy is subject to the existing **10 trillion ISK pseudo-escrow ceiling**, so high-priced items can receive fewer units; pricing uses 100% of positive `invTypes.basePrice` (rounded to cents), with a 100 ISK fallback for nonpositive or missing values. The **separate sell seed** keeps 550,000 charges, 550 standard items, and 10 rare Officer/Deadspace/Faction/Storyline modules where eligible. `seed -buy-only -force` deletes/recreates the marker-matched **static NPC buy orders** in configured regions while retaining player orders and NPC sell orders; ordinary `seed -force` rebuilds **both the static buy and eligible NPC corporation sell seeds**. Stop server/bot, verify full backup and inspect `-dryrun` before either force operation. Normal `seed -buy-only` does **not** retrofit preexisting buy-order volumes. Full database-tool documentation: [sql/README.md](sql/README.md).

## 9. Build master locally on Windows

**Recommended:** download the ready-made Windows ZIP from a successful master Actions run. For source builds:

1. Install [Git for Windows](https://git-scm.com/downloads/win) and [MSYS2](https://www.msys2.org/).
2. Open the **MSYS2 UCRT64** terminal; update packages with `pacman -Syu` and reopen the terminal if needed.
3. Install the same dependencies as CI:

~~~bash
pacman -S --needed git \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-binutils \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-boost \
  mingw-w64-ucrt-x86_64-libmariadbclient \
  mingw-w64-ucrt-x86_64-tinyxml \
  mingw-w64-ucrt-x86_64-utf8cpp \
  mingw-w64-ucrt-x86_64-zlib
~~~

4. Still in UCRT64, clone and compile:

~~~bash
git clone --branch master https://github.com/xrizonx/evemu_Crucible.git
cd evemu_Crucible
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
cmake --install build --prefix stage
~~~

A **local** `stage/bin/eve-server.exe` installation is **not a standalone portable package**. It still needs matching UCRT64 DLLs (or to be run inside UCRT64), its `etc` files and the SQL assets. CI handles portable DLL collection and file packaging. To build the patched EVEDBTool independently, install Go 1.23+ and run `sql\get_evedbtool.bat` from a source checkout.

Every push to `master` triggers **Linux GCC**, **Windows UCRT64** and **Docker** jobs, including registered CTest tests. Only tag-triggered runs (`v*` or `stable-rizonx`) publish GitHub releases; ordinary branch pushes create temporary artifacts.

## 10. Upgrading an existing server

1. Stop `eve-server.exe`. Back up your MariaDB database, customized `etc` files and private `evedb.yaml`.
2. Extract a **successful master build** into a **new folder** rather than overwriting the working installation.
3. Keep the new `bin` and runtime DLLs together. Copy your current settings, compare new templates with custom values, and retain your saved YAML configuration.
4. Use the new `sql` directory and EVEDBTool to run `status` and `up -dryrun`, then apply only reviewed necessary migrations.
5. Start from the new package's `bin` folder and validate login, missions, drones and modules **before** removing the old installation.

### Troubleshooting

| Symptom | First check |
| --- | --- |
| Config XML missing | Copy `share\evemu\utils\config\eve-server.xml` to `etc` and run from `bin`. |
| Missing `libmariadb.dll` / `libtinyxml-0.dll` / `libz-1.dll` | Extract the **full Windows ZIP**; keep DLLs in `bin` alongside the EXE. |
| Database access denied/refused | MariaDB service, `127.0.0.1:3306`, user grants, XML and YAML password/DB name. |
| Tool cannot read base/migrations | Use `share\evemu\sql` as the working directory; check `evedb.yaml`. |
| Server says database tables are missing | `install` for **empty DB**; `status`/`up -dryrun` for an **existing DB**, after backup. |
| Client connects but image service fails | `imageServer` address, port 26001 and Windows Firewall. |
| Crash or unstable combat | Gather logs, reproduction, `BUILD-INFO.txt` commit and a symbolized crash dump; source CI does not prove runtime safety. |

## Docker and Linux

Docker builds from `master` remain supported for development:

~~~bash
git clone --branch master https://github.com/xrizonx/evemu_Crucible.git
cd evemu_Crucible
docker compose up -d --build
docker compose logs -f server
docker compose stop
~~~

The compose file uses example MariaDB credentials (`evemu/evemu`); change these before shared or public exposure and keep container configuration consistent. Container startup invokes EVEDBTool and is **not** the same as the manual Windows steps above. Linux build artifacts include `sql/evedbtool`, bundled SQL resources and the server installation.

## Outstanding test backlog — Crucible client, server, SQL and database (20 November 2026)

**Status: pending / not certified.** The checklist below records tests that still require a reproducible result against the **final deployed commit**, even where older feature-specific checks or source guards have passed. Do not mark a case successful merely because the server compiles, a scripted source assertion passes, or the functionality was described in a merged PR. Track actual results (PASS / FAIL / BLOCKED / NOT RUN), dates and evidence in the issue tracker. This is an operator acceptance backlog, **not a claim that every C++ or SQL file has been reviewed**.

**Test baseline and preparation:** Confirm `BUILD-INFO.txt`, Crucible-era client build, operating system, MariaDB version, schema/migration state and matching server SHA. Use a **disposable, backed-up staging DB** for destructive, restart, failed-SQL and fault-injection tests; preserve player accounts, inventory and wallets. Use two clients/characters for visibility, trade, PvP and permissions tests. Keep logs, server/client crash dumps, Destiny/network traces, screenshots and SQL read-only before/after snapshots (with secrets and personal data redacted).

**Priority:** **P0** = release-blocking data integrity, startup, login or core gameplay; **P1** = functional and regression coverage; **P2** = performance and edge cases. Unchecked boxes are **NOT RUN** unless the linked issue records fresh evidence; **known unimplemented features are not to be enabled just to make a test pass**. For earlier test procedures see [client gameplay checklist](docs/CRUCIBLE_CLIENT_GAMEPLAY_VERIFICATION_CHECKLIST_2026-11-16.md), [subsystem test matrix](docs/GAMEPLAY_SUBSYSTEMS_AUDIT_TEST_MATRIX_2026-11-16.md), [stability worklog](STABILITY_WORKLOG.md), and [open issues](https://github.com/xrizonx/evemu_Crucible/issues).

### A. Distribution, startup, connection and character/session integrity
- [ ] **P0 ENV-01** — Re-run Linux GCC, Windows UCRT64, Docker and MariaDB seed jobs on the **final merged SHA**; record artifact links, CTest inventory/results and checksum verification.
- [ ] **P0 ENV-02** — Clean-install Windows ZIP and Linux/Docker package with bundled DLLs/SQL/config; start and stop the server three times without a missing dependency, malformed XML or unexpected DB migration.
- [ ] **P0 ENV-03** — Upgrade a copied existing database using `status`, `up -dryrun` and reviewed migrations; compare entity, character, market, wallet and job counts and verify restore procedure. Never use historical `install` or `seed -force` against a populated production DB.
- [ ] **P0 LOGIN-01** — Connect a compatible **Crucible-era** client, create/login two accounts, undock/dock, character-switch, disconnect/reconnect and relog with consistent session and inventory state.
- [ ] **P0 LOGIN-02** — Load stations with agent thumbnail URLs (including missing/old 50px portrait caches); verify no station-load crash, broken HTTP response or unsafe redirect; capture client dump and image-server trace if it fails ([#48](https://github.com/xrizonx/evemu_Crucible/issues/48)).
- [ ] **P0 LOGIN-03** — Open/reopen ship cargo (including Machariel), item hangar and corporate hangar before/after undock, dock, ship swap and relog; verify quantities, IDs and window refresh ([#48](https://github.com/xrizonx/evemu_Crucible/issues/48)).
- [ ] **P1 LOGIN-04** — Test client image request bursts and absent Character, Corporation, Alliance and Type images; check correct status, caching and no client crash.
- [ ] **P1 LOGIN-05** — Test a forced server restart during session change; require a clean relog and no duplicate ships/entities or stale character-online state.

### B. Destiny movement, warp, gates and docking
- [ ] **P0 MOVE-01** — Execute at least 20 consecutive manual warps from different grids/hulls; verify **visible warp tunnel**, arrival position and motion state on client and server, with packet ordering captured on any miss ([#45](https://github.com/xrizonx/evemu_Crucible/issues/45)).
- [ ] **P0 MOVE-02** — Verify warp-to-zero on gates and stations, jump/dock at the 2,500m interaction limit, and correct separation for small, large and capital radii; test manual and autopilot approach.
- [ ] **P0 MOVE-03** — Warp immediately after jump/undock, while aligning, with drones out or after stopping; verify restrictions, ship velocity, cloak/invulnerability and no teleport or invisible ship.
- [ ] **P0 MOVE-04** — With a second observer client, check bubble membership, visible warp effects, grid transitions and no ghost/double balls after repeated warp or system jump ([#45](https://github.com/xrizonx/evemu_Crucible/issues/45)).
- [ ] **P1 MOVE-05** — Approach, keep-at-range, orbit and stop on stations, ships, wrecks and asteroids; verify stable standoff, no overshoot/NaN and no repeated Destiny errors.
- [ ] **P1 MOVE-06** — Repeat autopilot across several gates and docking destinations; verify timer handling, session transitions and post-jump destination rather than just successful teleport.
- [ ] **P1 MOVE-07** — Test tackle, warp scramblers and interrupted pre-warp alignment; confirm supported Crucible client behavior without inventing newer-client mechanics.

### C. NPC AI, targeting, drones, combat and weapons
- [ ] **P0 COMBAT-01** — Belt/anomaly NPC spawns across security tiers, faction/type selection and respawn persistence; verify orbit, lock, aggression, damage and loot without stale-entity crashes.
- [ ] **P0 COMBAT-02** — Lock/unlock multiple targets, break locks on death/warp and exercise target painters/webs/ECM during dock and jump; monitor target-manager safety and historic crash regressions.
- [ ] **P0 COMBAT-03** — Test turret optimal, falloff and tracking versus angular/radial motion; verify in-range/out-of-range hits and damage calculation against comparable hulls.
- [ ] **P0 COMBAT-04** — Test missile explosion radius/velocity, flight and damage; exercise defenders, smartbomb area effect, bomb detonation and invalid/dead target handling.
- [ ] **P0 DRONE-01** — Combat drone launch, engage, focus-fire, passive/aggressive state, return, scoop and disconnect/relog; no stuck/orphan drones or double damage.
- [ ] **P0 DRONE-02** — Mining drone approach, orbit, harvest, return and unload to correct ship hold, from local and **AU-scale overview distances**; reconcile asteroid quantity and ship inventory ([#48](https://github.com/xrizonx/evemu_Crucible/issues/48)).
- [ ] **P0 DRONE-03** — Interrupt mining via recall, new order, pilot movement, asteroid depletion, warp, lost ship or drone destruction; no duplicate/lost ore or infinite mining timer ([#48](https://github.com/xrizonx/evemu_Crucible/issues/48)).
- [ ] **P1 COMBAT-05** — Test remote shield/armor transfers, tractor beam ownership/range, looting and salvaging across fleet/corporation/unauthorized targets.
- [ ] **P1 COMBAT-06** — Confirm module online/offline/reload/capacitor effects and invalid fitted-item or missing-target safeguards; include server restart and long fights.

### D. Missions, exploration and rewards
- [ ] **P0 MISSION-01** — Accept, journal-display, expire/fail/reaccept and complete normal security and courier missions; verify objectives only visible to authorized character.
- [ ] **P0 MISSION-02** — Run Blockade mission: expected cruiser/battlecruiser fleet, 12 target kills, warp-in/out persistence, journal completion and **one** ISK/standing payout.
- [ ] **P0 MISSION-03** — Generate DED/Officer expedition: targetable enemies (excluding known invalid Grigori), combat completion, boss loot/wreck contents and pocket cleanup.
- [ ] **P0 MISSION-04** — Disconnect or restart mid-mission and relog; verify mission offer/item/dungeon persistence, expiry and no duplicated mission reward.
- [ ] **P1 MISSION-05** — Validate mission standings, LP, faction reward, reward items and level-specific agent availability versus Crucible rules.
- [ ] **P1 MISSION-06** — Scan signatures/anomalies, bookmark and re-enter a dungeon; verify scanner cone geometry, repeat visits and safe cleanup after reset.

### E. Player and NPC market, 100M mineral/ore buys and wallet integrity
- [ ] **P0 MARKET-01** — On a **staging snapshot**, audit seeded mineral group 18/ore category 25 with [read-only 100M audit SQL](sql/diagnostics/2026-11-19-mineral-ore-npc-buy-100m-audit.sql): published types, station coverage, up-to-100M volume, 10T ISK cap, min volume and unique orders.
- [ ] **P0 MARKET-02** — With recorded player ISK/items, sell partial/full stacks into static NPC bids and verify price = stored order price, exactly-once wallet credit, consumed items and remaining volume/escrow ([#46](https://github.com/xrizonx/evemu_Crucible/issues/46)).
- [ ] **P0 MARKET-03** — Buy and sell player orders with two clients, corporation wallets, insufficient funds/space, permissions and invalid quantities; reconcile tax/fees and no duplicate debit/credit.
- [ ] **P0 MARKET-04** — Fault-inject DB failure/restart at each settlement step **only in isolated MariaDB staging**; confirm order, wallet, inventory and history commit atomically or roll back and retry safely. This is an **open architecture risk**, not a resolved fix ([#46](https://github.com/xrizonx/evemu_Crucible/issues/46)).
- [ ] **P0 MARKET-05** — Verify seed idempotence and player-order preservation with an isolated fixture. Review `seed -buy-only -force -dryrun` and a verified backup **before** any intentionally forced static-NPC-buy refresh; never reseed live data for testing.
- [ ] **P1 MARKET-06** — Verify normal Trader Joe/NPC sells versus static buys, baseline 100%-base-price policy, correct displayed bid/ask results and no stock depletion/negative escrow.
- [ ] **P1 MARKET-07** — Measure large-catalog market open/search/order latency, page and cache behavior, SQL `EXPLAIN` plans, index usage and regional seed load time; record before/after timings.

### F. Sovereignty, IHub upgrades, POS and structures
- [ ] **P0 SOV-01** — Claim/unclaim sovereignty via TCU, restart, and check ownership, FILETIME persistence, notification and station/system status.
- [ ] **P0 SOV-02** — Validate SBU distinct-gate counting, online/offline state, jump gates, bridge links, permission boundaries and structure cleanup.
- [ ] **P0 SOV-03** — Confirm `/sovindex5` is restricted to authorized DEV/ADMIN roles and does not permanently replace genuine activity-based progression; check override persistence/clearance ([#49](https://github.com/xrizonx/evemu_Crucible/issues/49)).
- [ ] **P0 SOV-04** — Install/uninstall IHub upgrades across required military/industrial levels, restart and confirm upgrade flags **and actual unlockable services**, not UI-only availability ([#49](https://github.com/xrizonx/evemu_Crucible/issues/49)).
- [ ] **P0 POS-01** — Anchor/online/offline a large tower, verify forcefield is **visible in client**, has correct permissions/range and survives restart ([#49](https://github.com/xrizonx/evemu_Crucible/issues/49)).
- [ ] **P1 POS-02** — Exercise silos, storage, corp ownership, tower fuel, structures, anchoring/unanchoring and safe access denial across characters.
- [ ] **P1 POS-03** — Test POS laboratory/array job line discoverability separately from actual job creation/delivery; incomplete POS job support remains tracked ([#47](https://github.com/xrizonx/evemu_Crucible/issues/47), [#49](https://github.com/xrizonx/evemu_Crucible/issues/49)).

### G. Manufacturing, research, blueprint copies and invention
- [ ] **P0 IND-01** — Install/deliver/cancel station manufacturing jobs; verify materials, BPO/BPC owner/location, run count, output and wallet charges.
- [ ] **P0 IND-02** — Run BPO ME/PE research and copying across normal/capped levels, skills and run boundaries; verify timing, persistence and actual returned blueprint stats.
- [ ] **P0 IND-03** — Force failure after wallet charge, material reservation, job creation and delivery **on disposable staging**; prove rollback or document non-atomic behavior ([#47](https://github.com/xrizonx/evemu_Crucible/issues/47)).
- [ ] **P0 IND-04** — Reject invalid runs/materials/blueprint access/remote or corp permissions without money, input, blueprint or job loss.
- [ ] **P0 IND-05** — Confirm invention still **rejects before charging** and never produces fictitious T2 output; real invention stays **unimplemented**, not a passed feature ([#47](https://github.com/xrizonx/evemu_Crucible/issues/47)).
- [ ] **P1 IND-06** — Verify offline/restart job recovery, overlapping job lines, slot capacity and POS/remote jobs **only after** their missing transaction-safe lifecycle is implemented ([#47](https://github.com/xrizonx/evemu_Crucible/issues/47)).

### H. Inventory, corporation, contracts, mail and permissions
- [ ] **P0 ITEM-01** — Split, merge, assemble, package, fit, move and transfer stacks/containers across station and ship cargo, with correct item IDs, quantities and owner flags.
- [ ] **P0 ITEM-02** — Restart immediately after inventory mutation; compare `entity`, `entity_attributes` and item/cargo ownership with before/after snapshots; no disappearing or duplicated items.
- [ ] **P0 ITEM-03** — Test newly guarded startup cleanup in staging: stored scanner/survey probes **outside** 30,000,000–32,000,000 location range must retain their attributes; in-space stale probes and unreferenced wormholes should be cleaned, referenced wormholes preserved.
- [ ] **P1 ITEM-04** — Test entity cleanup with a NULL signature item ID (if staging schema allows it); orphan detection must not be suppressed by SQL `NOT IN` NULL semantics.
- [ ] **P1 CORP-01** — Test corp office rent/visibility, wallet journal/tax, member roles and POS/inventory access before/after station relog and restart.
- [ ] **P1 SOCIAL-01** — Mail send, read/unread, recipient-only deletion, list administration and invalid ID handling; no cross-character access or corruption.
- [ ] **P1 SOCIAL-02** — Contracts create/accept/cancel/complete with item movement, correct assignees, owner checks, escrow and private visibility; verify failure paths do not lose assets.
- [ ] **P1 SOCIAL-03** — Fleet invites, standing changes, multi-client chat updates and permission checks over reconnect and session change.

### I. SQL correctness, resilience, observability and performance
- [ ] **P0 SQL-01** — Compare `evedbtool status` to applied migration files; review historical destructive market migrations (including old `TRUNCATE mktOrders` changes) without replaying them on populated data.
- [ ] **P0 SQL-02** — Verify migration/seed scripts on a **fresh disposable schema**, then upgrade a populated staging copy; compare row counts, constraints, order ownership and seed migration markers.
- [ ] **P0 SQL-03** — Verify no orphaned inventory attributes, stranded industry jobs, duplicated market rows or negative player balances after controlled SQL failure/restart scenarios; record actual repair needs rather than auto-deleting rows.
- [ ] **P1 SQL-04** — Check query plans/indexes for market order search, settlement, player inventory, corporation access, drone updates and industry jobs under realistic table sizes.
- [ ] **P1 SQL-05** — Audit float/decimal price rounding, overflow/unsigned range, escrow caps and NULL join/subquery handling; test large quantities and currency boundaries.
- [ ] **P1 PERF-01** — Profile startup static-data load, market/material price bulk loading, large corporation hangars, numerous system entities and DB connection churn.
- [ ] **P1 PERF-02** — Run multi-hour soak with two+ clients, repeated gate/warp, NPC combat, drone cycles, mission turn-in and market activity; capture memory, CPU, DB connections and log/error trends.
- [ ] **P2 PERF-03** — Review remaining per-row SQL writes (material price updates, industry/job and market settlement paths) for safe batching/transaction boundaries; benchmark before optimizing.
- [ ] **P2 PERF-04** — Repeat soak and migration/seed checks on **both** Windows UCRT64 and Linux/Docker, including graceful shutdown and abrupt staging-only restart.

### Completion and filing results

For each ID record: `test ID | PASS/FAIL/BLOCKED/NOT RUN | UTC date | server SHA | Crucible client build | OS | MariaDB/schema | reproducible steps | expected/actual | artifact/log/SQL evidence | issue URL`. File independently reproducible failures in [GitHub Issues](https://github.com/xrizonx/evemu_Crucible/issues/new), linking existing [#45–#49](https://github.com/xrizonx/evemu_Crucible/issues) when applicable; do not close a tracker issue without validated resolution and rerun. The source-level cleanup regression check lives in `tools/verify_subsystems_audit.py`; it is **not** a substitute for ITEM-03/04 database-integrity testing.

## Further information and legal

[Patch notes](PATCH_NOTES.md) · [Stability/CI](STABILITY_WORKLOG.md) · [Upstream issue triage](ISSUE_TRIAGE.md) · [Database README](sql/README.md) · [Mission guide](docs/missions/MIGRATION_GUIDE.md) · [EVEmu upstream](https://evemu.dev/)

**EVEmu is an educational project.** Its primary purpose is learning and teaching C++, database and network-service development. It is not intended for public servers, is not warranted for production use and does not guarantee bug-free gameplay. Upstream EVEmu copyright and GNU LGPL licensing terms continue to apply.
