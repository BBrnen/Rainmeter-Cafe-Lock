# Working on Rainmeter Cafe Lock

## Goal and current behavior
Help the owner, who is not a programmer, make small, verified changes to this Windows Rainmeter derivative. Explain outcomes in plain language; handle repository navigation and commands yourself. Ask about missing product decisions, not routine implementation details.

Current source uses password-based Maintenance Mode, not the superseded UAC helper design in the Stage 1 plan. Startup is locked; unlock is process-local; Lock Now and restart revoke it. Preserve normal launcher clicks, tabs, hover, meters and Lua while blocking editing/movement when locked. Do not change application behavior during workflow/documentation preparation.

## Repository map
- `Rainmeter.sln`: Visual Studio solution. `Application/`: executable entry point/resources.
- `Library/`: Rainmeter runtime. `Skin.cpp`: skin interaction, dragging/modifier/keyboard movement and message guards. `CommandHandler.cpp`: bang and launch dispatch. `Rainmeter.cpp`: lifecycle and direct management operations.
- `Library/CafeLock.h`, `CafeLock.cpp`: lock policy, native password dialogs, session transitions and restricted tray menu.
- `Common/CafePassword.h`: password verification/storage and session authorization.
- `Library/ContextMenu.cpp`, `TrayIcon.cpp`, `DialogManage.cpp`: menus, tray and maintenance settings. Guards must cover direct entry points as well as visible menus.
- `Common/`, `Plugins/`, `Language/`, `SkinInstaller/`, `Restart/`: shared code, plugins, localization and companion programs. Preserve upstream interfaces.
- `Build/Build.bat`: upstream build; `Build/CafeInstaller/`: Cafe Lock packaging; `Build/Skins/` and `Build/Layouts/`: shipped defaults.
- `Tests/`: Cafe Lock policy, password, running-app, ShelfSuite and installer regression tests.
- `.github/workflows/cafe-lock.yml`: authoritative Windows build/test/package sequence.
- `Docs/CafeLock-Stage4.md`, `CafeLock-Deployment.md`, `CafeLock-Compatibility.md`: password, deployment and compatibility details. Earlier stage notes describe historical checkpoints; resolve conflicts against current source and workflow.

## Safe change workflow
1. Inspect status, branch, diff and applicable instructions before editing. Preserve uncommitted user work. Never reset, clean, overwrite or stash it without understanding ownership.
2. Use a dedicated branch such as `feature/<short-description>` from the reviewed current base. This setup lives on `feature/codex-workflow`. Use a worktree only for actual isolation needs; check whether another chat relies on a checkout before switching it.
3. Translate the request into a short plan and observable acceptance criteria: trigger, expected result, behavior in Locked and Maintenance modes, and compatibility expectations. Resolve essential ambiguity before implementation.
4. Implement the smallest focused change. For behavior changes, add/update a meaningful regression test, demonstrate the failure before the fix when practical, then make it pass. Do not add artificial tests for prose-only edits.
5. Run the verification below, inspect the final diff, and review for bypasses, regressions, user-data loss and unrelated changes. If Superpowers is installed and its skills are available, read and use its applicable planning, testing, debugging and review instructions. Do not claim it is installed from a suggestion alone. Without it, follow this same plan/test/review sequence.
6. Report the branch, changes, verification evidence and any remaining limits in plain language. Do not call unverified behavior complete. Keep changes reviewable; merge, release and deployment are separate actions requiring user authorization.

## Compatibility and scope rules
- Preserve upstream ancestry, license and attribution. Baseline: Rainmeter `v4.5.26.3894`, commit `5a124b6a09e2f7f67f8be9232718c489100e6173`; upstream is https://github.com/rainmeter/rainmeter.
- Keep changes narrow. No unrelated refactors, bulk formatting, dependency upgrades or skin rewrites. Match each file's existing style and line endings.
- Preserve normal skin/plugin APIs and ShelfSuite behavior. CI pins unchanged ShelfSuite v2.1 at `d4f186ba0b5c262c7559b80841132f5fd3884f3c`; do not silently change that pin.
- Never add a persistent unlock flag, unlock bang, backdoor password, plaintext password logging or command-line password. Preserve fail-closed handling of malformed credentials.
- Leave inherited signing/WinGet workflows in `.github/upstream-workflows/` inactive. Do not use upstream publishing identities.
- Review stable upstream updates on a separate branch, preserving history. Do not replace the source with a ZIP or force-push shared history.
- Do not test against the owner's installed Rainmeter, real profile or customer machines. Do not kill unrelated Rainmeter processes. Use disposable Windows runners/VMs for GUI and installer tests. Never fake `GITHUB_ACTIONS=true` to bypass test safeguards.

## Build, tests and lint
Commands below match the inspected workflow. Re-read it if it changes. Required tools: Windows, Visual Studio 2022 with the solution's C++/Windows SDK requirements, PowerShell 7; NSIS 3.11 for packaging. `Build.bat` locates Community or Enterprise installations. Do not silently retarget the solution to work around missing tools.

Build from a Command Prompt at repository root:
```bat
cd Build
set CI=true
call Build.bat rainmeter-64 4.5.26.3894
```
Check the exit code immediately; stop on failure. Output is `x64-Release/` (language output also uses `x32-Release/`). `CI=true` only suppresses the batch pause; it does not authorize integration tests.

In an x64 Visual Studio 2022 Developer Command Prompt at repository root, run each command separately and stop if compilation or execution returns nonzero:
```bat
cl /nologo /EHsc /W4 /WX /DNOMINMAX Tests\CafeLockPolicy.cpp /Fe:CafeLockPolicy.exe
CafeLockPolicy.exe
cl /nologo /EHsc /W4 /WX /DNOMINMAX Tests\CafePassword.cpp /Fe:CafePassword.exe /link Bcrypt.lib
CafePassword.exe
```

Full verification uses the existing GitHub Actions workflow on a disposable Windows runner. It additionally compiles `RunAsStandard.exe` and `CafeLaunchProbe.exe`, verifies x64 binaries, then runs:
```powershell
pwsh -NoProfile -File Tests/CafeLockSmoke.ps1 -Maintenance
./RunAsStandard.exe "C:\Program Files\PowerShell\7\pwsh.exe" -NoProfile -File Tests/CafeLockSmoke.ps1 -StandardUser -Maintenance
./Tests/CafeShelfSuiteRunner.ps1 -ShelfSuiteDirectory work-package/ShelfSuite
./Build/CafeInstaller/Package.ps1
./Tests/CafeInstaller.ps1
```
These are dependent steps, not a standalone local test recipe: use the workflow's helper builds, pinned ShelfSuite checkout, NSIS installation and fresh staging. Smoke tests require an isolated desktop and `RUNNER_TEMP`; installer and ShelfSuite tests alter disposable-machine state and have runner safeguards.

The workflow currently runs for PRs targeting `main` and certain stage branches, or by manual dispatch. Pushes to arbitrary feature branches do **not** trigger it. For application changes, open an authorized PR to `main` or run the workflow for the feature branch through GitHub; verify the run's tested revision matches the final change. Do not mistake an old green run for validation of new code.

There is no dedicated repository-wide lint command in the inspected workflow. Use compiler diagnostics, the tests' `/W4 /WX`, and `git diff --check`; do not invent npm/lint commands or impose a formatter. `Build.bat` excludes upstream Native Unit Tests. For changes touching upstream-tested components, also build/run the relevant tests in Visual Studio Test Explorer as described in `Docs/UnitTests.md`; a release build alone does not run them.

## Generated-file and user-data boundaries
- Do not hand-edit or commit build outputs: `x32*/`, `x64*/`, objects, test executables, `bin/`, `obj/`, `.vs/`, `dist/`, `work-package/`, installer binaries and checksums.
- `Build.bat` rewrites `Version.h`, `Version.cs` and `Language/Language.rc`; installer builds generate `Build/Installer/Languages.nsh`. Some generated files are tracked despite ignore rules. Inspect their diffs; exclude incidental generated changes. Do not blindly restore a file that had pre-existing user edits.
- Edit packaging sources, not `work-package/payload` or `DeleteFiles.nsh`. Packaging requires fresh staging and archives committed HEAD; never publish binaries alongside a source archive that omits their changes.
- Preserve real `Rainmeter.ini`, `CafeLock.ini`, skins, layouts and passwords. Test fixtures must use disposable profiles. Keep credentials and machine-specific paths out of commits.

## Definition of done
- The requested behavior and acceptance criteria are satisfied with no unrelated changes.
- Final diff reviewed; whitespace and documentation/path/command checks pass; generated artifacts and private data are excluded.
- For application/build/test changes: fresh x64 build and relevant tests pass; the full existing Windows workflow passes before declaring the feature ready. Record revision, run link and results. Failures must be fixed or clearly reported as blocking, never silently skipped.
- For documentation-only changes: validate paths/commands against current source and review the complete diff; no application rebuild is required when executable content is unchanged. If local tools are unavailable, verify the remote diff and say which local checks were not run.
- Record manual checks separately. Real sign-in/restart/shutdown/sign-out and actual browser configurator saving require a spare PC/VM per `Docs/CafeLock-Compatibility.md`; a green CI run does not prove them.
- The owner receives a short explanation of what changed, what passed, what remains untested, and the next action. No merge or release is implied by completion.
