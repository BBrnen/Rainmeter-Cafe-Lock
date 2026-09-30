# ShelfSuite F1 compatibility delivery design

Date: 2026-10-01
Branch: `feature/shelfsuite-f1-delivery`
Verified F1 base: `c6ce82ab7c2e9f901b01c712a0efd22bdee6d5c9`
Status: specification for owner review; implementation has not started.

## Purpose and authority

Produce `ShelfSuite-Cafe-Lock-F1-Compatibility.zip` so the owner can manually
test the verified adaptive tabs on a spare Windows PC or VM. Update the existing
`Shelf Suite` directory in place. Do not create a second skin or profile.

The approved contract is `Docs/CafeLock-ShelfSuite-F1-Delivery.md`. This design
adds delivery mechanics without changing F1, its patch, or Rainmeter behavior.
The owner approved conservative theme recognition and simple recovery:
recognize only verified templates, refuse ambiguity, back up only changed
files, and restore only when doing so cannot overwrite unexpected edits.

## Owner experience

1. Extract the ZIP outside the installed `Shelf Suite` directory. Run the
   supplied Update ShelfSuite launcher as the normal Windows user.
2. Read the short explanation and reminder to make a separate personal backup.
   The updater itself never copies the whole skin directory.
3. Choose the existing `Shelf Suite` directory through a native folder picker.
   Show its resolved location; require explicit confirmation before applying.
4. Close Rainmeter normally. The updater checks that no Rainmeter process is
   running and refuses if it cannot establish this. It never kills a process,
   unlocks Cafe Lock, or launches Rainmeter automatically.
5. Complete all validation before creating any backup, temporary file, or
   persistent log. Show the files proposed for modification, or an actionable
   failure identifying the affected path and reason. Cancellation changes
   nothing.
6. Create and verify a timestamped backup, apply the approved changes, and
   verify the resulting files. Show success and the backup location. The owner
   starts Rainmeter and performs the F1 manual acceptance checks.

Use built-in Windows PowerShell 5.1 and native Windows dialogs. Require no
administrator rights, downloads, Git installation, PowerShell 7, or extra
dependencies. If machine policy prevents script execution, report the problem
without changing persistent execution policy or machine settings.

## Package and provenance

The ZIP contains a guided updater script and launcher, the two approved shared
file payloads, an explicit verification manifest, recovery instructions, and
the upstream MIT licence and attribution. It contains no complete skin,
launcher configuration, icons, themes, or executable Rainmeter payload.

CI generates the payload only from ShelfSuite v2.1 upstream revision
`d4f186ba0b5c262c7559b80841132f5fd3884f3c` plus the unchanged committed
`ThirdParty/ShelfSuite/patches/0001-cafe-lock-adaptive-tabs.patch` at the verified
F1 base. The manifest records the upstream and F1 revisions, patch digest,
approved paths, recognized input fingerprints, and expected output hashes.
Preserve upstream licensing. Verify packaged bytes against the manifest before
using them. Publish the ZIP and its SHA-256 as review artifacts, not as a
release. Hashes establish consistency; they are not a digital signature.

The normal Cafe Lock installer remains separate. This branch does not change
Rainmeter binaries or automatically install this compatibility update.

## Conservative installation recognition

The selected root must be an existing local directory named `Shelf Suite`.
Reject network paths, reparse points or redirected ancestors, linked target
files, and an update package located inside the selected skin. Resolve every
target and backup location within its intended boundary.

Check only `@Resources/ShelfEngine.lua`, `@Resources/Variables.inc`, and
`Shelf<number>/Shelf.ini`. Enumerate immediate shelf directories; do not
recursively inventory or copy the skin. Reject ambiguous shelf identifiers,
missing required INIs, unreadable/busy files, unsupported encodings, duplicate
relevant sections/settings, and unrecognized source differences. Require at
least one recognized shelf. If one proposed target is invalid, refuse the
entire update before writing anything.

The shared engine and variables must match the exact known upstream or
approved F1 versions. A version label or matching filename is insufficient.
Recognize LF and CRLF forms explicitly; do not ignore arbitrary whitespace,
comments, or other differences when verifying source identity.

Recognize only the three pinned stock shelf templates and the trusted
Cafe Lock-generated shelf template from the verified lineage, with or without
the approved `DynamicWindowSize=1` entry. The only permitted theme variation
is the existing single `@IncludeTheme` selection of `DeepOcean`, `Forest`,
`Terracotta`, or `Obsidian` in its recognized location. Preserve that selection
exactly. Do not inspect theme files or infer custom themes. Descriptive metadata
must match a recognized template; it is not a general wildcard. No broad INI
repair, theme detection, or best-effort matching is permitted.

For INIs, verify the complete template after accounting solely for those
explicit variations. Retain the original supported encoding and line endings.
Accept only encoding forms proven against the pinned files and generated
templates by tests; do not guess an encoding or silently transcode.

An installation whose shared files and all shelf INIs already match F1 reports
that no action is needed and creates no backup. A mixture of known upstream
and F1 files may be completed; any unknown mixture is refused.

## Allowed modifications and protected data

Replace the shared engine and variables only with their verified F1 payloads.
For each recognized shelf INI lacking the setting, insert
`DynamicWindowSize=1` in the existing `[Rainmeter]` section. Preserve all other
bytes. INIs already containing the recognized setting are not rewritten.
Do not replace entire shelf INIs with stock copies.

The updater never reads, hashes, copies, rewrites, migrates, or uploads
`config.lua`. It also leaves Rainmeter settings and positions, Cafe Lock
credentials, launcher data, icons, themes, and all unrelated files untouched.
It does not determine shelf labels from configuration files.

This is an owner-run filesystem tool used with Rainmeter closed. It introduces
no skin command, hosted-editor route, unlock mechanism, or Cafe Lock policy
change. It has only the signed-in user's existing file permissions.

## Backup, application, and simple recovery

After successful preflight and owner confirmation, create a uniquely named
timestamped backup beside `Shelf Suite`, outside the skin directory. Back up
only files that will change, preserving their relative paths. Verify each
backup against the original bytes before the first replacement. If a backup
cannot be created or verified, stop without modifying installed files.

Record a small recovery manifest containing relative paths and original/new
hashes, plus readable restore instructions. Do not include source contents or
launcher data in logs. Recheck identity and original bytes before replacement;
refuse changed or newly redirected targets. Use staged verified output and
per-file replacement, not direct writes that truncate installed files.
Verify every completed replacement against its expected output hash.

Several replacements are not one atomic operation. On failure, attempt to
restore only files changed by this run whose current bytes still match the
updater's recorded output. Verify backups before restoring. Never overwrite
an unexpected current file, even during rollback. If any recovery step is
unsafe or fails, stop, retain the backup, and clearly identify which files need
manual inspection and restoration. Do not build a generic transaction engine,
background retry system, or automatic repair system.

Manual recovery: close Rainmeter, open the reported backup folder, inspect any
unexpected current changes, and copy the listed original files back to their
same relative locations in `Shelf Suite`. Do not replace the whole skin.
Keep backups after both success and failure; do not automatically delete them.
After an interrupted run, retain the backup/recovery record for manual review
rather than blindly replaying writes.

## Verification and acceptance

Automated tests use disposable fixtures, never the owner's installation.
Demonstrate refusal before writes for customized/ambiguous source, unknown
themes, missing/busy files, unsupported encoding, links/redirection, invalid
paths, corrupt payloads, and failed backup creation. Test cancellation, stock
shelves, trusted generated shelves, all four known theme selections, LF/CRLF,
supported encoding forms, already-updated and known partial installations,
spaces/non-English paths, changed targets, partial failure, safe rollback,
and refusal to roll back over an outside edit.

The test harness may create/hash its own sentinel `config.lua`, positions,
icons, and other unrelated fixtures to prove preservation. That activity must
remain outside the updater. Verify updater file access is restricted to the
declared targets and its own package/backup files, including failure paths.
Exercise the updater under a standard-user token and test the extracted ZIP,
not just repository scripts. Run the complete existing Windows workflow and
verified F1 live-layout regressions on the final revision.

Manual acceptance on the spare PC/VM covers selecting the real skin, refusal
while Rainmeter is running, confirmation, backup location, successful update,
repeat-run no-op, manual restore, existing shelf positions/items/themes, short
and long tabs, new shelves, gear placement, hover/selection, and Locked versus
Maintenance behavior. No manual test is performed automatically on the owner's
active desktop.

## Scope and next gate

Likely implementation files are confined to `Build/ShelfSuiteF1/`, delivery
tests in `Tests/`, the existing Windows workflow, and delivery documentation.
Do not modify the verified F1 patch, native shelf template, Rainmeter
authorization/password/interaction code, or unrelated ShelfSuite behavior.
F2, merging, releases, and deployment to the owner's machine are excluded.

Owner review of this written specification is required before creating the
implementation plan. Implementation remains prohibited until the subsequent
plan/execution approval.
