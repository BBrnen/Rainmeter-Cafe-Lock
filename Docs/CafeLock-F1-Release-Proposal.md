# Rainmeter Cafe Lock F1 — final release proposal

Prepared 2026-10-02. Status: owner manual acceptance received; release preparation
only. Nothing is merged, tagged, published or deployed by this proposal.

## The revision we are preserving

- Accepted installer implementation: `66f1002f47ac05a0bca9beee322f8dca03796056`.
- Final verification record: `ecbc94537feb4068f7f96fd10b980ac0cce3a9dc`,
  `Docs/superpowers/plans/2026-10-01-shelfsuite-f1-native-finalization-verification.md`.
- Branch: `feature/f1-installer-finalization`.
- Existing stable tag `v4.5.26.3894-cafe-lock-f1.0` remains unchanged at
  `c6ce82ab7c2e9f901b01c712a0efd22bdee6d5c9`.
- Proposed NEW tag: **`v4.5.26.3894-cafe-lock-f1.1`**, pointing to the accepted
  implementation `66f1002f`, not a new rebuild or a later documentation commit.
  This tag has not been created.
- Unchanged adaptive-tab patch SHA-256:
  `6C4913AF57F1B8542D287C72B08E3EE840275CAE51B7F7A96DF40707891956E9`.

The owner reports successful hands-on testing on spare Windows computers,
including an original ShelfSuite installation, with no bugs or errors found,
and approves entering final release preparation. This is owner-reported
acceptance, not an automated claim about every individual PC/test scenario.

The full Windows workflow passed at the accepted implementation:
https://github.com/BBrnen/Rainmeter-Cafe-Lock/actions/runs/36898837897

All four jobs pass: x64 build/runtime/installer; 318 editor checks; native F1
inspection, authorization and recovery; and prerequisite policy. Attempt 1 had
an editor-open timeout; the unchanged rerun passed all checks. Independent
whole-branch review findings were addressed and verified before acceptance.
No product code or packaging changes are required for this release preparation.

A fresh read-only release-documentation review found no critical, important or
minor issues in provenance, instructions or rollout safety. It did not repeat
the completed implementation review or invent individual manual test results.

## Pull request and approval sequence

Prepare one final **draft** PR from `feature/f1-installer-finalization` to `main`.
`main` is an ancestor of this branch. The PR necessarily includes the previously
accepted guarded ShelfSuite editor work from PR #6 and the adaptive-tab work
represented by PR #7, because neither is merged into main. Describe that complete
release behavior in the PR; do not pretend its diff contains only the new button.
Do not merge, close, retarget or rewrite PR #6/#7 as part of preparation.

After the owner's next approval, the proposed sequence is:

1. Check the final PR has no unexpected source changes or merge conflict and
   inspect required checks. Any changed executable/package revision needs fresh
   complete Windows verification and a separate acceptance decision.
2. Merge the approved PR while preserving history. Do not squash/rewrite away
   the accepted implementation or move the existing stable tag. Check the merged
   result before publishing; this proposal does not claim a future merge is tested.
3. Create the new annotated tag at the exact accepted implementation above.
4. Publish the accepted installer bytes, their matching source archive and
   checksums as a release under that new tag. Do not rebuild or relabel another
   artifact as the accepted installer. If the original artifact is unavailable,
   stop: a different build/hash needs separate verification and approval.
5. Begin the one-PC pilot below only after deployment is approved.

This document authorizes none of those future actions. The owner reviews the
proposal and gives the next approval first.

## Exact files proposed for publication

Accepted Actions artifact: **Rainmeter-Cafe-Lock-stage6-installer-x64**,
artifact ID `11181855337`, from run `36898837897`, attempt 2. It is currently
available and expires **2026-10-31 17:33:33 UTC** under Actions retention.
Keep a verified local copy before that date; no release has been published yet.

Extract the artifact ZIP in Windows Explorer. Its publication files are:

| File | SHA-256 |
| --- | --- |
| Rainmeter-Cafe-Lock-4.5.26.3894-x64-Setup.exe | CC3B139EC8DDD05D8A03F1B45F2D983ECDA6EA2E85C00C13D59853197A054B59 |
| Rainmeter-Cafe-Lock-source.zip | EB8D3FD1661243D268DA9AA4273FD48AFC05E73C5D6735AC6F2D5087322BF5A6 |
| README.md | 945D7A788E1D74EA260C1513E697D430E5F7B85891C80E208B324DDB8B715086 |
| WebView2-runtime-manifest.json | 0A23A33B8DC5C87C89591246839E71BA6244B5B647A9AA4CCC6E7D615477E3BE |

Include the extracted `SHA256SUMS.txt` alongside them. These are the file hashes
emitted by the verified build, distinct from the enclosing GitHub artifact ZIP
digest `e15c4493ff73a38306daf2f5b46d574f032dd9053969dbe9eef9229bc2492b75`.
The source archive is the source matching the installer; later proposal/evidence
documentation on the branch is not substituted for that archive.

The installer filename and internal Rainmeter version stay unchanged. Identify
this F1.1 release by its new Git tag and exact checksum, not by filename alone.

The installer is unsigned. No signing purchase, subscription, new updater,
PowerShell installation script or security-policy change is proposed. If Windows
blocks the installer, stop and report it; do not disable or bypass protection.

## Proposed release notes

**Rainmeter Cafe Lock F1.1 — adaptive ShelfSuite tabs with native delivery**

Long ShelfSuite tab names automatically fit their colored backgrounds with
padding. Short names retain the 85 px minimum and normal layouts retain 480 px
shelf width. Following tabs do not overlap; the shelf and gear adjust only when
the combined tabs need more room. Fonts, colors, height, rounded appearance,
hover and selection behavior are preserved.

After password unlock, Manage > Settings provides **Apply ShelfSuite F1
compatibility**. It previews the existing installation, verifies exact supported
files, creates verified backups, and applies the approved compatibility changes
only after confirmation. Already-compatible installations require no changes or
new backup. Additional shelves are recognized by file contents, not their number.
Unexpected, customized, busy or unsafe files are refused rather than repaired.

Existing launcher/editor functionality and password/Locked Mode behavior remain
intact. Rainmeter starts locked at Windows sign-in and restart. Setup never
automatically modifies ShelfSuite. The compatibility action never accesses
`config.lua`, icons, themes, launcher data, passwords or saved positions.

F2/grid snapping and the unfinished PowerShell updater are **not included**.
ShelfSuite itself is not bundled as a second skin. The supported source remains
ShelfSuite v2.1 at `d4f186ba0b5c262c7559b80841132f5fd3884f3c`.

## Before installing on any cafe PC

1. Choose a quiet maintenance period. Begin with only one PC, not all computers.
2. Keep your existing personal backup or a complete system/VM snapshot. Keep the
   PC's previously working installer and its checksum for application rollback.
   The stable Git tag preserves source; it is not an executable or PC backup.
3. Record the PC name, intended Windows cafe account, current working installer,
   configured skin path and where your backups are stored. Do not open or edit
   `config.lua`. Keep password backups private; do not upload them to GitHub.
4. Download the exact artifact above, extract it, and check the Setup hash. In
   Command Prompt, use this read-only command with your actual extracted path:

   ```bat
   certutil -hashfile "C:\your extracted folder\Rainmeter-Cafe-Lock-4.5.26.3894-x64-Setup.exe" SHA256
   ```

   Compare all 64 characters with the Setup hash above. Stop on any mismatch.
   Retain this same verified package for every PC; do not use a newer untested run.

## Installation or upgrade, one PC at a time

1. In the intended cafe account, unlock the running Cafe Lock and use its normal
   Exit control. If ordinary Rainmeter is running, close it normally too. Never
   force-close a customer's session just to install.
2. Run the verified Setup.exe using its normal Windows installation permission.
   Setup installs into `C:\Program Files\Rainmeter Cafe Lock`. Installing/upgrading
   requires administrator permission; Rainmeter and F1 compatibility do not.
   Do not uninstall the working copy or delete profiles/skins as a first step.
3. Start Rainmeter Cafe Lock from the Start menu in the intended normal Windows
   cafe account, without Run as administrator. It must start Locked. Setup's
   automatic startup is at user sign-in, not before the Windows login screen.
4. For an upgrade, your existing Cafe Lock password/profile should remain. For
   a fresh account, choose Unlock / Enter Maintenance Mode and set its password
   before customer use. Each Windows account has its own verifier/profile.
5. Cafe Lock uses `%APPDATA%\Rainmeter Cafe Lock`; an ordinary Rainmeter profile
   is separate. If your shelves/profile are not present, stop and check the
   configured skin path/profile before proceeding. An earlier Cafe Lock profile
   can be migrated while Rainmeter is closed using the existing deployment guide,
   retaining CafeLock.ini. Do not reset a working password, create a second
   ShelfSuite copy or overwrite shelf data to make the installation appear ready.
6. Unlock, open Manage > Settings, and select Apply ShelfSuite F1 compatibility.
   It must use the actual configured skin directory and existing
   `Shelf Suite\@Resources`. Read the preview and expand Show file list.
7. If it says Already compatible, finish without rewriting files or making
   another backup. If it refuses, keep the installation intact, record the
   relative file/error and ask for help. Do not edit hashes or source files to
   bypass recognition. If changes are expected, you may Cancel to inspect safely,
   then reopen and explicitly confirm Apply.
8. On success, keep the complete reported `Shelf Suite-F1-Backup-...` folder.
   Reload affected skins yourself in Maintenance Mode, or exit/restart normally.
   The updater does not automatically reload or reposition loaded shelves.
9. Complete the pilot checks below. Select Lock Now before returning the PC to
   customer use. A restart/sign-in must also return to Locked Mode.

Optional WebView2 is the existing prerequisite for the integrated ShelfSuite
editor. Setup offers the bundled Microsoft prerequisite if missing; normal
launchers and the native F1 compatibility operation do not use the web editor.
Follow the existing deployment guide if the editor prerequisite needs attention;
do not add new tools or change Windows security policy for F1.

## Simple rollback and recovery

If everything works, keep the backups; no rollback is needed.

**Application rollback:** close Rainmeter normally in Maintenance Mode, then run
the PC's previously verified working Cafe Lock installer. Preserve the existing
profile and CafeLock.ini. Do not delete or uninstall user settings first. Restoring
program files does not undo separately applied ShelfSuite compatibility files.
If that installer/backup is unavailable, stop and use your system/VM recovery
plan; do not download an arbitrary build or move the stable Git tag.

**ShelfSuite file rollback:** close Rainmeter normally before restoring files.
Read RESTORE.txt in the compatibility backup. Restore only the listed changed
originals to the exact matching installation paths. Never copy a complete stock
skin over your existing installation. Do not touch config.lua, icons, themes,
launchers or positions. Preserve any unexpected current target separately before
manual replacement; ask for help when identities/hashes or paths are unclear.

**Incomplete operation:** do not reload affected shelves. Keep the entire backup,
PHASE.txt, RESTORE.txt, Holding originals and staged outputs. Lock Now stops later
writes, including automatic recovery; it does not silently complete an interrupted
update. Phase entries record intentions, so check actual paths before restoration.
Follow guided manual recovery rather than repeatedly pressing Apply.

## Staged cafe deployment checklist

### Stage A — one pilot cafe PC

- [ ] Owner has approved release/deployment; the exact accepted package is retained
      and its Setup checksum matches.
- [ ] Personal/system backup and previous working installer are available.
- [ ] Normal installation/upgrade completed without changing ShelfSuite automatically.
- [ ] Correct normal Windows cafe account is used; existing password/profile and
      original shelves are present.
- [ ] F1 preview/cancel and Apply or Already compatible produce the expected result.
- [ ] Short ONLINE/OFFLINE/INTERNET tabs retain their appearance. A long label such
      as SCHOOL/WORK fits, following tabs do not overlap, and the gear stays aligned.
- [ ] Existing icons, themes, launcher clicks, tabs, hover effects and shelf positions
      work as before. Maintenance editor/configurator remains usable.
- [ ] Lock Now blocks editing, movement and settings access; existing launchers work.
- [ ] Sign-out/sign-in, restart and shutdown behave normally. Automatic startup is
      Locked; Maintenance never persists across a restart.
- [ ] A normal operating period on this PC reveals no issue; record the outcome.

Stop rollout on any failure, preserve diagnostics/backups and use the recovery
instructions. Do not assume success on the spare PCs proves every cafe PC ready.

### Stage B — remaining cafe PCs

Only after the pilot passes, repeat the same preparation, installation and checks
on each remaining PC individually. Record PC/account, date, accepted Setup hash,
F1 result, backup path and pass/fail. Use the same accepted package throughout.
Keep each PC's backups and previous installer; stop the batch immediately if an
unexpected refusal, password/profile change, security block or malfunction occurs.

## Exclusions and current limitations

The experimental `feature/shelfsuite-f1-delivery` branch is not merged into this
branch. The installed native bundle contains only five allowlisted data/licence
files. No Update-ShelfSuite PowerShell/.cmd updater or separate maintenance helper
is shipped. CI/build PowerShell scripts in the source archive are development
tools, not customer installation steps. Existing Rainmeter snapping is unchanged;
F2/grid snapping is not implemented by this release.

Exact-hash refusal is intentional; custom/ambiguous or unsafe-permission source
files are not automatically repaired. Signing remains deferred. Cafe Lock retains
the existing application-management lock and user-permission model described in
CafeLock-Deployment.md; this preparation adds no Windows kiosk/security mechanism.

## Owner decision requested

Approve or revise the final draft PR, the proposed new tag at 66f1002f, these
release assets/notes, and the one-PC-first deployment procedure. Merging, tag
creation, release publication and actual deployment remain separate future
actions and will not happen automatically.
