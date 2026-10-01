# Native ShelfSuite F1 finalization verification

Date: 2026-10-02 (Asia/Manila). Branch: `feature/f1-installer-finalization`.

## Preserved source and scope

- Verified F1 baseline/tag: `c6ce82ab7c2e9f901b01c712a0efd22bdee6d5c9`, `v4.5.26.3894-cafe-lock-f1.0`.
- The adaptive-tab patch is unchanged: SHA-256 `6c4913af57f1b8542d287c72b08e3ee840275cae51b7f7a96df40707891956e9`.
- ShelfSuite v2.1 remains pinned to `d4f186ba0b5c262c7559b80841132f5fd3884f3c`.
- No F2, experimental PowerShell delivery, password algorithm changes, alternate authorization route, merge, release or deployment.
- The only user-skin operation is the explicit authenticated Maintenance Settings action. Installer/startup never invokes it.
- Native compatibility inspection/application does not access `config.lua`, icons, themes, launchers, passwords or saved positions. CI-only sentinel setup/hash checks are confined to disposable fixtures.

## What was added

Exact native recognition data and shared payload under `ThirdParty/ShelfSuite/F1Compatibility`; the bounded native inspector/applicator under `Library/CafeShelf/F1Compatibility.*`; one guarded Manage Settings button and native confirmation/result dialogs; native safety/live-UI tests; protected installer runtime data and deployment instructions.

Every immediate recognized ShelfN is accepted by complete file contents, not folder number. Already-compatible files produce no writes or new backup. Any unexpected target refuses the whole operation. Changed originals are backed up and verified before moves. Recovery records are flushed before handle-bound, no-overwrite moves. Lock Now revokes later writes and recovery; incomplete materials are preserved for manual restoration.

## RED/GREEN evidence

| Task | RED evidence | GREEN evidence |
| --- | --- | --- |
| 1: payload provenance | No separate original RED transcript retained in the execution ledger; no retrospective RED claim | Exact pinned payload/catalog audit in final Windows workflow |
| 2: inspection | Missing native inspection interface and recognition/redirected-manifest assertions | Run 36872130507 at b735be4b; final workflow repeats all approved variants and unsafe-path refusals |
| 3: backup/apply | Run 36872832896: Apply stub fails Updated; 36875521426 revised gap/permissions tests; 36878577145 backup-lease failure | Run 36880703737 at a08e06fa; final workflow repeats real process interruption, PasswordSession gap revocation, competing destinations, backup leases and exact permissions |
| 4: guarded UI | 36882359009 missing policy query; 36884670630 missing Settings button; 36887273511 missing post-dispatch guard | Full run 36894674527 at 19e9a116; final workflow repeats actual standard-user native UI and existing ShelfSuite regression |
| 5: installer | Run 36895430093 at fdb9d2bf: F1BundleInstalledAndProtected missing manifest.json | Full run 36896428163 at d77b6265; exact allowlist, standard read/no-write/no-delete/no-add and install/upgrade/uninstall checks |

The UI fixture's position assertion initially captured a newly created window at 0,0 before Rainmeter finished startup. Diagnostics in 36894022888 proved before/after application remained 100,100 with the same live window. The test now waits for the configured startup position. No product movement code was changed.

## Complete Windows verification

Full successful workflow: https://github.com/BBrnen/Rainmeter-Cafe-Lock/actions/runs/36898837897

Tested final code/package revision: `66f1002f47ac05a0bca9beee322f8dca03796056`, attempt 2. All four jobs pass. The first attempt timed out at the existing WebView editor-open assertion; one unchanged rerun passed every check, including editor and installer. No timeout was increased, check removed or code changed for that rerun. This evidence-only documentation commit does not change tested executable or packaging code. Artifacts and their matching source archive belong to 66f1002f.

All four jobs completed successfully:

- `cafe-runtime-policy`: 12 compiled prerequisite decisions/results.
- `cafe-shelf-core`: 318 protocol, host, controller, selection, real WebView browser, launcher, icon, config and storage checks; zero failures. Delivery-contract audit and compiled diagnostic startup/exit-code checks also pass. This is not a new manual Explorer-drop claim.
- `cafe-f1-inspection`: compiled production handler/service entry tests with mutation RED sensitivity; pinned native payload audit; complete approved recognition catalog; missing/customized/no-shelf/modified-manifest/link/hardlink/redirected-path denial; additional shelves; manually updated no-op; policy around event dispatch; actual native apply/recovery standard-token checks. Compiled /W4 /WX on Windows.
- `cafe-lock-x64`: x64 build/architecture; password setup/change/malformed-record/no-plaintext; locked/runtime and standard-user smoke; Windows end-session messages; real native F1 UI while original skins are loaded; audited unchanged F1 patch and rendered adaptive-tab regression; existing launch/hover/tab/configurator behavior; NSIS package; installation, standard-user permissions/runtime, upgrade and uninstall.

Commands executed by that workflow include the F1 native payload audit, `Tests/CafeShelfTests.ps1 -Suite F1Compatibility`, `-Suite F1CompatibilityApply`, `-Suite F1CompatibilityUi`, compiled `CafeLockPolicy.exe`, `CafePassword.exe`, `Tests/CafeLockSmoke.ps1`, `Tests/CafeShelfSuiteRunner.ps1`, `Build/CafeInstaller/Package.ps1`, and `Tests/CafeInstaller.ps1`. See the workflow for dependent fixture setup and exact compiler/link flags. They are not recipes to run against a real installed profile.

Local verification: PowerShell parsing, scoped diff whitespace, unchanged patch/tag and working-tree checks. No local Visual Studio C++ toolchain was installed; native compilation/runtime claims come from Windows Actions only. Whole-branch whitespace check reports only the two existing intentional Markdown hard-break spaces in the approved design's Date/Branch lines; all other changed files pass with core.whitespace=cr-at-eol. The approved specification was not rewritten for formatting.

## Installer artifact

Artifact: `Rainmeter-Cafe-Lock-stage6-installer-x64` on the run above (GitHub artifact ID 11181855337). Extract the ZIP before running Setup.

Installer file: `Rainmeter-Cafe-Lock-4.5.26.3894-x64-Setup.exe`

SHA-256: `CC3B139EC8DDD05D8A03F1B45F2D983ECDA6EA2E85C00C13D59853197A054B59`

Matching source ZIP SHA-256: `EB8D3FD1661243D268DA9AA4273FD48AFC05E73C5D6735AC6F2D5087322BF5A6`.

These are file hashes printed from `dist/SHA256SUMS.txt`, not the GitHub artifact-container digest. The installer is unsigned. Do not disable Windows security if it blocks the review build; report the block instead. No signing service, subscription or additional updater is required.

## Independent review

A fresh-context whole-branch review of c6ce82ab..fb470e3d found no critical production defect. Its Important test gap was confirmed: sending ID 118 to the tray or an already destroyed Settings HWND did not exercise the actual entry guards. Tests now compile the exact production Settings case and service prologue with inert entry/path/inspection counters. They demonstrate locked denial before path/inspection access, authorized entry, reentrancy refusal, and deliberately removed handler/service guards each fail. No shipped seam or alternate invocation route was added. Existing live Lock Now tests remain.

The reviewer also found historical placement paths labelled Updated after recovery. This was regraded Important because an owner needs unambiguous recovery status. Run 36898583829 reproduced the failed-result changed-list assertion; 66f1002f clears that historical list on failure while preserving backup/manual-recovery paths and all file/auth logic. Native tests in run 36898837897 pass both findings. The full final-code Windows workflow 36898837897 now passes; finalization is ready for spare-PC/VM manual acceptance, not automatic merge or deployment. The reviewer declined no behaviors; no deferred minor findings remain.

## Approved adjustment and implementation rulings

- User-approved Task 3 adjustment at a937a47b/a620b574: identity-bound no-overwrite moves replace filename-based ReplaceFileW, with explicit interruption/revocation gap tests and durable pre-move records.
- Upstream MIT licence is force-tracked because the repository ignores .txt files; it remains included with the payload/source.
- Audits run before patching the disposable upstream fixture. Inspection has its own job in the same workflow for prompt native failures; the full x64 job retains every required check. This dedicated branch is added to the existing push trigger because browser dispatch was unavailable.
- Existing project C++ standard is retained: Win32 paths and compatible namespace syntax replace unavailable std::filesystem/nested C++17 syntax; no toolchain retarget or dependency is added.
- Preview retains trusted payload path and target identities for fresh reinspection and acquisition; no external serialization or authorization route.
- Exact ACL verification safely refuses differing-owner/inheritance cases rather than silently changing permissions. The successful standard-user fixture is created by that standard user; an administrator-created fixture separately proves safe refusal.
- A dedicated original-source live UI fixture isolates native update tests from the existing patched ShelfSuite regression. A scoped, CI-only native GUI UI Automation observer replaces unavailable console-child observation under the restricted CI token. It is not shipped and adds no product dependency.
- Existing Windows workflow is used for all C++ verification. Fixtures wait for initialized window positions and use real native UI callbacks. No checks are disabled.

## Remaining manual acceptance: spare PC/VM only

Work on a snapshot or spare-PC copy, with a personal backup. Do not deploy to cafe PCs yet.

1. Download the installer artifact from the successful run above. Extract it in Explorer. Verify the Setup file against the SHA-256 above and the extracted SHA256SUMS.txt. A read-only Command Prompt command, `certutil -hashfile "full path to Setup.exe" SHA256`, does not change security policy. Stop on mismatch or a Windows security block.
2. Unlock the existing Cafe Lock and Exit normally before Setup. Run Setup with its normal installation permission. Setup must not change ShelfSuite or create an F1 backup. Start Rainmeter from the Start menu in the cafe's normal Windows account, not Run as administrator. If a new profile appears, follow the existing deployment guide's profile migration instructions; preserve CafeLock.ini.
3. Confirm startup Locked Mode, normal launcher icons/tabs/hover, blocked skin movement/settings gear, and no available F1 update action. Enter Maintenance using the existing password. Open Manage > Settings and select Apply ShelfSuite F1 compatibility.
4. Check the displayed installation is the real configured skin folder's existing Shelf Suite. Expand Show file list. Review only @Resources shared files and immediate ShelfN/Shelf.ini targets, including extra shelves. Click Cancel; no file changes or backup should occur.
5. On the already manually updated spare-PC installation, expect Already compatible without a preview requiring changes, a rewrite or a new backup. If it refuses, preserve files and report the relative file/error; do not edit them to bypass recognition.
6. To test application, use a separate spare-PC/VM snapshot containing the exact supported stock v2.1 files and your preserved user data. Include additional shelves with approved template contents. Preview and explicitly Apply. Confirm a timestamped sibling backup was created, success lists only changed targets, and loaded skins/positions remain intact before you reload them yourself.
7. Reload affected shelves through normal Maintenance controls, or Exit and restart normally. Check short tabs retain the original compact appearance and a long name such as SCHOOL/WORK fits its padded colored background. Check following tabs do not overlap, long combined tabs grow only the needed shelf width, and the gear stays correctly placed. Check existing shelves/themes/icons/launcher actions/positions/password still work.
8. Run the action again: Already compatible, no second backup. Test Lock Now during preview: it cancels without changes. Confirm the button disappears and launchers remain usable; restart returns Locked Mode.
9. In a disposable copy/snapshot only, customize an approved ShelfN/Shelf.ini by adding a comment. Preview must refuse the entire update with a relevant relative-file message and no new backup. Restore this test change manually from your personal copy before continuing. Do not change config.lua for any F1 test.
10. For manual rollback on the disposable successful-update snapshot: Exit Rainmeter normally; read the backup's RESTORE.txt. Preserve any unexpected current target separately. Restore only listed originals from the verified backup to matching paths. Do not copy the whole shelf or touch config.lua/icons/themes/data. Restart and check the original layout works. If an actual interrupted update occurs, do not reload; retain PHASE.txt, Holding and stages and request guided help before restoration. Do not intentionally crash your production installation.
11. Test ordinary sign-out, restart and shutdown while locked, including with a password dialog open; no blocking prompt or unusual delay. Sign back in: automatic startup is locked. Record the results before any deployment decision.

Manual standard-user installation/native operation on your spare PC, visual results after native delivery, real OS session transitions, and manual recovery are still unverified by this finalization work. Hosted end-session messages are not real reboot/sign-out tests. Exact-hash refusal is intentional; unsupported customized or unsafe-permission installations are not automatically repaired.

## Focused branch commits

- `6c23e092 docs: specify native ShelfSuite F1 finalization`
- `c6d0eb3c docs: plan native ShelfSuite F1 finalization`
- `e3a61e45 build: audit native ShelfSuite F1 compatibility payload`
- `185a7068 docs: retain ShelfSuite F1 licence`
- `b649aeed test: verify native F1 payload against patched source`
- `d966ebdc feat: inspect ShelfSuite F1 compatibility natively`
- `4eaace9a fix: use Win32 paths for F1 inspection`
- `ffd68629 test: run F1 inspection against clean pinned fixtures`
- `a97de38e test: cover all F1 variants and redirected ancestors`
- `385205c5 fix: compile F1 namespace with upstream C++ standard`
- `036649b0 ci: verify native F1 inspection independently`
- `3d306a9d fix: avoid narrowing F1 catalog paths`
- `314b7304 test: diagnose exact F1 payload bytes on Windows checkout`
- `977c92ec build: preserve exact F1 payload bytes on Windows`
- `6ab9530c test: reject altered F1 catalogs and identify missing targets`
- `ff747790 test: keep full-catalog sentinel within a declared shelf`
- `5686789a fix: pin F1 inspection paths and reject redirected ancestors`
- `949cf6a8 fix: identify refused F1 targets without showing contents`
- `b735be4b fix: authenticate exact approved F1 recognition catalog`
- `f505318f test: specify native F1 backup and recovery behavior`
- `9248d2f1 test: provide compile-only F1 fault injection seam`
- `a937a47b docs: propose Task 3 replacement safety clarification`
- `a620b574 docs: approve identity-bound F1 replacement safeguards`
- `d4eaf2c3 test: cover F1 move-gap interruption revocation and permissions`
- `76d38589 feat: apply F1 compatibility through identity-bound file moves`
- `b780d484 test: verify F1 busy-target refusal and standard-user operation`
- `021bf80a test: assert outside-edit injection on hidden F1 targets`
- `ff07684d test: verify retained hidden originals after F1 interruption`
- `69b9e3ba test: cover real session revocation and recovery destination races`
- `3e8ecc40 test: include memory for F1 session-revocation fixture`
- `79f5fe8b test: use owner-writable standard-user F1 fixtures`
- `07024d7b test: exercise competing recovery files and backup leases`
- `b7891a3d fix: retain verified F1 backups against competing writes`
- `cc1c3280 test: diagnose standard-user F1 permission mismatch`
- `a6e1604e fix: avoid re-expanding inherited F1 staging permissions`
- `96c2ca74 fix: verify original permissions before F1 file moves`
- `a08e06fa test: distinguish user-owned skins from unsafe permission changes`
- `25173b0d test: require Maintenance authorization for native F1 action`
- `63d51806 test: exercise native F1 action with loaded standard-user shelves`
- `1637989b test: run native F1 UI regression before policy gate`
- `6c174c84 test: use existing runtime window identity conventions`
- `6b2ef095 test: create protected fixture sentinel before UI checks`
- `1971f7c2 test: compare complete F1 fixture hash sequences`
- `4af02ef0 test: send real Lock Now during native F1 application`
- `6674aed8 test: handle zero backups under strict PowerShell mode`
- `043f42e9 feat: add Maintenance Mode native ShelfSuite F1 action`
- `a266559a test: verify native F1 preview paths and file list`
- `1f54e17e test: drive native TaskDialog through Windows interfaces`
- `4b8d85d3 test: require F1 authorization after Windows event dispatch`
- `bee7e96d fix: recheck F1 authorization after Windows event dispatch`
- `b2d5e617 test: redirect CI UI Automation reader output explicitly`
- `d862529e test: observe F1 preview with a standard-user native probe`
- `d778708c test: report precise native preview observer failure`
- `0361a099 fix: label native F1 file-list expansion correctly`
- `e12a4a88 test: dismiss F1 results through their real OK control`
- `705a97e7 test: invoke native F1 result OK through UI Automation`
- `1530c1d6 test: trace loaded shelf position across native F1 apply`
- `204a7215 test: include positions at F1 apply boundary`
- `19e9a116 test: wait for ShelfSuite startup position before F1 snapshot`
- `fdb9d2bf test: require protected F1 installer bundle and shelf preservation`
- `d77b6265 feat: package protected native ShelfSuite F1 compatibility`
- `fb470e3d docs: keep native F1 and desktop acceptance sections distinct`
- `4b998635 test: verify actual F1 entry guards and accurate recovery results`
- `66f1002f fix: report only completed F1 updates after recovery`

The final evidence-only commit adds this verification record; executable/package code remains 66f1002f.

## Retained execution record

```text
# SDD ledger — plan: Docs/superpowers/plans/2026-10-01-shelfsuite-f1-native-finalization.md
Pre-flight: Task 1 produces source payload/catalog; Task 2 consumes installed bundle. Task 2 produces Preview/Apply; Task 3 extends Apply. Task 4 consumes service. Task 5 consumes payload. Task 6 consumes all. No interface conflicts; spec controls.
Task 1: complete (commits c6d0eb3c..185a7068, tests: pwsh -NoProfile -File Tests/CafeShelfF1NativePayload.ps1 -UpstreamDirectory work-package/ShelfSuite-F1-base -> PASS)
Task 1: Ruling: retained the upstream MIT licence as a force-added .txt source file because the repository globally ignores *.txt — cost if wrong: attribution would be omitted from the source payload.
Task 2: ongoing — native code has not passed Windows compilation/tests. Run 36865845678 failed because std::filesystem is unavailable under the existing project standard; 4eaace9a replaces it with Win32 paths without retargeting.
Task 2: Ruling: run payload audit and inspection fixtures before patching the disposable upstream checkout — the clean-source assertions require original pinned bytes; changing step order preserves their strength.
Task 2: Ruling: add this dedicated feature branch to the existing workflow push trigger because browser dispatch is unavailable — no checks removed; Windows remains authoritative.
Task 2: test harness corrections: resolve upstream path before changing directory, avoid duplicating existing DynamicWindowSize, return after standalone inspection suite. Native GREEN remains unverified.
Task 2: Windows run 36870261340 RED: C2429 nested namespaces require C++17. Replaced only namespace syntax with C++14-compatible nested declarations; project standard unchanged. Native tests were skipped, not passing.
Task 2: Ruling: separate inspection/audit into an independent job in the same Windows workflow — compile/test diagnostics no longer wait for unrelated live desktop tests; full x64 job remains required and unchanged.
Task 2: run 36870677873 failed under /W4 /WX: C4244 from narrowing a wide catalog path. Compare the original JSON ASCII allowlisted path instead; warnings remain errors.
Task 2: native /W4 /WX compilation passed on run 36870830857, first runtime fixture refused. Investigating exact payload checkout bytes (Git autocrlf) with stronger audit; no hashes broadened.
Task 2: reproduced payload mismatch with checkout-index core.autocrlf=true: ShelfEngine SHA256 47e5f1b4... instead of approved 7b800838... . Scoped attributes preserve exact approved LF payload/manifest bytes; verified F1 patch unchanged.
Task 2: run 36871114520 passed mixed/no-op/partial/refusal cases after LF fix. All-variant fixture failed because sentinel creation added a Shelf1 folder with no INI; number full-catalog fixtures Shelf1..64 so its sentinel belongs to a declared healthy shelf.
Task 2: RED confirmed on 36871361476: all 64 approved INIs recognized, redirected ancestor erroneously accepted. Fix pins all directory ancestors without delete sharing, refuses reparse/case-sensitive/ambiguous paths, and closes enumeration handles safely. Await Windows GREEN.
Task 2: run 36871746670 GREEN for all variants, redirected ancestors and hard links; RED for missing-target diagnostics (generic refusal). Preserve relative inspection context and safe internal error reasons; never include file contents.
Task 2: run 36871894501 GREEN for missing-file diagnostics; RED for modified manifest accepting customized target. Pin the exact approved LF manifest SHA256 1b4869f15aa72ce9febc31ccf3b8df10f0cbbf91e8b7f82755386526affe0aed before parsing; source catalog/payload unchanged.
Task 2: complete (commits d966ebdc..b735be4b; Windows run https://github.com/BBrnen/Rainmeter-Cafe-Lock/actions/runs/36872130507 at b735be4b; cafe-f1-inspection succeeded: /W4 /WX x64 compile, payload audit, all 64 approved INI variants, mixed additional shelves, manual no-op, partial update, unknown/no-shelf refusal, locked data sentinels, junction/hard-link denial, relative missing-file error, altered manifest denial). Full workflow still pending; no final-release claim.
Task 3: RED build evidence 36872570499: test-only injection setter absent. Added only compile-gated setter (not shipped); Apply still stub so application behavior RED will be verified before implementation.
Task 2: full Windows workflow at b735be4b run 36872130507 completed SUCCESS (all jobs). Verified baseline/tag and F1 patch hash remain intact.
Task 3: runtime RED confirmed on run 36872832896 at 9248d2f1: compiled tests fail Updated assertion because Apply remains unsupported.
Task 3: PAUSED for owner decision, not task complete. Security/plan contradiction: checking identity then ReplaceFileW by filename cannot condition replacement on verified identity under concurrent rename. Proposed narrow handle-bound no-overwrite replacement/recovery with explicit interruption gap is documented in Docs/superpowers/plans/2026-10-01-shelfsuite-f1-task3-safety-clarification.md. Approved design/plan unchanged; no product write implementation yet.
Task 3: resumed on explicit owner approval of a937a47b with safeguards: flush backup and phases before moves; gap revocation performs no writes and reports manual recovery; retain permissions/attributes; no competing-file overwrite. Only Task 3 spec/plan wording amended.
Task 3: revised tests compiled RED on 36875521426 at d4eaf2c3 (expected update absent). Implemented handle-bound leases/moves with replacement disabled; backup/record flush before moves, original DACL/basic attributes on stage, gap revocation no further writes; Windows verification pending.
Task 3: Ruling: retain payload path and target identity in native Preview — needed for fresh catalog/reinspection and identity-bound acquisition; no external serialization or authorization route added.
Task 3: 36876671160 compiled native code and passed success/DACL/attributes, denied, backup failure and partial recovery. Outside-edit test fixture failed to write a hidden file through ofstream; explicit OPEN_EXISTING plus asserted WriteFile/FlushFileBuffers now proves injection actually occurs, without removing hidden-attribute coverage.
Task 3: run 36877241483 passed update, permissions/attributes, busy, denial, backup failure, safe rollback, outside edit, no-op, gap interruption recovery, gap revocation and competing-file refusal. Crash verifier omitted hidden originals because Get-ChildItem lacked -Force; retain hidden-attribute coverage and enumerate them explicitly. Standard-token check awaits this gate.
Task 3: crash-gap verification passed 36877746412; standard-token fixture failed before Apply because elevated checkout ACL grants ordinary Users read-only. Ruling: grant only runner owner SID Modify on the disposable standard Skins subtree, matching a normal user-owned skin directory; no world/admin grants or product ACL changes.
Task 3: real PasswordSession revocation in the move gap passed on 36878034653. Competing recovery-destination fixture RED because its injection boundary was missing; added compile-gated no-op-in-production boundary before restoring original. Added test-first immutability check for verified backups while originals move.
Task 3: verified-backup immutability RED on 36878577145 (backup could be opened for write/delete before moves). Keep read-only, no-delete-sharing backup leases through update/recovery, checking identity and hash at acquisition. No extra data paths.
Task 3: run 36879804014 confirmed standard-user mismatch: original four ACEs, output five, extra inherited owner Full Control. Explicit UNPROTECTED_DACL_SECURITY_INFORMATION re-expands CREATOR OWNER for the new staging owner. Omit redundant inheritance re-enable for already-unprotected new stages; unchanged exact-DACL test remains the gate. Browser suite transient failure on previous runner passed on this run without code changes.
Task 3: removing redundant inheritance re-enable did not prevent Windows CREATOR OWNER expansion. Exact production DACL verification now refuses before any original move. Ruling: standard-user success fixture must be created by that standard user, matching user-owned skins; separately retain admin-created fixture and assert safe refusal with unchanged original permissions. Do not grant broader rights or bypass inheritance.
Task 3: complete (commits b735be4b..a08e06fa; Windows 36880703737 cafe-f1-inspection SUCCESS: payload audit, inspection, F1CompatibilityApply all scenarios including real interruption, actual PasswordSession gap revocation, competing rollback destinations, immutable backups, exact permissions/attributes, standard-user-owned success and differing-owner safe refusal). Local C++ remains unverified; Windows compiled /W4 /WX and executed tests.
Task 4: BASE a08e06fa; use dedicated Tests/CafeShelfF1Ui.ps1 behind approved F1CompatibilityUi suite to isolate original-source live updates from existing patched-layout regression fixture. No runtime route or dependency added.
Task 4: policy RED confirmed 36882359009 C2039/C3861 missing AllowsF1Compatibility. Live fixture corrections before product code: use full-path meter title/DummyRainWClass, create only disposable no-access config sentinel, parenthesize joined hash comparisons, count empty backup arrays under StrictMode. Local parser and C# observer compilation pass. Run 36884670630 stopped earlier at unchanged smoke restart timeout; rerun only failed job, no checks skipped.
Task 4: live UI RED confirmed on rerun 36884670630: Timed out F1 Settings button, after loaded stock shelves, locked/forged denial and real password setup. Implement confined Settings action, native explicit Apply/Cancel/Lock Now preview, read-only result states, stable tray-owned dialogs, reentrancy guard and operation-only revocation latch. Existing password/session state unchanged; normal UI messages are serviced at write authorization boundaries.
Task 4: 043f42e9 built x64; live test reached TaskDialog but ordinary HWND button search failed. Use documented TDM_CLICK_BUTTON to invoke the real callback and built-in Windows UI Automation (CI only) to inspect DirectUI preview content. Product authentication unchanged; no runtime PowerShell/dependency.
Task 4: self-inspection found a necessary authorization boundary: PeekMessage can dispatch sent Lock Now even when it returns no queued message. Add a production-used policy helper tested for revocation inside dispatch, cancellation latching and dispatch denial; then recheck current authorization after dispatch, not only inside the queued-message loop. No new unlock mechanism/session change.
Task 4: post-dispatch policy RED confirmed 36887273511 C2660 missing overload. Added production-used before/after dispatch guard, including cancelled-operation latch. This handles sent-message revocation even if PeekMessage returns no queued message; no session authorization rules changed.
Task 4: post-dispatch policy and native recovery suites GREEN on 36887676024. UI Automation console reader could not start under filtered token (Access denied), matching existing runner console-child limitations. Replace only the test observer with a Windows GUI-subsystem native UI Automation probe, scoped to the exact F1 dialog/process and a new disposable result file. Never package it; runtime remains unchanged and has no scripts/dependencies.
Task 4: native observer compiled and successfully validated preview root/count/backup on 36890371369; RED at file-list expander. Root cause: TaskDialog expanded/collapsed control labels were reversed. Microsoft TASKDIALOGCONFIG docs identify CollapsedControlText as the expansion button. Swap only those labels; keep Show file list assertion intact.
Task 4: 36891453578 GREEN for preview text/file-list, cancel, preview Lock Now, reauthentication and busy refusal. Test then failed dismissing the native result MessageBox through synthetic menu-style WM_COMMAND. Click its actual IDOK button with BM_CLICK after the control exists; no product change.
Task 4: modern native result message box also exposes DirectUI controls (no IDOK child HWND). Reuse the already working, process/title-scoped native UI Automation observer to invoke its actual OK button. No product UI/auth changes and no skipped assertions.
Task 4: run 36894022888 diagnostics proved startup HWND appears at 0,0 before initialization finishes; configured and before/after native apply positions are both 100,100, same HWND alive. Wait on configured startup position before snapshot; retain unchanged-window/position assertion. No product change.
Task 4: complete at 19e9a116, Windows run 36894674527: native live UI, post-dispatch policy, patched ShelfSuite standard-user regression GREEN. Native Apply preserves initialized HWND/position; locked/forged denial, preview file-list/cancel, Lock Now before/during Apply, busy refusal, additional shelves, update/no-op/custom refusal passed. Task 5 BASE 19e9a116; add runtime allowlist/protection and disposable shelf/profile/backup sentinel installer tests before packaging.
Task 4: full workflow 36894674527 at 19e9a116 completed SUCCESS, all four jobs including unchanged installer suite. Final F1 bundle packaging still pending Task 5.
Task 5: installer RED verified 36895430093 at fdb9d2bf: F1BundleInstalledAndProtected missing manifest.json. Add only five-file protected runtime allowlist with copied-byte verification, existing generated uninstall ownership, optional Maintenance instructions, emitted dist checksums. No user-skin installer action. Local PS parse/diff checks pass; Windows GREEN pending.
Task 5: complete at d77b6265 (plus fb470e3d documentation heading cleanup), full Windows run 36896428163 SUCCESS all four jobs. Installed five-file exact bundle, standard read/no-write/no-delete/no-new-file, install/upgrade/uninstall fixture preservation, owned bundle removal and existing installed runtime/password checks GREEN. Installer SHA256 3FF7EB24578AE6DA7C233E3964355D3CE2100C1BC97B6DA09ABDC84C64D37AC1. Task 6: BASE fb470e3d; focused suites compiled/run in same full Windows workflow; fresh whole-branch review pending.
Final review at fb470e3d: no critical defect; Important actual-entry coverage gap verified (tray id does not route; Settings HWND already destroyed). Fix in one pass with source-derived compiled production handler/service prologue and inert entry/path/inspection counters, positive authorized sanity and deliberate guard-removal RED sensitivity; no shipped test seam or new invocation route. Existing live Lock Now tests retained. Reviewer Minor recovered paths labeled Updated regraded Important: confusing an owner during incomplete/recovered safety operation violates clear final-state reporting. Add failed-result changed-list RED assertion, then clear historical placement list on failure. Declined-to-judge list empty.
Final fix pass: run 36898583829 at 4b998635 compiled exact production entry prologue/handler; service-guard mutation and handler-guard mutation each RED (counter failure), unmodified production GREEN before paths/inspection; authorized sanity/reentrancy GREEN. Recovery reporting RED at apply-recover assertion changed.empty(). Clear historical changed list only on failed operation; retain actual file/recovery/auth logic and backup/manual paths unchanged. Full final-code Windows GREEN pending.
Task 6: final code 66f1002f run 36898837897: all independent jobs GREEN, main x64/native F1 UI and adaptive-layout/shortcut/locked ShelfSuite tests GREEN; unchanged WebView editor-open regression timed out before installer step. Inspect existing boundary; no relevant host/editor code changed by review fix. Rerun only failed x64 job unchanged once, retain all checks; do not classify timeout as resolved until rerun passes.
Task 6: complete; fresh whole-branch review at fb470e3d, one test-first fix pass 4b998635..66f1002f, mutation RED/production GREEN entry coverage and recovery-reporting RED/GREEN verified. Full final-code Windows run 36898837897 at 66f1002f attempt 2 SUCCESS all four jobs; attempt 1 existing editor startup timeout, unchanged failed-job rerun passed. Installer artifact 11181855337, exe SHA256 CC3B139EC8DDD05D8A03F1B45F2D983ECDA6EA2E85C00C13D59853197A054B59; source SHA256 EB8D3FD1661243D268DA9AA4273FD48AFC05E73C5D6735AC6F2D5087322BF5A6. All rulings/evidence and beginner manual acceptance steps retained in verification document. No deferred minors, merge, release, deployment or F2. Keep branch/worktree for owner acceptance.

```
