# ShelfSuite Cafe Lock F1 compatibility update

Use this review package on a spare Windows PC/VM first. It updates adaptive tab
backgrounds in your existing **Shelf Suite**, not a second skin. The normal
Cafe Lock installer does not install this update. Rainmeter itself is unchanged.

## Before starting

1. Download `ShelfSuite-Cafe-Lock-F1-Compatibility.zip` and its `.zip.sha256`
   from the same verified workflow run. Compare the ZIP's SHA-256 with the file.
   These hashes show consistency, not a digital signature or publisher identity.
2. If you want a whole-skin personal backup, make one yourself. The updater
   never reads, hashes or copies `config.lua` or backs up your whole skin.
3. Extract the ZIP to a new local folder **outside** the installed Shelf Suite.
   Run `Update-ShelfSuite.cmd` normally; do not use Run as administrator.
   It needs built-in Windows PowerShell 5.1, no downloads or extra dependencies.
4. Close Rainmeter normally before updating. The updater will refuse while
   Rainmeter runs or if it cannot check. It never kills or starts Rainmeter.

If Windows blocks scripts, the launcher reports failure. Your PC administrator
must decide what the machine's script policy allows. This package never changes
persistent policy, bypasses it in its launcher, or asks for elevation.

## Updating

Choose the existing directory named `Shelf Suite`. Read the resolved location
and the proposed file list. Cancel changes nothing. After OK, the updater makes
a uniquely timestamped backup **beside** Shelf Suite and verifies every original
before changing installed files. Keep the displayed backup location.

Only `@Resources/ShelfEngine.lua`, `@Resources/Variables.inc`, and recognized
`Shelf<number>/Shelf.ini` files can change. The INI change is only insertion of
`DynamicWindowSize=1`; existing settings and theme choices are retained.
Only the pinned v2.1 stock/trusted generated templates and four existing themes
(DeepOcean, Forest, Terracotta, Obsidian) are recognized. Supported source text
is UTF-8 without BOM with consistent LF or CRLF newlines. Customizations, missing
or busy files, links, network/redirected locations and ambiguous templates are
refused rather than repaired. One invalid shelf refuses the entire update.

Positions, icons, themes, passwords and launcher configurations are untouched.
The updater never opens `config.lua`. Already-updated installations need no
action and get no additional backup. Known partial installations can complete;
unrecognized mixtures are refused.

## Failure and manual recovery

This is not one atomic operation across all files. Do not interrupt it.
On failure it restores only this run's files that still exactly match its own
written output, after verifying their backups. Unexpected edits are never
silently overwritten. A failure dialog lists files needing manual inspection.
Backups are retained on success and failure; interrupted runs are not replayed.

To restore manually:

1. Close Rainmeter normally and open the reported backup.
2. Read `RESTORE.txt` and `recovery.json`. Inspect any unexpected installed edits
   before overwriting anything; keep those edits separately if needed.
3. Copy only the listed files from the backup's `original` subfolder to the
   **same relative paths** in the existing Shelf Suite. Do not replace the whole
   skin, copy launcher configuration, or use `staged`/`restore` as originals.
4. Start Rainmeter yourself and verify the restored layout. Keep the backup.

If the backup is unreadable or does not match the recorded original hashes,
stop; do not use it blindly. No automated repair or recovery replay is supplied.

## Spare-PC acceptance

Check running-Rainmeter refusal, folder/confirmation cancellation, successful
update and backup, then start Rainmeter yourself. Existing positions, launcher
items, icons and themes should remain unchanged. Check short tab names, long
SCHOOL/WORK names and combined long tabs for fitted backgrounds/no overlap;
check settings gear, hover/selection and a newly created shelf. Check launcher
use in Locked Mode and editing/movement only in Maintenance Mode. Close
Rainmeter and repeat the updater: expect no action/no new backup. Test refusal
on a customized target using a disposable copy, and test manual restore.

## Provenance and licence

ShelfSuite v2.1: MartinSantosT/ShelfSuite, revision
`d4f186ba0b5c262c7559b80841132f5fd3884f3c`.
F1: Rainmeter Cafe Lock verified revision
`c6ce82ab7c2e9f901b01c712a0efd22bdee6d5c9` and its unchanged committed adaptive-tab
patch. See `manifest.json` for payload/recognition hashes and patch digest.
ShelfSuite copyright (c) 2025 Martin Santos; MIT licence is included as
`LICENSE-ShelfSuite.txt`. This is a separate compatibility review artifact,
not an installer, merged release or deployment to your active PC.
