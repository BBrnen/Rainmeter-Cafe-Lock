# ShelfSuite F1 native finalization design

Date: 2026-10-01
Branch: `feature/f1-installer-finalization`
Stable F1 baseline: `v4.5.26.3894-cafe-lock-f1.0` at
`c6ce82ab7c2e9f901b01c712a0efd22bdee6d5c9`

## Purpose

Make the verified ShelfSuite F1 adaptive-tab update practical to deploy on café
PCs without PowerShell, a second updater program, a second ShelfSuite copy, or
automatic edits during setup. The existing Rainmeter Cafe Lock installer will
carry the approved F1 compatibility data. The normal-user Rainmeter process
will offer one explicit, password-authorized compatibility action in
Maintenance Mode.

F1 remains exactly the adaptive-tab work already verified at the stable tag.
This finalization adds a guarded way to apply its five approved ShelfSuite file
changes to an existing installation. It does not change the tab-sizing
algorithm, F2, Cafe Lock authorization, password storage, launcher behavior,
or the unfinished PowerShell delivery experiment.

## Owner experience

1. The owner installs the normal Cafe Lock setup. Setup updates the protected
   program files only; it never examines or edits ShelfSuite in the user's
   skin directory.
2. Rainmeter starts under the normal café Windows account and remains Locked.
3. The owner unlocks through the existing password dialog, opens **Manage**,
   then the existing **Security / Cafe Lock settings** area.
4. That area contains **Apply ShelfSuite F1 compatibility**. The button is
   absent and unusable while Locked.
5. The native dialog inspects the ShelfSuite location obtained from
   `GetRainmeter().GetSkinPath()` plus `Shelf Suite`. It never assumes a
   Documents path or a Windows username.
6. The dialog reports the resolved path, files needing changes, files already
   compatible, and the proposed timestamped backup location. It requires an
   explicit **Apply** confirmation.
7. On success it lists changed files and tells the owner to reload the affected
   ShelfSuite skins or restart Rainmeter normally. It does not restart,
   refresh, close, move, or otherwise alter skins itself.
8. On refusal it names only the affected path and safe reason. It does not show
   configuration contents, launcher data, icons, or passwords. On an already
   compatible installation it reports that no changes or backup are needed.

The manual F1 guide remains useful only as historical acceptance evidence. It
is not called, run, or installed by this design.

## Why this is the smallest safe delivery route

The existing NSIS setup is elevated and intentionally protects the program
directory. It must not directly edit the normal user's ShelfSuite directory:
doing so risks choosing the wrong user profile and would mix application setup
with user-owned data.

The running Rainmeter process already has the correct user's configured skin
path and runs at normal-user integrity. Its existing Maintenance Mode provides
the required authorization boundary. A narrowly scoped native compatibility
component therefore avoids PowerShell's Restricted policy, avoids a separate
unsigned updater executable, and does not need a new dependency, subscription,
or signing service.

The main setup remains unsigned unless the project later chooses signing. An
unsigned application can still receive Windows reputation warnings; this design
does not bypass a warning or weaken Windows security. It removes only the
additional PowerShell-policy obstacle.

## Packaged F1 data and provenance

The installer will place a small, read-only F1 payload bundle inside its
protected program directory. It contains only the two approved shared output
files and machine-readable recognition data; it does not contain a complete
ShelfSuite skin, a user profile, themes, icons, or `config.lua`.

The bundle is derived solely from:

- ShelfSuite v2.1 base `d4f186ba0b5c262c7559b80841132f5fd3884f3c`;
- F1 patch `ThirdParty/ShelfSuite/patches/0001-cafe-lock-adaptive-tabs.patch`;
- stable F1 baseline `c6ce82ab7c2e9f901b01c712a0efd22bdee6d5c9`.

Its recognition data records exact SHA-256 input and output bytes. It accepts
only the three pinned stock Shelf INI templates and the trusted Cafe Lock
generated template; each may use DeepOcean, Forest, Terracotta, or Obsidian;
each may use consistent LF or CRLF line endings; and each may already have the
approved `DynamicWindowSize=1` line. The folder number is never treated as
proof of a template identity.

The build and CI tests independently apply the unchanged F1 patch to the exact
pinned upstream source, then compare every bundled byte and recognition entry.
The package retains ShelfSuite's MIT licence and attribution. The existing
Cafe Lock source archive retains the implementation sources necessary to audit
the bundle.

## Authorization and interface boundary

The button lives in `DialogManage::TabSettings`, beside the existing Cafe Lock
security controls. The UI only creates/enables it in Maintenance Mode.

The native operation has a second, independent `CafeLock::IsLocked()` check at
its public entry point and at every phase that can write data. The dialog may
display a read-only refusal while Locked, but it cannot inspect or prepare an
update. Lock Now, a password-dialog close, Rainmeter shutdown, or session
revocation cancels the operation before any later write.

No tray command, bang, command-line flag, registry value, skin action,
environment variable, WebView message, file toggle, or external process route
will call the compatibility entry point. A forged `WM_COMMAND` aimed at the
Manage window still reaches the second Maintenance Mode check and is refused.
The feature does not add an unlock path or make changes to password handling.

## Read-only inspection and allowed changes

The service roots all paths at the actual configured skin directory and then
at its literal child `Shelf Suite`. It accepts only an ordinary local fixed-disk
path and rejects missing paths, aliases, network/removable locations,
reparse-point ancestors, links, hard links, redirected files, and unsafe path
names.

It reads only these declared files:

```text
Shelf Suite\@Resources\ShelfEngine.lua
Shelf Suite\@Resources\Variables.inc
Shelf Suite\Shelf<number>\Shelf.ini
```

`Shelf<number>` means a direct child whose name exactly matches `Shelf` plus a
positive decimal number. The service examines every such direct child. A
missing, locked, unreadable, oversized, duplicate, ambiguous, unsupported, or
customized target refuses the **entire** update before any backup or write.
There is no best-effort repair and no partial update around a bad shelf.

After all input hashes match the approved catalog, the only writes are:

```text
replace @Resources\ShelfEngine.lua with the approved F1 bytes
replace @Resources\Variables.inc with the approved F1 bytes
insert DynamicWindowSize=1 immediately after AccurateText=1 in a recognized Shelf.ini that lacks it
```

The last operation preserves every other byte, including the recognized theme
choice and original LF/CRLF form. A recognized file already at its F1 output
hash is left untouched. A known mixture of approved old/F1 files can be
completed; an installation entirely at approved F1 hashes reports no action
and creates no backup.

The operation never opens, hashes, copies, rewrites, migrates, uploads, or
parses `config.lua`. It also leaves icons, themes, launcher data, passwords,
desktop positions, Rainmeter settings, and unrelated files untouched.

## Backups, application, recovery, and live skins

After successful inspection and owner confirmation, the normal-user process
creates a uniquely named sibling folder beside `Shelf Suite`, for example
`Shelf Suite-F1-Backup-20261001-153000-<random>`. It copies only files that
will change, preserving their relative paths. Every backup is flushed and
re-hashed against the approved original before the first replacement.

Task 3 safety adjustment approved after `a937a47b`: the service holds verified
targets by Windows file handles denying outside writes/renames. It writes and
hashes staged output, preserves the target's appropriate permissions and
attributes, and flushes verified backups and recovery/phase records before
moving any original. It moves originals and staged replacements by their
verified handles, with overwrite disabled at every destination, then verifies
the placed output. Recovery follows the same no-overwrite rules. Records and
plain-language restore instructions contain only declared paths, hashes and
phases, never user configuration contents.

An individual replacement comprises two moves, not one atomic replacement.
Interruption after moving an original may leave that target temporarily absent;
verified backup/holding materials and the flushed phase record support manual
restoration. A competing file is never overwritten, even during recovery.
Maintenance authorization is checked before every mutation. Revocation between
the moves stops placement and recovery writes and explicitly reports incomplete
work with retained recovery materials; it never silently bypasses authorization.
Windows tests must cover this gap, process interruption, competing files, busy
targets, preserved attributes/DACLs and standard-user access.

The operation is not falsely described as one atomic transaction across all
files. If a later replacement fails, it restores only files written by this
run that still have the exact expected F1 output and unchanged identity. It
never overwrites an unexpected external edit. If recovery is unsafe or fails,
it retains the verified backup and identifies only the files needing manual
restoration.

Rainmeter itself is already running to provide this Maintenance-only action.
The service does not close the process or forcibly unload skins. It requires
exclusive/safe target access; a busy target causes a no-write refusal. On
success it tells the owner to reload the affected ShelfSuite skins through
normal maintenance controls or restart Rainmeter normally. This applies the
new files without silently changing layout, position, or running-skin state.

## Installer behavior

The existing NSIS installer remains responsible for the protected Cafe Lock
program directory, startup shortcut, and normal application upgrades. It gains
only the read-only native F1 bundle and its licence/provenance notice. It does
not detect, copy, scan, back up, or modify ShelfSuite during installation,
upgrade, uninstall, or startup.

The generated uninstall manifest removes the bundled program files it owns.
It never removes the user's ShelfSuite folder or F1 backup folder. A normal
installer upgrade preserves both the user profile and ShelfSuite unchanged
until the owner deliberately uses the Maintenance Mode button.

## Implementation boundaries

Likely new native files belong under `Library/CafeShelf/` and have one purpose:
strict F1 recognition, backup, application, and bounded recovery. A small
declaration belongs in `Library/CafeLock.h` or a focused compatibility header.
The existing `Library/CafeLock.cpp` and `Library/DialogManage.cpp/.h` gain only
the authenticated entry point and button. Resource/build wiring adds the
verified F1 bundle to `Build/CafeInstaller/Package.ps1` and
`Build/CafeInstaller/Installer.nsi`.

The static payload/recognition source is kept under the existing
`ThirdParty/ShelfSuite/` provenance area, not generated build output. CI is the
authority that verifies it against the pinned upstream checkout and unchanged
patch. Do not merge the PowerShell delivery branch into this branch; its
runtime scripts and UI are not part of this design.

No F1 sizing source, compatibility patch, password code, locking code, normal
launcher code, ShelfSuite editor protocol, external skin API, or F2 source is
refactored or changed for convenience.

## Verification plan

Tests use only disposable fixture roots and standard-user tokens. They must
prove the following before any release candidate is considered ready:

1. The bundled shared payload and full recognition catalog exactly match the
   F1 patch applied to the pinned ShelfSuite revision.
2. Each approved stock/generated template, known theme, LF/CRLF form, and
   known partially updated combination has the expected plan and output bytes.
3. A fully compatible installation produces no writes and no backup.
4. A custom, unknown, missing, locked, oversized, linked, redirected, or
   ambiguous target refuses the whole update before any backup or write.
5. A sentinel `config.lua`, position, icon, theme, and launcher file remains
   unaccessed and byte-identical on every success and failure path.
6. Backup creation/verification occurs before the first replacement. Only
   changed target files are backed up. Output bytes are verified after each
   replacement.
7. Injected write, disk, or replacement failures either restore only verified
   just-written targets or retain a verified backup with correct manual
   recovery instructions. An outside edit is never overwritten by rollback.
8. A standard user can perform the approved operation using normal file
   permissions; no elevation, PowerShell, separate updater, or policy change
   is involved.
9. The button and native service refuse while Locked, during revocation, and
   through direct/forged dialog command paths. Existing Maintenance Mode,
   password, editor, launcher, and Lock Now regression tests continue to pass.
10. Installer fresh-install, installer-upgrade, and uninstall tests prove the
    protected F1 bundle is present/removable as program data while all user
    ShelfSuite data and backups remain untouched.
11. The full Windows x64 workflow passes at the final revision, including
    pinned ShelfSuite F1 checks, Café Lock policy/password checks, standard-user
    runtime tests, editor tests, and installer tests.

Manual spare-PC/VM acceptance follows the normal café workflow: install,
unlock, preview, cancel, apply, inspect the backup, reload/restart normally,
check short and long tabs, test an already-compatible rerun, confirm Locked
Mode denial, and restore from a backup on a disposable copy. It also checks
existing positions, themes, icons, launcher items, and password behavior.

## Scope and release gate

This is a focused F1 finalization feature. It is deliberately not a general
ShelfSuite updater, automatic installer migration, public distribution system,
or F2 foundation. A full workflow pass and successful spare-PC/VM acceptance
are required before an F1 release decision. Merge, release creation, and
deployment remain separate owner approvals.
