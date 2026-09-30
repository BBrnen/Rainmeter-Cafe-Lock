# ShelfSuite F1 Compatibility Delivery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a safely verified `ShelfSuite-Cafe-Lock-F1-Compatibility.zip` for updating the owner's existing Shelf Suite on a spare Windows PC/VM.

**Architecture:** A standalone Windows PowerShell updater performs read-only recognition, obtains confirmation, backs up only changed files, and applies verified F1 bytes. A separate CI builder produces payloads and recognition fingerprints from pinned source and the unchanged F1 patch. Recovery is limited to this run's changed files; it never repairs or replaces a whole skin.

**Tech Stack:** Windows PowerShell 5.1, .NET Framework file/cryptography APIs, native Windows Forms dialogs, narrowly scoped Windows file-identity calls if needed, and the existing Windows GitHub Actions workflow. Git is used only by the package builder, never required on the owner's PC. Tests use the repository's assertion-script style without installing Pester or another dependency.

**Spec:** `Docs/superpowers/specs/2026-10-01-shelfsuite-f1-delivery-design.md` (approved and unchanged). Also read `Docs/CafeLock-ShelfSuite-F1-Delivery.md` and `AGENTS.md`.

## Global Constraints

- Work only on `feature/shelfsuite-f1-delivery`, based on verified F1 commit `c6ce82ab7c2e9f901b01c712a0efd22bdee6d5c9`.
- ShelfSuite v2.1 base remains `d4f186ba0b5c262c7559b80841132f5fd3884f3c`.
- Do not modify the F1 patch, native shelf template, Rainmeter binaries/behavior, authorization/password rules, launcher behavior, or F2.
- Update the same existing local directory named `Shelf Suite`; no second skin/profile and no administrator requirement.
- No downloads, installed Git, PowerShell 7, extra dependencies, process termination, automatic Rainmeter launch, or persistent execution-policy change on the owner's PC.
- Never read, hash, copy, rewrite, migrate, or upload `config.lua`. No recursive skin inventory or whole-skin backup. Positions, credentials, icons, themes and launcher data remain untouched.
- Inspect only the two shared target files and immediate `Shelf<number>/Shelf.ini` files. Any unrecognized proposed target refuses the entire update before writes.
- Recognize only three stock templates and the trusted Cafe Lock-generated template, LF/CRLF, the approved dynamic-size entry, and the single existing theme selection among DeepOcean, Forest, Terracotta and Obsidian. No metadata wildcard or speculative theme detection.
- Full preflight and explicit confirmation precede backup, staging, or persistent logs. Backup only changed files, outside the skin, and verify every backup before replacing any target.
- No-op creates no backup. Keep backups after success/failure. Recovery must not overwrite an unexpected outside edit.
- Hashes prove consistency, not publisher authentication. Do not claim the unsigned package is tamper-proof.
- Publish only review artifacts after verification; no merge, release, normal-installer change, or deployment to the owner's installation.

## Review Focus

1. An extra customized shelf beside valid shelves must refuse this updater without partial writes (Task 2).
2. A path alias, link, junction, or redirected ancestor must not redirect reads/writes outside declared boundaries (Tasks 2 and 3).
3. Non-English paths, theme choices and encoding/newlines must survive without normalization of unrelated bytes (Tasks 1 and 2).
4. Disk/permission failure or an outside edit during update must leave verified originals available and never trigger unsafe recovery (Task 3).
5. A moved/extracted ZIP, cancelled dialog, or restrictive execution policy must behave safely under the actual standard-user launch path (Tasks 4 and 5).

## Files and interfaces

Create these production delivery files only:

- `Build/ShelfSuiteF1/Package.ps1`: build provenance, recognition catalog, payloads, ZIP and external SHA-256.
- `Build/ShelfSuiteF1/Updater.psm1`: package checks, bounded file operations, read-only preflight, backup/application and simple recovery.
- `Build/ShelfSuiteF1/Update-ShelfSuite.ps1`: native guided user interaction; import the module from `$PSScriptRoot`.
- `Build/ShelfSuiteF1/Update-ShelfSuite.cmd`: launch built-in PowerShell 5.1 with `-NoProfile -STA -File`, preserve exit status and keep failures readable. Do not set or bypass machine execution policy.
- `Build/ShelfSuiteF1/README.md`: extraction, owner workflow, script-policy limits, backup and manual recovery instructions.

Create test files:

- `Tests/CafeShelfF1Delivery.ps1`: suite runner/assertions, restricted to newly created disposable fixtures; accepts `-Suite Package|Preflight|Apply|UI|All`, `-PackageDirectory`, and `-UpstreamDirectory`.
- `Tests/CafeShelfF1DeliveryFixtures.ps1`: generate fixtures from pinned source/trusted template and known variations; never touch an installed skin.
- `Tests/CafeShelfF1DeliveryRunner.ps1`: disposable Windows CI preparation, package extraction, standard-user helper invocation and evidence collection.

Modify only `.github/workflows/cafe-lock.yml` for the new delivery job and `Docs/CafeLock-Compatibility.md` to link the separately verified artifact/manual checks. Preserve the historical F1 delivery-contract statement: F1 itself did not ship this package, and the normal installer still does not deploy it.

PowerShell objects below have explicit properties; they are not persisted session authorization:

- `PackageInfo`: SchemaVersion (1), UpstreamRevision, F1Revision, PatchSha256, PackageDirectory, Payloads, Recognition. Paths are relative allowlisted paths; hashes are SHA-256 hex strings.
- Recognition entries: relative shared-file path or recognized INI family, exact input hash, expected output hash, encoding/newline identity, and known dynamic/theme variant. Hash input is original bytes, not broadly normalized text.
- `Change`: RelativePath, OriginalBytes (`byte[]`), OutputBytes (`byte[]`), OriginalHash, OutputHash, Identity (volume/file identity and link/reparse checks).
- `Preflight`: Root, PackageInfo, Changes (`Change[]`), checked directory identities. Empty Changes means no action needed. This in-memory object is not sufficient authority without rechecking files at apply time.
- `UpdateResult`: Status (`Updated|NoAction|FailedRecovered|ManualRecoveryRequired`), BackupDirectory, ChangedPaths, ManualRecoveryPaths, Errors. Errors identify filenames/reasons, not source contents or launcher data.

Module exports:

- `Read-F1Package([string]$PackageDirectory) -> PackageInfo` (throws a safe diagnostic on invalid package).
- `Get-F1Preflight([string]$Root, [PackageInfo]$Package) -> Preflight` (read-only, throws on any invalid target).
- `Invoke-F1Update([Preflight]$Plan) -> UpdateResult` (rechecks, backs up, stages, replaces, verifies; performs bounded recovery on failure).

Private file helpers centralize all target opens and identity checks. Tests instrument those helpers inside test scope to record accesses and inject failures; no distributed test/fault flags, bypass switches, or arbitrary script execution hooks. The UI supplies confirmation before invoking the exported apply function; it cannot authorize an unknown file.

---

### Task 1: Build the verified package and recognition catalog

**Files:** Create `Build/ShelfSuiteF1/Package.ps1`, the Package suite and fixture helpers in the two test files above. Read, never edit, the patch and `Library/CafeShelf/ShelfTemplate.h`.

**Interfaces:** Builder command: `Package.ps1 -UpstreamDirectory <clean pinned checkout> -OutputDirectory <fresh directory>`. Produces `ShelfSuite-Cafe-Lock-F1-Compatibility.zip`, matching `.zip.sha256`, and a staging directory containing `manifest.json`, `payload/@Resources/ShelfEngine.lua`, `payload/@Resources/Variables.inc`, updater files, README and `LICENSE-ShelfSuite.txt`. Recognition INIs are fingerprints, not full shelf copies. The final complete ZIP becomes available after Task 4 adds the guided entry point.

- [ ] **RED:** Add named assertions `ExactPinnedSourceOnly`, `UnchangedF1PatchOnly`, `CatalogMatchesStockAndGeneratedBytes`, `CatalogThemeAndDynamicVariantsOnly`, `PayloadAndZipAllowlist`, `CorruptPayloadRejected`, and `FreshOutputRequired`. Wrong source revision, local source modifications, altered patch and template lineage, stale staging, path traversal/duplicate manifest paths, and extra skin/config/icon/theme content must fail.
- [ ] Run `powershell.exe -NoProfile -File Tests/CafeShelfF1Delivery.ps1 -Suite Package -UpstreamDirectory <disposable pinned checkout>`; confirm failure is the absent builder/package validation, not a fixture setup error.
- [ ] Implement the builder. Verify upstream HEAD and clean tracked source; compare patch and native template bytes with their versions at c6ce82ab. Archive only needed pinned source into fresh staging; use `git apply --unidiff-zero --check` then `git apply --unidiff-zero`; verify the five approved patch paths. Generate both shared payloads and catalog entries from the actual source bytes.
- [ ] Explicitly verify valid UTF-8 without BOM for the pinned/trusted templates and support that encoding only, in separately fingerprinted LF and CRLF forms. Reject BOM/UTF-16, invalid UTF-8 and mixed newlines initially. This is conservative recognition, not transcoding. If pinned/trusted bytes contradict this finding, stop for a decision rather than broadening recognition silently.
- [ ] Generate INI variants only by the one recognized theme-line substitution and presence/absence of `DynamicWindowSize=1` immediately after `AccurateText=1`. Preserve complete remaining template bytes. For shared files include exact upstream/F1 LF/CRLF fingerprints; use the matching newline form of the approved output. Include SHA-256 for distributed executable scripts/module as well as payloads; validate manifest schema and strict path allowlists. This does not establish a digital signature.
- [ ] **GREEN:** Rerun Package assertions; independently apply the unchanged patch in a disposable checkout and compare outputs/catalog hashes. Until Task 4, test package construction with a clearly incomplete internal fixture; do not upload it as the owner ZIP.
- [ ] Commit only sources/tests: `build: generate verified ShelfSuite F1 delivery payloads`.

### Task 2: Implement conservative, read-only preflight

**Files:** Create `Build/ShelfSuiteF1/Updater.psm1`; extend Preflight suite and fixtures.

**Interfaces:** Implement `Read-F1Package` and `Get-F1Preflight` using Task 1's manifest and the objects defined above. All target file access passes through private bounded helpers. No backups, staging files or persistent logs in these functions.

- [ ] **RED:** Add `StockThreeShelvesRecognized`, `GeneratedShelfRecognized`, `AllFourThemesPreserved`, `KnownMixedInstallRecognized`, `AlreadyF1HasZeroChanges`, `OnlyDeclaredTargetsOpened`, and `UnknownShelfRefusesWholeUpdate`. Assert output bytes equal originals except the precise dynamic-size insertion/approved shared payloads. Include immediate Shelf4 and higher known generated shelves.
- [ ] Add rejection cases for no shelves, Shelf1/Shelf01 identifier ambiguity, noncanonical or overflowing numeric identifiers, unknown/case-ambiguous source names, customized metadata, duplicated relevant section/setting, unknown theme, missing/unreadable/exclusively locked INI, invalid encoding/mixed newlines, customized shared file, corrupt manifest/payload and insufficient read access. Assertions: safe relative filename/reason; zero created files and unchanged targets.
- [ ] Add local root/name checks, UNC/network paths, package-inside-skin, junction/symlink/reparse ancestors and target files, hard-linked files, and spaces/non-English paths. No recursive inventory. Instrument target reads; an exclusively locked fixture `config.lua` must not affect recognition, and the access trace must contain no config, position, credential, icon or theme-file access. Only the harness reads its own sentinels.
- [ ] Run `powershell.exe -NoProfile -File Tests/CafeShelfF1Delivery.ps1 -Suite Preflight -PackageDirectory <fixture package> -UpstreamDirectory <pinned checkout>`; confirm expected RED cases before implementation.
- [ ] Implement full-path/ancestor containment and identity checks using .NET plus the smallest necessary Windows handle calls for reparse/link count/file identity. Enumerate immediate directories and match whole file bytes through the catalog; do not parse generic INI or infer shelf labels. Produce only necessary Changes. Reject inability to establish safety with a clear path/reason.
- [ ] **GREEN:** Rerun Preflight and Package suites in PowerShell 5.1; verify every invalid fixture has no writes, valid LF/CRLF variants retain exact bytes, and no-op has empty Changes.
- [ ] Commit: `feat: validate existing ShelfSuite before F1 updates`.

### Task 3: Verified backups, limited replacement and simple recovery

**Files:** Extend `Updater.psm1`, Apply suite and fixtures.

**Interfaces:** Implement `Invoke-F1Update`; consume Preflight and return UpdateResult. Private recovery consumes the current run's changed-path list and verified backup record only; it is not an exported replay/repair command.

- [ ] **RED:** Add `BackupsVerifiedBeforeFirstReplacement`, `OnlyChangedFilesBackedUp`, `TimestampCollisionNeverOverwrites`, `NoActionCreatesNothing`, `BackupFailureLeavesTargetsUnchanged`, `ChangedOrRedirectedTargetRefused`, `StagedOutputVerified`, and `AppliedOutputVerified`. Inject access/space/write errors and corruption using test-scoped helper substitutions, never a real full disk or owner's files.
- [ ] Add `PartialFailureRestoresMatchingOutputs`, `OutsideEditNeverOverwrittenByRecovery`, `CorruptBackupNotRestored`, `RecoveryFailureKeepsBackupAndInstructions`, and `InterruptedRunHasManualRecord`. Assert listed manual paths are accurate; untouched files stay untouched; changed files are original or identified for manual review; no blind replay or backup deletion.
- [ ] Run `powershell.exe -NoProfile -File Tests/CafeShelfF1Delivery.ps1 -Suite Apply -PackageDirectory <fixture package> -UpstreamDirectory <pinned checkout>` and confirm RED failures arise from missing apply/recovery behavior.
- [ ] Implement immediate revalidation of package, Rainmeter process absence, directory/file identity and original hashes before application. Create a collision-safe timestamped sibling backup using create-new semantics; validate parent/backup boundaries. Copy only Changes with relative paths; verify all original backup hashes before the first target replacement. Write a small recovery manifest and readable restore instructions with paths/hashes only.
- [ ] Stage verified output inside the validated backup area, on the same volume as the target. Recheck each target immediately before per-file replacement; use the Windows/.NET replacement operation rather than truncating target files. Verify output immediately afterward. Preserve the required newline form and each INI's original supported encoding. Document the multi-file operation's non-atomic nature rather than claiming crash-proof or race-proof transactions.
- [ ] On failure restore only this run's files still equal to expected just-written bytes and with safe identity/path checks. Verify each backup before restoration and verify restored bytes. If unsafe or unsuccessful, retain everything and return `ManualRecoveryRequired` with the affected paths and manual instructions. An interrupted process leaves the existing record for owner inspection, not automated replay.
- [ ] **GREEN:** Rerun Package, Preflight and Apply suites. Access traces on success and every failure may contain only package files, declared target files, checked directories and this run's backup/staging files. Harness sentinel contents/hashes remain unchanged.
- [ ] Commit: `feat: back up and safely apply ShelfSuite F1 compatibility files`.

### Task 4: Guided standard-user launcher and owner instructions

**Files:** Create `Update-ShelfSuite.ps1`, `Update-ShelfSuite.cmd`, `README.md`; complete package inclusion; extend UI suite; update `Docs/CafeLock-Compatibility.md` with a separate-delivery link.

**Interfaces:** Entry script takes no root/unlock/force arguments; the folder picker chooses Root. It calls Read-F1Package, Get-F1Preflight and, only after confirmation, Invoke-F1Update. Exit 0 means success/no-action/cancel; exit 1 means refusal/failure. UI-only private dialog/process helpers can be substituted in tests; production has no hidden bypass.

- [ ] **RED:** Add `CancelChangesNothing`, `RunningRainmeterRefuses`, `ProcessInspectionFailureRefuses`, `ResolvedPathAndChangedListConfirmed`, `NoOpSkipsBackup`, `SafeErrorContainsNoSourceContents`, `LauncherUsesBuiltinPowerShell`, `LaunchFromDifferentWorkingDirectory`, and `PolicyRefusalDoesNotAlterPolicy`. Test process refusal with a test-scoped substitute, not by killing/starting the owner's Rainmeter.
- [ ] Run the UI suite in PowerShell 5.1; expected RED is missing guided entry/launcher behavior.
- [ ] Implement native folder selection, personal-backup reminder, resolved path and proposed file list, explicit confirm/cancel, running-process refusal, and result dialogs with backup location/manual recovery paths. Close errors explain how to exit Rainmeter normally; never terminate or automatically restart it. All package paths use `$PSScriptRoot`, literal paths and correct argument quoting.
- [ ] Implement the CMD launcher without elevation or policy bypass. It uses built-in PowerShell 5.1; failures remain visible with a short explanation that machine policy may block scripts and must be resolved by the owner. Never modify persistent execution policy. Controlled tests substitute the child-launch result instead of changing the host's policy.
- [ ] Write plain-English extraction/update/no-op/failure/manual-restore instructions, explaining exactly which files can change and that no config.lua is accessed. Include licence/attribution, unsigned-hash limitation, owner-managed backup advice, and separate normal-installer status. Finish Task 1's complete ZIP allowlist tests.
- [ ] **GREEN:** Run `-Suite All` in PowerShell 5.1 on disposable fixtures. Test the packaged launcher from a different working directory with spaces/non-English names. Manual native dialog appearance remains a spare-PC check; tests prove control flow, not visual usability.
- [ ] Commit: `feat: guide owners through the ShelfSuite F1 compatibility update`.

### Task 5: Extracted-ZIP standard-user verification and full Windows workflow

**Files:** Create `Tests/CafeShelfF1DeliveryRunner.ps1`; modify `.github/workflows/cafe-lock.yml`. No normal installer changes.

**Interfaces:** Runner accepts `-ZipPath <built ZIP> -UpstreamDirectory <pinned source>`; it requires a disposable GitHub Windows runner, extracts to fresh `RUNNER_TEMP` staging, gives the restricted user only the fixture/staging access needed, verifies the token, and launches the All suite against the extracted package using existing `RunAsStandard.exe` and built-in PowerShell 5.1. Never fake `GITHUB_ACTIONS` locally.

- [ ] **RED:** Add package-runner assertions that fail on an unextracted/repository module path, elevated/admin token, invalid ZIP hash, missing packaged entry point, contaminated output folder, or use of PowerShell 7 as the owner runtime. Final tests must import code from the extracted ZIP, not repository copies.
- [ ] Implement a separate `shelfsuite-f1-delivery` Windows-2022 job with the workflow's pinned checkout/upload actions, full repository history for provenance, exact upstream checkout, builder, x64 compilation of existing `Tests/RunAsStandard.cpp`, extracted-ZIP restricted-user tests, and artifact upload only after that job passes. Exit nonzero on any child/build/test failure. Upload the exact ZIP and `.zip.sha256` as review artifacts; no release action.
- [ ] Verify extracted ZIP success/refusal/recovery suites under medium-integrity, non-admin token and PS5.1. Confirm the package includes no fixture configs, full skin, private paths/log contents, themes, icons, test hooks or generated test binaries. Record stdout-safe results separately from owner data.
- [ ] Commit: `ci: verify extracted ShelfSuite F1 package as a standard user`.
- [ ] Run the complete existing `.github/workflows/cafe-lock.yml` using workflow_dispatch on this branch; arbitrary feature pushes do not trigger it. Verify all jobs at the final implementation revision: x64 build/architecture, policy/password, standard-user runtime, complete editor tests, pinned F1 live-layout regressions, existing installer tests, and the new delivery job. A green older run or delivery-only run is insufficient.

### Task 6: Whole-branch review, verification record and manual handoff

**Files:** Create `Docs/superpowers/plans/2026-10-01-shelfsuite-f1-delivery-verification.md` with actual evidence only. Fixes, if needed, stay within approved delivery scope.

- [ ] Use `superpowers:requesting-code-review` for a fresh whole-branch review after implementation; inspect data boundaries, refusal-before-write, theme/template recognition, package provenance, backup verification, unsafe-recovery refusal and standard-user coverage. Native execution preserves the owner's previous preference; no feature expansion.
- [ ] Address justified findings with focused RED/GREEN fixes. If review finds a specification contradiction, stop for the owner's decision; do not change the approved specification or verified F1 behavior.
- [ ] Use `superpowers:verification-before-completion`; run `git diff --check`, inspect the complete diff/file allowlist and compare the verified patch/template/application files against c6ce82ab. Run changed delivery suites again; after executable changes obtain a fresh full Windows workflow at the actual final code revision.
- [ ] Record commits, RED/GREEN evidence, PowerShell/token versions, final workflow revision/run URL, artifact names/checksums, limitations and manual checklist. Documentation-only evidence commit does not require rebuilding unchanged binaries; identify the exact code revision whose artifact was tested.
- [ ] Commit: `docs: record ShelfSuite F1 delivery verification and manual checks`.
- [ ] Give the owner the review artifact and these exact spare-PC/VM steps; do not test or install automatically on their desktop:
  1. Make a personal backup yourself; extract the ZIP outside Shelf Suite and run its launcher as the normal user.
  2. Select the existing Shelf Suite while Rainmeter is running: expect refusal and no changes. Exit Rainmeter normally, rerun, select again, cancel confirmation: expect no backup/changes.
  3. Rerun and confirm: check the displayed path/file list, timestamped sibling backup and success report. Start Rainmeter yourself.
  4. Check existing shelf positions, launchers, icons and themes; short ONLINE/OFFLINE/INTERNET tabs remain compact, SCHOOL/WORK/long combinations fit, tabs do not overlap, and gear/hover/selection remain correct. Create a shelf through Maintenance Mode and check its adaptive tabs too.
  5. Check Locked Mode still permits launchers/tabs but blocks editing/movement; Maintenance Mode still works normally. This updater adds no unlock route.
  6. Close Rainmeter, rerun the updater: expect no action needed and no new backup.
  7. On a disposable copy, customize one target INI: expect a clear refusal with all files unchanged. Do not customize the owner's real working skin for this test.
  8. On the spare installation, close Rainmeter and manually restore only the listed files from the verified backup to their original relative paths; start Rainmeter and confirm the original layout returns and launcher data/positions remain intact. Keep the backup.
- [ ] Stop. Do not start F2, merge, release or deploy. Delivery readiness remains conditional on the owner's manual acceptance, even after CI passes.

## Planning self-review

Coverage: package/provenance (Task 1), conservative recognition and protected reads (Task 2), backups/write boundaries/recovery (Task 3), native owner workflow and policy limits (Task 4), extracted-ZIP/non-admin/full Windows verification (Task 5), independent review/evidence/manual acceptance (Task 6). Every Review Focus condition has a named test in its owning task. Signatures and object properties above are shared across tasks. No production change or test execution is authorized by this document alone: owner plan approval is the next gate.
