# ShelfSuite F1 Native Finalization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver the verified F1 adaptive-tab compatibility update through one explicit Maintenance Mode action in the normal Rainmeter Cafe Lock installer/runtime flow.

**Architecture:** The installer carries a protected, source-controlled F1 payload and exact recognition manifest; it never touches a user's ShelfSuite installation. A focused native C++ component performs strict inspection, backup, update, verification, and bounded recovery under the normal user's token. The existing Manage Settings UI invokes that component only after Cafe Lock has authenticated Maintenance Mode.

**Tech Stack:** C++17/Win32, BCrypt SHA-256, existing CafeShelf JSON/header conventions, Windows file identity/reparse APIs, existing NSIS installer, PowerShell only for disposable CI/test orchestration.

**Spec:** `Docs/superpowers/specs/2026-10-01-shelfsuite-f1-native-finalization-design.md`

## Global Constraints

- Work only on `feature/f1-installer-finalization`, based on the preserved F1 tag `v4.5.26.3894-cafe-lock-f1.0` at `c6ce82ab7c2e9f901b01c712a0efd22bdee6d5c9`.
- Keep `ThirdParty/ShelfSuite/patches/0001-cafe-lock-adaptive-tabs.patch` byte-for-byte unchanged. Do not modify F1 sizing behavior or begin F2.
- ShelfSuite source remains pinned to `d4f186ba0b5c262c7559b80841132f5fd3884f3c`; preserve its MIT licence and attribution.
- Do not merge or use the unfinished `feature/shelfsuite-f1-delivery` PowerShell updater at runtime. No PowerShell, separate updater program, execution-policy change, elevation, download, or security bypass occurs on café PCs.
- The normal installer copies protected program files only. It must not inspect, copy, back up, modify, or delete a user ShelfSuite directory during install, upgrade, uninstall, or startup.
- The native operation uses `GetRainmeter().GetSkinPath()` and the literal child `Shelf Suite`; never assume Documents, a username, or a folder-number/template relationship.
- Allow only ordinary local fixed-disk paths and the exact target set: `Shelf Suite\@Resources\ShelfEngine.lua`, `Shelf Suite\@Resources\Variables.inc`, and immediate exact `Shelf<number>\Shelf.ini` files.
- Recognize every `ShelfN` by its complete approved bytes: stock/generated template, four known themes, LF/CRLF, and approved dynamic-size state. Reject one unexpected target before any write.
- Never read, hash, copy, rewrite, migrate, parse, or upload `config.lua`; preserve icons, themes, launcher data, passwords, positions, Rainmeter settings, and unrelated files.
- The button and native entry point require Maintenance Mode; repeat authorization checks before every write/recovery phase. Add no bang, command-line flag, registry switch, tray route, or alternate unlock mechanism.
- Verify backups before the first replacement, verify every staged/output byte, and never overwrite an outside edit during recovery. An already manually F1-updated installation creates no backup and reports **Already compatible**.
- Use disposable fixtures and standard-user tokens for all tests. Keep real café PCs and profiles out of automated tests.

## Review Focus

1. **Additional shelf identity:** `Shelf4` or `Shelf27` may be stock, generated, or a different approved family; its exact contents, not its number, must decide recognition. Task 2 tests mixed recognized families and an unexpected additional shelf.
2. **Manual F1 state:** all approved F1 hashes, including manually updated shared files/INIs, must produce **Already compatible** with zero writes and no backup. Task 2 tests this directly.
3. **Locked/direct invocation:** a forged Settings-window command or Lock Now during confirmation/application must not inspect or modify user files. Task 4 tests both UI and service gates.
4. **Live Rainmeter/ShelfSuite:** the action runs inside Rainmeter while its shelves are loaded; busy/unsafe files refuse without writes, while a safe run leaves loaded skins untouched until the owner reloads/restarts normally. Task 4 tests both outcomes.
5. **Failure after a write:** a later target failure or outside edit must restore only verified F1 output and retain clear manual recovery data. Task 3 injects both cases.

---

### Task 1: Freeze and audit the native F1 payload

**Files:**
- Create: `ThirdParty/ShelfSuite/F1Compatibility/manifest.json`
- Create: `ThirdParty/ShelfSuite/F1Compatibility/payload/@Resources/ShelfEngine.lua`
- Create: `ThirdParty/ShelfSuite/F1Compatibility/payload/@Resources/Variables.inc`
- Create: `ThirdParty/ShelfSuite/F1Compatibility/README.md`
- Create: `Tests/CafeShelfF1NativePayload.ps1`
- Modify: `.github/workflows/cafe-lock.yml`

**Interfaces:**
- Consumes: the unchanged F1 patch and a disposable checkout of the exact pinned ShelfSuite revision.
- Produces: a source-controlled JSON manifest with the F1 revision, patch digest, two payload SHA-256 values, and all approved input/output recognition entries. Later runtime code receives the installed copy at `<RainmeterPath>\Compatibility\ShelfSuiteF1`.

- [ ] **Step 1: Write failing provenance assertions**

  In `Tests/CafeShelfF1NativePayload.ps1`, add named assertions `PinnedSourceAndPatchOnly`, `PayloadMatchesPatchedSource`, `RecognitionCatalogCoversOnlyApprovedVariants`, `NoFullSkinOrUserDataBundled`, and `ManifestRejectsTraversalOrDuplicates`. Require the two exact shared payload paths, 72 recognition entries, the F1/tag revisions, and no `config.lua`, icons, themes, or full ShelfSuite copy.

- [ ] **Step 2: Run the payload audit to verify RED**

  Run: `pwsh -NoProfile -File Tests/CafeShelfF1NativePayload.ps1 -UpstreamDirectory work-package/ShelfSuite`

  Expected: FAIL because the native payload/manifest does not yet exist.

- [ ] **Step 3: Add only the reviewed F1 payload and exact manifest**

  Derive the two payload files and every recognition entry by applying the unchanged patch to a clean pinned checkout. Record input/output SHA-256 values for all allowed stock/generated, theme, dynamic, and newline combinations. Keep `README.md` limited to provenance/licence information; do not include a runnable script or updater.

- [ ] **Step 4: Run the payload audit to verify GREEN**

  Run: `pwsh -NoProfile -File Tests/CafeShelfF1NativePayload.ps1 -UpstreamDirectory work-package/ShelfSuite`

  Expected: PASS, including a byte-for-byte comparison with the patched checkout and rejection of unexpected bundled content.

- [ ] **Step 5: Add the workflow audit and commit**

  Insert the audit after the workflow checks out/applies the pinned ShelfSuite patch. Commit only the payload provenance sources, audit, and workflow wiring.

  ```powershell
  git add ThirdParty/ShelfSuite/F1Compatibility Tests/CafeShelfF1NativePayload.ps1 .github/workflows/cafe-lock.yml
  git commit -m "build: audit native ShelfSuite F1 compatibility payload"
  ```

### Task 2: Implement strict native inspection and no-op recognition

**Files:**
- Create: `Library/CafeShelf/F1Compatibility.h`
- Create: `Library/CafeShelf/F1Compatibility.cpp`
- Modify: `Library/Library.vcxproj`
- Create: `Tests/CafeShelfF1Compatibility.cpp`
- Create: `Tests/CafeShelfF1CompatibilityFixtures.ps1`
- Modify: `Tests/CafeShelfTests.ps1`

**Interfaces:**
- Consumes: an explicit `skinPath`, protected `payloadPath`, and the Task 1 manifest/payload data.
- Produces:

  ```cpp
  namespace CafeShelf::F1 {
  enum class Status { Preview, AlreadyCompatible, Refused, Updated, FailedRecovered, ManualRecoveryRequired };
  struct Change { std::wstring relativePath, originalHash, outputHash; };
  struct Preview { Status status; std::wstring shelfRoot, proposedBackup, message; std::vector<Change> changes, compatible; };
  struct ApplyResult { Status status; std::wstring backup, message; std::vector<std::wstring> changed, manualRecovery; };
  Result<Preview> Inspect(const std::wstring& skinPath, const std::wstring& payloadPath);
  Result<ApplyResult> Apply(const Preview&, const std::function<bool()>& authorized);
  }
  ```

  `Inspect` performs no write or backup. `Apply` accepts only a fresh successful preview and invokes `authorized()` before each write/recovery phase.

- [ ] **Step 1: Write failing native inspection tests**

  Add fixtures for every approved stock/generated family across the four allowed themes and both newline forms. Include `Shelf1`, `Shelf2`, `Shelf3`, `Shelf4`, and `Shelf27` deliberately assigned to different approved families. Assert that each change is chosen by complete source hash, never by folder number. Add `AlreadyCompatibleManualF1NoWrites`, `KnownPartialF1Completes`, `UnexpectedAdditionalShelfRefusesWholePlan`, `NoShelvesRefuses`, and sentinel access tracing that proves no `config.lua`/icon/theme/position path is opened.

- [ ] **Step 2: Run the core suite to verify RED**

  Run: `pwsh -NoProfile -File Tests/CafeShelfTests.ps1 -Suite F1Compatibility`

  Expected: FAIL because `F1Compatibility` and the compiled fixture harness do not yet exist.

- [ ] **Step 3: Implement bounded path, manifest, and hash inspection**

  In `F1Compatibility.cpp`, use the existing safe-file patterns from `Storage.cpp`: canonical local fixed-disk paths, reparse/hard-link refusal, bounded reads, identity capture, BCrypt SHA-256, and literal allowlisted relative paths. Parse only the shipped manifest schema, verify its expected provenance and each payload hash, enumerate only direct exact `Shelf<number>` folders, and compare whole target bytes against one approved catalog entry. Do not infer content from folder number or parse `config.lua`.

- [ ] **Step 4: Implement preview/no-op result formation**

  Construct a `Preview` containing only relative paths/hashes and a proposed sibling backup path. Return `AlreadyCompatible` with empty changes when every target matches an approved F1 output hash. Return a safe refusal identifying only the first affected relative target and reason when any target is unexpected; create no filesystem state.

- [ ] **Step 5: Run the core suite to verify GREEN and commit**

  Run: `pwsh -NoProfile -File Tests/CafeShelfTests.ps1 -Suite F1Compatibility`

  Expected: PASS for every recognized `ShelfN`, manually F1-updated no-op, partial known update preview, and all refusal/no-access cases.

  ```powershell
  git add Library/CafeShelf/F1Compatibility.* Library/Library.vcxproj Tests/CafeShelfF1Compatibility.cpp Tests/CafeShelfF1CompatibilityFixtures.ps1 Tests/CafeShelfTests.ps1
  git commit -m "feat: inspect ShelfSuite F1 compatibility natively"
  ```

### Task 3: Add verified backup, replacement, and bounded recovery

**Files:**
- Modify: `Library/CafeShelf/F1Compatibility.cpp`
- Modify: `Tests/CafeShelfF1Compatibility.cpp`
- Modify: `Tests/CafeShelfF1CompatibilityFixtures.ps1`
- Modify: `Tests/CafeShelfTests.ps1`

**Interfaces:**
- Consumes: a Task 2 `Preview` with recognized changes and an authorization callback.
- Produces: `ApplyResult` with `Updated`, `FailedRecovered`, or `ManualRecoveryRequired`; backup/recovery paths contain only declared changed files and hashes.

- [ ] **Step 1: Write failing application/recovery tests**

  Add assertions `BackupsVerifiedBeforeReplacement`, `OnlyChangedFilesBackedUp`, `AppliedOutputsMatchManifest`, `NoOpCreatesNoBackup`, `BackupFailureLeavesTargetsUnchanged`, `BusyLoadedTargetRefusesWithoutWrites`, `PartialFailureRestoresOnlyOwnOutput`, `OutsideEditNeverOverwrittenDuringRecovery`, and `RecoveryRecordContainsNoUserConfiguration`. Use fixture seam callbacks for write/replace failures and locks; do not use an actual café profile or full disk.

- [ ] **Step 2: Run the application suite to verify RED**

  Run: `pwsh -NoProfile -File Tests/CafeShelfTests.ps1 -Suite F1CompatibilityApply`

  Expected: FAIL because `Apply` is not implemented.

- [ ] **Step 3: Implement verified backups and replacement**

  Create a collision-safe timestamped sibling backup only after a fresh reinspection. Copy only planned originals under matching relative paths; flush and verify every backup hash before staging/replacing any target. Preserve each INI's recognized byte/newline form, stage and hash outputs, recheck path identity/original hash immediately before `ReplaceFileW`, then re-read and verify the output hash.

- [ ] **Step 4: Implement bounded recovery and human instructions**

  Write a minimal recovery record and `RESTORE.txt` containing paths/hashes only. On later failure, restore only a target still holding this run's expected output with the recorded safe identity. Retain the backup and return `ManualRecoveryRequired` when recovery would overwrite an outside edit, the backup is invalid, or a restore fails.

- [ ] **Step 5: Run the application suite to verify GREEN and commit**

  Run: `pwsh -NoProfile -File Tests/CafeShelfTests.ps1 -Suite F1CompatibilityApply`

  Expected: PASS for success, no-op, busy running-shelf refusal, pre-write failures, partial failures, safe rollback, and manual-recovery refusal.

  ```powershell
  git add Library/CafeShelf/F1Compatibility.cpp Tests/CafeShelfF1Compatibility.cpp Tests/CafeShelfF1CompatibilityFixtures.ps1 Tests/CafeShelfTests.ps1
  git commit -m "feat: apply ShelfSuite F1 compatibility with recovery"
  ```

### Task 4: Expose one Maintenance-Mode-only native action

**Files:**
- Modify: `Library/CafeLock.h`
- Modify: `Library/CafeLock.cpp`
- Modify: `Library/DialogManage.h`
- Modify: `Library/DialogManage.cpp`
- Modify: `Tests/CafeLockPolicy.cpp`
- Modify: `Tests/CafeShelfSuite.ps1`
- Modify: `Tests/CafeShelfTests.ps1`

**Interfaces:**
- Consumes: `CafeShelf::F1::Inspect`/`Apply`, `GetRainmeter().GetSkinPath()`, `GetRainmeter().GetPath()`, and `CafeLock::IsLocked()`.
- Produces: `CafeLock::OpenShelfSuiteF1Compatibility(HWND owner)`, which shows preview/confirmation/result with TaskDialog/MessageBox and has no browser, tray, bang, or command-line entry point.

- [ ] **Step 1: Write failing authorization/UI tests**

  Extend `CafeLockPolicy.cpp` and the live ShelfSuite runner with `F1ButtonAbsentWhileLocked`, `ForgedF1SettingsCommandDenied`, `MaintenancePreviewShowsChangesAndBackup`, `CancelChangesNothing`, `LockNowBeforeApplyChangesNothing`, `LockNowDuringApplyStopsBeforeNextWrite`, and `LoadedShelvesRequireOwnerReload`. Assert that normal launcher/tab/hover behavior remains unchanged while locked.

- [ ] **Step 2: Run the policy and live fixture to verify RED**

  Run separately:

  ```bat
  cl /nologo /EHsc /W4 /WX /DNOMINMAX Tests\CafeLockPolicy.cpp /Fe:CafeLockPolicy.exe
  CafeLockPolicy.exe
  ```

  ```powershell
  pwsh -NoProfile -File Tests/CafeShelfTests.ps1 -Suite F1CompatibilityUi
  ```

  Expected: new action/authorization assertions fail because no Maintenance UI route exists.

- [ ] **Step 3: Implement the confined Settings-tab button and guarded entry point**

  Add one Settings control named **Apply ShelfSuite F1 compatibility**. Its handler calls only `CafeLock::OpenShelfSuiteF1Compatibility`. That function immediately rejects Locked Mode, obtains the runtime skin/program paths, requests the native preview, presents its path/change/compatible/backup summary, and asks for explicit confirmation. Pass an authorization lambda into `Apply` so revocation prevents later writes. Keep the operation out of `TrayIcon`, `CommandHandler`, web editor messages, and all Bang handling.

- [ ] **Step 4: Implement clear owner results without automatic skin changes**

  Display distinct **Already compatible**, **Refused**, **Updated**, **Recovered after failure**, and **Manual recovery required** messages. On success list only changed relative paths and the backup location, then instruct the owner to use normal Maintenance controls to reload affected ShelfSuite skins or restart Rainmeter. Do not automatically refresh, reload, unload, move, or close skins.

- [ ] **Step 5: Run GREEN UI/security tests and commit**

  Run the two Step 2 commands plus `pwsh -NoProfile -File Tests/CafeShelfSuite.ps1` on its disposable fixture.

  Expected: PASS; Locked Mode cannot expose/invoke the service, Maintenance can preview/cancel/apply, safe loaded-skin cases leave the runtime untouched until the owner reloads, and all existing launcher/lock behavior remains intact.

  ```powershell
  git add Library/CafeLock.* Library/DialogManage.* Tests/CafeLockPolicy.cpp Tests/CafeShelfSuite.ps1 Tests/CafeShelfTests.ps1
  git commit -m "feat: add Maintenance Mode ShelfSuite F1 action"
  ```

### Task 5: Package protected payload data and test installation/upgrade

**Files:**
- Modify: `Build/CafeInstaller/Package.ps1`
- Modify: `Build/CafeInstaller/Installer.nsi`
- Modify: `Tests/CafeInstaller.ps1`
- Modify: `.github/workflows/cafe-lock.yml`
- Modify: `Docs/CafeLock-Deployment.md`
- Modify: `Docs/CafeLock-Compatibility.md`

**Interfaces:**
- Consumes: the Task 1 source-controlled F1 bundle and existing installer manifest generation.
- Produces: `<InstallDir>\Compatibility\ShelfSuiteF1\` readable by standard users but protected by the existing program-directory ACL; installer results leave user ShelfSuite and backups untouched.

- [ ] **Step 1: Write failing installer/package assertions**

  Add tests named `F1BundleInstalledAndProtected`, `InstallerNeverTouchesUserShelfSuite`, `UpgradePreservesExistingShelfSuiteAndBackup`, `UninstallLeavesUserShelfSuiteAndBackup`, and `StandardUserCanReadButCannotAlterF1Bundle`. Use a disposable profile containing a sentinel `Shelf Suite` directory, `config.lua`, icons, themes, positions, and backup; compare their byte hashes before/after install, upgrade, and uninstall.

- [ ] **Step 2: Run the installer suite to verify RED**

  Run only on the disposable GitHub runner:

  ```powershell
  pwsh -NoProfile -File Tests/CafeInstaller.ps1
  ```

  Expected: FAIL because the installer does not yet contain the protected F1 bundle or its checks.

- [ ] **Step 3: Package the F1 bundle without user-skin actions**

  Copy only `ThirdParty/ShelfSuite/F1Compatibility` into the installer payload under `Compatibility\ShelfSuiteF1`; include the ShelfSuite F1 licence/provenance notice. Let the existing generated uninstall manifest own this protected program-data path. Do not add NSIS detection, mutation, PowerShell launch, user-profile path, or automatic F1 action.

- [ ] **Step 4: Update deployment documentation**

  Explain the two-step café workflow: install normally, sign in as the café account, unlock, preview/apply F1, retain the reported backup, then reload/restart normally. Document no-op, refusal, and manual recovery behavior. State that F2 and the PowerShell experiment are not shipped.

- [ ] **Step 5: Run installer GREEN checks and commit**

  Run the installer suite on the disposable runner and inspect the staged payload allowlist.

  Expected: PASS for protected bundle presence, standard-user read-only access, no user-skin/config access during installer operations, upgrade preservation, and safe uninstall.

  ```powershell
  git add Build/CafeInstaller/Package.ps1 Build/CafeInstaller/Installer.nsi Tests/CafeInstaller.ps1 .github/workflows/cafe-lock.yml Docs/CafeLock-Deployment.md Docs/CafeLock-Compatibility.md
  git commit -m "feat: package native ShelfSuite F1 compatibility"
  ```

### Task 6: Complete verification, review, and manual handoff

**Files:**
- Create: `Docs/superpowers/plans/2026-10-01-shelfsuite-f1-native-finalization-verification.md`
- Modify: only files needed to address justified review findings within this F1 scope.

**Interfaces:**
- Consumes: all prior focused commits and their recorded RED/GREEN evidence.
- Produces: an evidence-only verification record and a reviewable F1 installer-finalization branch; no release, merge, or deployment.

- [ ] **Step 1: Run all focused native suites**

  Run in a VS 2022 x64 Developer Command Prompt and disposable fixture environment:

  ```bat
  cl /nologo /EHsc /W4 /WX /DNOMINMAX Tests\CafeShelfF1Compatibility.cpp Library\CafeShelf\F1Compatibility.cpp /Fe:CafeShelfF1Compatibility.exe /link Bcrypt.lib
  CafeShelfF1Compatibility.exe
  cl /nologo /EHsc /W4 /WX /DNOMINMAX Tests\CafeLockPolicy.cpp /Fe:CafeLockPolicy.exe
  CafeLockPolicy.exe
  ```

  ```powershell
  pwsh -NoProfile -File Tests/CafeShelfF1NativePayload.ps1 -UpstreamDirectory work-package/ShelfSuite
  pwsh -NoProfile -File Tests/CafeShelfTests.ps1 -Suite F1Compatibility
  pwsh -NoProfile -File Tests/CafeShelfTests.ps1 -Suite F1CompatibilityApply
  pwsh -NoProfile -File Tests/CafeShelfTests.ps1 -Suite F1CompatibilityUi
  ```

- [ ] **Step 2: Request a whole-branch review**

  Use `superpowers:requesting-code-review`. Review source provenance, all `ShelfN` recognition-by-content behavior, no-config access traces, authorization at both UI/service/write boundaries, race/recovery handling, installer ownership, and F2 isolation. Address justified findings with focused RED/GREEN commits; stop for an owner decision on any design contradiction.

- [ ] **Step 3: Run the full Windows workflow at the final revision**

  Dispatch `.github/workflows/cafe-lock.yml` on `feature/f1-installer-finalization`. Confirm the revision matches HEAD and all jobs pass: x64 build/architecture, lock/password policy, standard-user runtime, ShelfSuite editor and F1 live-layout regression, native payload audit, F1 compatibility suites, installer install/upgrade/uninstall, and artifact packaging.

- [ ] **Step 4: Record only actual evidence and commit**

  Use `superpowers:verification-before-completion`, run `git diff --check`, inspect the full branch diff against `c6ce82ab`, and verify the F1 patch still has no changes. Record commits, test commands/results, workflow URL/revision, artifact checksums, no-op/refusal/recovery evidence, and untested manual items in the verification document.

  ```powershell
  git add Docs/superpowers/plans/2026-10-01-shelfsuite-f1-native-finalization-verification.md
  git commit -m "docs: record native ShelfSuite F1 finalization verification"
  ```

- [ ] **Step 5: Give the owner exact spare-PC/VM acceptance steps and stop**

  The handoff must cover: normal install; Locked Mode button absence; password unlock; preview/cancel; update of mixed recognized additional shelves; backup location; normal reload/restart; manually F1-updated **Already compatible** no-op; a customized disposable-copy refusal; safe recovery from the recorded backup; existing positions/icons/themes/launchers/passwords; and standard Windows restart/sign-out/shutdown. Do not merge, release, deploy, or start F2.

## Planning self-review

Coverage: Task 1 fixes payload provenance; Task 2 handles all recognized `ShelfN` discovery and the manually-updated no-op; Task 3 owns write/backup/recovery behavior; Task 4 owns Maintenance authorization and live Rainmeter/ShelfSuite behavior; Task 5 owns installer boundaries; Task 6 provides full workflow, review, and spare-PC validation. The plan does not alter the preserved F1 patch, import the PowerShell branch, or introduce a runtime script/dependency. Each Review Focus item has a named owning test.
