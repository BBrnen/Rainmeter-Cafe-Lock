# ShelfSuite Launcher Usability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. The owner must review this plan and select the execution method before implementation.

**Goal:** Let the owner browse or drop Windows launchers and icons into the existing ShelfSuite configurator during Maintenance Mode, then save safely without editing Lua.

**Architecture:** Host a narrowly adapted copy of the pinned configurator in a Rainmeter-owned WebView2 window. Native components inspect selected files, prepare PNG icons, parse configuration as data and coordinate writes with the live lock state. Keep the original installed skin, engine and configuration schema.

**Tech Stack:** Existing Visual Studio 2022 C++/Windows SDK toolchain; Win32/COM shell and WIC APIs; Microsoft.Web.WebView2 SDK 1.0.4258.31; Evergreen Runtime; existing Library/json/json.hpp (3.10.1); PowerShell 7; NSIS 3.11.

**Spec:** [Approved design](../specs/2026-09-30-shelfsuite-launcher-usability-design.md), approved by the owner after commit 526bfa26363c5dedf75a04859f9c420ad5aac3cf. That immutable spec's status line predates approval; this plan records approval without rewriting the reviewed artifact.

## Global Constraints

- Work on `feature/cafe-lock-usability`; preserve user work and upstream ancestry.
- Rainmeter v4.5.26.3894 baseline: `5a124b6a09e2f7f67f8be9232718c489100e6173`.
- ShelfSuite v2.1 pin: `d4f186ba0b5c262c7559b80841132f5fd3884f3c`.
- No second skin/profile, ShelfSuite upgrade, unrelated refactor or engine rewrite.
- Startup locked; process-local password unlock only; Lock Now/restart revoke it.
- Preserve normal launchers, tabs, hover, meters and Lua while locked.
- No new unlock bang, persistent flag, plaintext password exposure, external editing service or elevation of Rainmeter.
- Editor scripts are bundled trusted assets. Every operation is native-authorized.
- Retain ShelfSuite MIT attribution, Rainmeter licensing and dependency notices.
- Preserve real profiles; UI, association and installer tests use disposable Windows runners/VMs only.
- Full existing Windows workflow must pass on the final tested revision.
- Commit focused source/test changes; exclude binaries, private paths, generated noise.
- No merge, release or deployment without separate owner authorization.

## Review Focus

1. A dialog/extraction/save callback finishing after Lock Now or after re-unlock must never revive authorization (Tasks 1, 2, 5, 6).
2. A .lnk with arguments, a custom working directory, a custom icon or an unavailable target must remain the same shortcut action (Tasks 3, 4, 7).
3. Existing Lua with comments, escapes, tuple items, unknown fields or executable statements must not be silently simplified or overwritten (Task 5).
4. A concurrent edit, case-insensitive filename collision, disk failure or redirected destination must preserve previous user data (Tasks 4, 5, 7).
5. Dropped names containing Unicode, HTML, quotes, brackets, hashes or percent signs must survive all supported encoding layers without becoming script/commands (Tasks 1, 3, 5, 6).

## Boundaries, dependencies and exact files

Create focused modules under `Library/CafeShelf/`:
- `Types.h`: shared data/result types and limits.
- `Session.h/.cpp`: editor generations, selection lifetime and operation gating.
- `Protocol.h/.cpp`: bounded JSON request/response validation.
- `Host.h/.cpp`: WebView2 window, approved resources, lifecycle and dispatch.
- `Selection.h/.cpp`: native pickers and actual dropped-file selection.
- `Launcher.h/.cpp`: Windows name/path/icon-source inspection.
- `Icons.h/.cpp`: PNG/ICO decoding, shell icon extraction and PNG encoding.
- `Config.h/.cpp`: safe Lua data parsing and focused edits.
- `Storage.h/.cpp`: containment, snapshots, backups, unique files and commits.
- `Controller.h/.cpp`: connect validated UI requests to those operations.
- `EditorResources.rc`: named RCDATA resources for editor assets.

Create `Library/CafeShelf/UI/configurator.html`, `configurator.js`,
`configurator.css`, `LICENSE-ShelfSuite.txt`, and `UPSTREAM.md`.
Derive the UI from the exact pinned source, recording original file SHA and
focused adaptations; do not replace the owner's installed configurator file.

Modify `Library/Library.vcxproj`, its `.filters`, `Library/Library.rc`,
`Library/CommandHandler.cpp`, `Library/CafeLock.cpp` and `.h`.
Register the new modules explicitly and disable precompiled headers for the
standalone-testable modules, without changing global compilation options.

Create `Build/CafeDependencies/Restore.ps1`, `dependencies.json`,
`WebView2.props`; modify `Build/Build.bat` to call restore and stop on failure.
Pin SDK 1.0.4258.31 from NuGet; at acquisition record and verify its SHA-256
and package signature before committing the manifest. Do not invent a digest.
Use its static WebView2 loader for each built architecture, matching the existing
static runtime configuration. Do not upgrade the existing JSON library.
Keep downloads/extraction in ignored build staging; support a verified offline cache.

Embed UI resources in Rainmeter.dll so user-modifiable HTML never gains native
authority. Serve only `https://cafe-shelf.invalid/index.html`, `/app.js`,
`/app.css` from those resources. Preview images are native-generated PNG data;
do not map a writable skin directory into the privileged origin.

Create tests:
`Tests/CafeShelfCore.cpp`, `CafeShelfWindows.cpp`,
`CafeShelfHostHarness.cpp`, `CafeShelfUi.ps1`,
`CafeShelfFixtures.ps1`, `CafeShelfTests.ps1`.
Extend existing CafeShelfSuite, smoke, launch-probe and installer tests where noted.

## Shared contracts and limits

All C++ types below live in namespace `CafeShelf`.
Use `Result<T>` with fields `bool ok`, `T value`, `Error code`,
`std::wstring message`; `Status` has the same fields except value.
Errors are typed: Locked, Stale, InvalidInput, Unsupported, NotFound, Conflict,
AccessDenied, IoError, Cancelled, RuntimeMissing. Log error codes, not credentials.

- `Ticket { uint64_t editor, generation; }`: native-issued, checked against
  live state; possession is never sufficient authorization.
- `SelectionId = uint64_t`: opaque native record; never interpreted as a path.
- `SelectedFile { std::wstring path; bool directory; }`: created only by native
  selection; byte content of EXE/files is not exposed to the page.
- `LauncherDraft { std::wstring name, action, iconSource; }`.
- `PngImage { std::vector<uint8_t> bytes; uint32_t width, height; }`.
- `FileVersion`: handle-derived file identity plus SHA-256 of source bytes.
- `ShelfDocument`: original bytes/encoding, source spans, supported data model.
- `EditBatch`: typed shelf/tab/item/theme changes; it carries no raw Lua or
  destination paths. Item values use name/action/icon or a SelectionId for a
  prepared import.
- `SaveResult { FileVersion version; std::vector<std::wstring> newIcons;
  std::wstring backup; }`.
- `using Json = nlohmann::json` from the existing bundled header.

Limits: inbound JSON 1 MiB, JSON nesting 32, strings 32,767 UTF-16 code units,
config source 4 MiB, source nesting 64, PNG/ICO input 32 MiB, decoded image
16 megapixels and 4096 pixels per dimension. Normalize imported previews/icons
to fit within 256 by 256, preserving aspect ratio and alpha; do not upscale.
Do not apply the PNG/ICO file-size ceiling to an EXE: extract bounded icon
resources rather than reading an entire executable as an image.
Exceeding a limit gives an error without changing files.

One selected launcher per Add/Edit operation; multiple dropped entries receive
a clear request to select one. Folder import means the folder itself, never
recursively importing its contents.

No arbitrary new tab/item ceiling: discover each installed Shelf.ini's available
tab/item meter slots and enforce those capacities without rewriting its engine.
Recognize the pinned layout; incompatible customized layouts remain untouched.

## Preflight and verification commands

Before implementation, inspect `git status --short --branch`, `git diff`,
`git diff --cached`, branch refs and applicable AGENTS files. Fetch the feature
branch only after checking local ownership. Do not switch/reset/stash user work.
Use a worktree only if isolation is needed, following the native worktree tool
and applicable skill. Local runner failure is a preflight blocker for local
implementation, not grounds to fabricate a clean checkout.

First run the existing workflow on the unmodified approved base by manual
dispatch and verify the actual tested SHA. No ordinary feature-branch push is
assumed to trigger CI.

Task 1 creates `Tests/CafeShelfTests.ps1 -Suite <name>`, run from an x64
VS 2022 Developer Command Prompt through PowerShell 7. Suites are
Protocol, Host, Launcher, Icons, Storage, Ui, All. The script compiles needed
new modules with `cl /nologo /EHsc /W4 /WX /DNOMINMAX`, explicitly listing
sources and Windows libraries, and stops at every nonzero compile/test exit.
It emits named PASS/FAIL cases and exits nonzero on any failed assertion.
No tests exercise installed user profiles.

Build verification uses `Build/Build.bat rainmeter-64 4.5.26.3894` with
`CI=true` to suppress pause, not to authorize integration tests.
Full verification remains `.github/workflows/cafe-lock.yml`.
Run `git diff --check` and inspect staged files before every focused commit.

## Task 1: Revocable editor session and strict message contract

**Files:** Create Types, Session and Protocol modules and
`Tests/CafeShelfCore.cpp`, `Tests/CafeShelfTests.ps1`.
Modify project entries when production integration first consumes these modules.

**Interfaces:**
- `Ticket Session::Open(bool locked)`.
- `void Session::Revoke()`.
- `bool Session::Allows(Ticket ticket, bool locked) const`.
- `Result<Request> DecodeRequest(const std::string& utf8)`.
- `Json EncodeResponse(const Response& response)`.
- Request contains a monotonically increasing id, operation enum and validated
  payload; native envelope supplies source URI and Ticket, never trusts those
  fields from page JSON.

- [ ] Write Protocol tests with these assertions: locked Open cannot authorize;
  valid ticket works only unlocked; Revoke rejects that ticket even after new
  Open; wrong editor rejects; duplicate/unknown operations, invalid JSON,
  oversized input, excessive nesting and invalid strings reject. Response values
  containing `</script>`, quotes and Unicode survive JSON round trips as data.
- [ ] Run `pwsh -NoProfile -File Tests/CafeShelfTests.ps1 -Suite Protocol`;
  record the missing-contract/failing assertion before implementation.
- [ ] Implement Session generation checks and bounded SAX/schema validation using
  existing JSON code. Explicit operations: load, browseLauncher, browseFolder,
  browseIcon, importDrop, saveEdits, cancelDraft, lockNow. No unlock operation.
  Reject repeated request ids within the editor lifetime.
- [ ] Re-run Protocol; require all named cases to PASS.
- [ ] Review and commit `feat: add revocable ShelfSuite editor protocol`.

## Task 2: Guarded host and real Windows selection/drop path

**Files:** Create Host, Selection, initial Controller and EditorResources modules,
the initial UI assets and dependency files. Modify Library project/resources,
Build.bat, CommandHandler and CafeLock. Create HostHarness and fixtures; extend
the test runner Host suite.

**Interfaces:**
- `bool CafeShelf::TryOpen(const WCHAR* file)`: returns handled only for the
  configured ShelfSuite configurator; consumes existing lock/path checks.
- `void CafeShelf::RevokeAndClose()`: invalidates first, closes second.
- `Result<SelectedFile> Pick(HWND owner, SelectionKind kind, Ticket ticket)`.
- `Result<SelectedFile> AcceptDrop(IUnknown* fileObject, Ticket ticket)`.
- `void Controller::Receive(Ticket, const std::wstring& source, const Request&)`.
- SelectionKind values: LauncherFile, Folder, Icon. Controller owns Session,
  SelectedFile records and asynchronous responses.

- [ ] Write Host tests asserting locked gear opens no editor; unlocked gear
  creates one instance; wrong-origin and frame-origin messages are rejected;
  unrelated HTML dispatch remains unchanged; missing
  Runtime leaves launcher operation intact; Lock Now closes host/picker and
  invalidates asynchronous initialization and late replies.
- [ ] Add actual Windows file/drop fixtures for .lnk, EXE, folder and document.
  Native selection must return the shortcut path and folder path unchanged.
  A constructed browser File with no real path must be rejected.
- [ ] Run Host tests before the host implementation; record failures.
- [ ] Restore the pinned SDK, embed the initial trusted UI and implement the
  host. Feature-detect required Runtime COM interfaces rather than assuming
  installed Edge is usable. Keep a private per-editor WebView profile under
  the disposable/runtime user-data area, with no shared browser grants.
- [ ] Deny all navigation except the exact trusted page; deny new windows,
  remote resources, downloads and direct FileReadWrite permissions. No host
  objects or generic ExecuteScript bridge. Disable production devtools and
  do not enable remote debugging; test harness instrumentation stays in its
  separate executable and is not an installed Rainmeter flag.
- [ ] Use `IFileOpenDialog` with filesystem selection,
  `FOS_NODEREFERENCELINKS` and folder mode as appropriate. Use supported
  additional-file-object messages for drops; if real folder/shortcut drops
  cannot preserve paths, implement an OLE drop surface in the same host window.
  Do not continue to later UI work until all four real drop categories pass.
- [ ] Revoke before destroying UI and on unexpected navigation/process failure.
  Route existing gear calls to TryOpen; call RevokeAndClose from LockNow/Shutdown.
  Lock remains immediate while picker/extraction callbacks are outstanding.
- [ ] Run Host and Protocol suites, build x64 and check gear/basic-skin regression.
- [ ] Commit `feat: host ShelfSuite editing behind Maintenance Mode`.

## Task 3: Launcher inspection and preserved shortcut behavior

**Files:** Create Launcher modules; extend Selection, Windows tests and
`Tests/CafeLaunchProbe.cpp`; add fixtures in CafeShelfFixtures.ps1.

**Interfaces:**
- `Result<LauncherDraft> InspectLauncher(const SelectedFile& selection)`.
- Produces name, original launch path and icon-source metadata for Task 4;
  no write or launch side effects.

- [ ] Write Launcher tests: both picker and drop keep .lnk action; EXE description
  falls back to stem; folders/files retain paths; missing target does not retarget
  a shortcut; files with spaces, Unicode, brackets, hashes and percent signs
  either launch the exact selection or fail clearly without alternate execution.
- [ ] Generate a shortcut to a harmless probe with distinguishable arguments,
  working directory and custom icon. Record Windows shell baseline results.
  Through the actual ShelfSuite meter, require the same arguments/working
  directory and a standard-user process token. Test ordinary file association
  and folder activation on the disposable desktop.
- [ ] Run Launcher suite and record failures.
- [ ] Implement shell display names, EXE version-description fallback and
  shortcut inspection without executing targets or repairing shortcuts.
  Retain original .lnk as action. If the integration test proves RunFile's
  existing directory override changes shortcut semantics, narrow that fix to
  shortcut dispatch and preserve other launch behavior.
- [ ] Re-run Launcher and existing launcher regressions; assert import creates
  no launch-probe marker and changes no shortcut bytes.
- [ ] Commit `feat: detect launcher details without changing shortcut behavior`.

## Task 4: Transparent icon preparation and safe names

**Files:** Create Icons modules; extend Windows/core tests and fixtures.

**Interfaces:**
- `Result<PngImage> PrepareIcon(const SelectedFile& source)`.
- `Result<PngImage> PrepareLauncherIcon(const LauncherDraft& launcher)`.
- `std::wstring IconBaseName(const std::wstring& displayName)`.
- Actual destination reservation belongs to Storage (Task 5); Icons never writes
  the skin. Return typed extraction failure so UI offers a generic icon.

- [ ] Write Icons tests: PNG alpha pixels survive; multi-resolution ICO chooses
  a suitable frame; EXE resource and LNK custom icon produce decodable PNG;
  target-icon fallback works; failed automatic extraction offers generic;
  corrupt/oversized images reject; generated names avoid Windows reserved names,
  trailing dots/spaces, separators and case-only collision assumptions.
- [ ] Run Icons suite and record failures.
- [ ] Implement WIC decoding/encoding and Windows shell icon extraction using
  icon-only behavior. Enforce the shared input/dimension limits before allocating
  decoded buffers. Handle legacy icon masks and alpha correctly. Do work off the
  Rainmeter UI thread and return results tagged with the original Ticket.
- [ ] Re-run Icons; decode output again and assert dimensions, alpha and relevant
  fixture pixels. Preview bytes and saved bytes must be identical.
- [ ] Commit `feat: prepare ShelfSuite PNG icons from Windows files`.

## Task 5: Preserve configuration and coordinate safe filesystem commits

**Files:** Create Config and Storage modules; extend core/Windows Storage tests.

**Interfaces:**
- `Result<ShelfDocument> ParseConfig(const std::vector<uint8_t>& bytes)`.
- `Result<std::vector<uint8_t>> ApplyEdits(const ShelfDocument&, const EditBatch&)`.
- `Result<Snapshot> Storage::Load(const std::wstring& shelfId)`.
  Snapshot contains ShelfDocument and FileVersion.
- `Result<PreparedSave> Storage::Prepare(const Snapshot&, const EditBatch&,
  const std::vector<PreparedIcon>& icons, Ticket)`.
- `Result<SaveResult> Storage::Commit(PreparedSave&&, Ticket, bool locked)`.
- PreparedIcon contains logical import id, safe base name and PngImage.
  PreparedSave owns temporary files, original versions, destination handles,
  edits and ticket; no page-provided paths. RAII cleanup touches owned files only.

- [ ] Write Storage tests: stock named fields, tuple items, single/double-quoted
  strings, escapes, comments and unknown literal fields round-trip; unrelated
  bytes stay unchanged after focused item edits. A function call, executable
  statement, duplicate ambiguous field or unsupported expression blocks edits
  and leaves source bytes unchanged; never execute Lua during parsing.
- [ ] Add collision tests asserting spotify.png unchanged, second import named
  spotify-2.png, case-insensitive and concurrent creation safe. Add atomic-save
  failure tests: full/denied destination, changed source version, replacement
  failure and crash checkpoints leave previous config usable.
- [ ] Add containment tests for traversal, device paths, junction/symlink swaps,
  hard links, unexpected shelf identifiers and unknown files in a removed shelf.
  No operation may recursively delete an unknown/user-owned directory tree.
- [ ] Add ordering tests: revoke during Prepare -> Commit Locked/Stale; revoke
  then re-open -> old save Stale; a save committed before lock remains; no file
  commit occurs after the lock transition.
- [ ] Run Storage tests and record failures.
- [ ] Implement a bounded lexer/parser for literal ShelfConfig tables with
  source spans. Do not reuse the original lossy regex parser. Preserve source
  encoding and newline style; support UTF-8/BOM and UTF-16 BOM; reject undecodable
  input without modifying it. Serialize changed string values with correct Lua
  escapes and validate launcher representation through Rainmeter.
- [ ] Resolve permitted shelf ids from the installed root and known metadata.
  Permit only recognized config.lua, targeted theme include changes, known
  pinned Shelf.ini templates for new shelves, new icons and recovery backups.
  Shelf removal uses a recoverable rename inside the same trusted root after
  explicit UI confirmation, not deletion of unknown files. No engine or password
  writes are allowed. Reject destinations that are reparse/hard links and hold
  validated parent handles against rename/delete during the transaction.
- [ ] Prepare temporary files off the UI thread. Coordinate final commit with
  LockNow using one native gate; re-check live lock, Ticket, file identity and
  SHA-256 under that gate. Never wait for remote I/O in the lock callback:
  editing roots requiring network I/O are Unsupported; existing launch actions
  may still target network locations.
- [ ] Reserve complete icons with CREATE_NEW semantics; flush, make uniquely
  named old-config backup and atomically replace the config. On failure remove
  only this transaction's files. Preserve a fully written unreferenced icon after
  a crash if ownership cannot be proven. Serialize refresh after successful save
  and only for the affected already-loaded shelf.
- [ ] Re-run Storage, Icons and Protocol; verify fixtures' hashes after failures.
- [ ] Commit `feat: save ShelfSuite edits without losing existing configuration`.

## Task 6: Complete the familiar configurator and await real save outcomes

**Files:** Adapt UI files and Controller; create CafeShelfUi.ps1 and extend
HostHarness, test runner and ShelfSuite tests.

**Interfaces:**
- `void Controller::Receive(...)` dispatches the Task 1 operations and uses
  Tasks 3-5 without generic filesystem access.
- Page function `request(op, payload, additionalObjects = []) -> Promise`
  correlates request ids; only native responses settle success.
- `renderDraft(draft)` displays name/action/PNG preview using safe DOM APIs.
- `saveEdits(batch) -> Promise<SaveResult>` awaits native commit before success.

- [ ] Write UI tests covering Add and Edit, browse/drop launcher and custom icon,
  automatic values, preview, name edits, Cancel, delayed Save, failed Save,
  duplicate clicks and stale callbacks. Before a delayed native success, the UI
  must not show Saved or discard its draft.
- [ ] Add XSS fixtures in labels, tab names, filenames and error messages;
  assert literal text, no script invocation and no unexpected bridge request.
  Add over-capacity tab/item checks and unsupported-config read-only messaging.
- [ ] Run Ui suite and record failures.
- [ ] Preserve the pinned configurator's layout and usable shelf/tab/theme
  controls. Extract script/style into embedded resources, replace inline event
  handlers and unsafe interpolation with event listeners/text DOM. Apply a CSP
  allowing only owned scripts/styles and PNG previews, with no frames or network
  connect. Remove stock direct File System Access writes and remote update-fetch
  behavior in the integrated page; retain attribution as text.
- [ ] Bind Browse/drop to native selection; keep drafts/icons in memory until
  Save, with original .lnk paths. Existing custom icon values remain selectable.
  Show the precise cancel/error/fallback state; validate capacities from Task 5.
- [ ] Wire every modifying control through typed edits; theme/shelf mutations
  use Task 5 operations. Add Lock Now using existing native locking only.
  Close/revoke on lock even if unsaved; do not leave a writable browser fallback.
- [ ] Run Ui, Host, Storage and real ShelfSuite tests with a standard-user token.
  Use UI Automation/Win32 for real Rainmeter UI; renderer assertions may use the
  separately built host harness's private test instrumentation. Do not ship a
  test unlock switch or production test endpoint.
- [ ] Commit `feat: add browse and drop editing to the ShelfSuite configurator`.

## Task 7: Packaging, all acceptance tests and review evidence

**Files:** Modify `Build/CafeInstaller/Package.ps1`, `Installer.nsi`,
`.github/workflows/cafe-lock.yml`, `Tests/CafeInstaller.ps1`,
`CafeShelfSuite.ps1`, `CafeShelfSuiteRunner.ps1`, `CafeLockSmoke.ps1`,
`CafeShelfTests.ps1`, `Docs/CafeLock-Deployment.md`,
`Docs/CafeLock-Compatibility.md`. Create
`Build/CafeDependencies/AcquireRuntime.ps1`,
`Docs/CafeLock-Usability-Verification.md`.

**Interfaces:** Package.ps1 consumes committed code/assets and a verified
Microsoft Evergreen standalone x64 installer in staging. AcquireRuntime.ps1
records actual file version, SHA-256 and Authenticode signer in the build manifest;
a release package always records exactly which installer it contains.
No unverified download is executed.

- [ ] Write packaging tests for Runtime already present, absent, offline,
  installation declined and install failure. Existing Rainmeter launchers must
  still function; editor gives RuntimeMissing instead of crashing.
- [ ] Run `pwsh -NoProfile -File Tests/CafeInstaller.ps1` in the existing
  prepared disposable installer-test environment; record failure of the new
  prerequisite assertions before implementing them.
- [ ] Bundle the official signed Evergreen standalone x64 installer for offline
  prerequisite setup. Present a clear optional prerequisite choice only if
  missing; silent installs install it only with explicit /INSTALLWEBVIEW2=1.
  Never uninstall the shared Runtime on Cafe Lock uninstall. Do not automatically
  fetch/install anything when opening the editor. Include SDK/ShelfSuite notices
  and matching source; do not bundle a second skin.
- [ ] Preserve install-directory ACLs, standard-user startup shortcut and user
  data retention. Installer may elevate installation as before; it must never
  launch Rainmeter elevated. Test upgrade with existing config/icons/password.
- [ ] Extend the workflow with SDK restore and all new suites. Keep the pinned
  upstream checkout/hash checks, existing password/policy/smoke/installer tests
  and x64 validation. Update gear assertions to integrated editor visibility;
  retain an unrelated-HTML control so ordinary launch dispatch is still tested.
- [ ] Execute A1-A12 from the spec, record pass/fail and evidence by id. Extend
  CafeLaunchProbe for args/current directory/token and preserve original tests.
  Exercise real Windows drop/picker/association behavior on a disposable desktop;
  harness calls alone do not prove OS drag/drop.
- [ ] Run `pwsh -NoProfile -File Tests/CafeShelfTests.ps1 -Suite All` in the
  prepared disposable environment and the full authoritative workflow on the
  final commit. Require all compiler/test exits zero, x64 verification PASS,
  matching tested SHA and artifact generation success. Run relevant upstream
  tests if a shared component was changed.
- [ ] Run real sign-out/restart/shutdown checks on a spare VM, including editor
  and picker open. Record manual evidence separately; do not restart the user's PC.
- [ ] Review the entire branch for authorization bypasses, path escapes,
  encoding failures, data loss, artifact/source mismatch and unrelated changes.
  Run git diff --check; exclude generated version/build outputs.
- [ ] Commit `test: verify ShelfSuite imports and package editor prerequisites`.
  If fixes follow, rerun affected checks and final workflow on the new revision.
- [ ] Report commits, run link, artifacts and remaining manual limits. Stop for
  review before merge or release.


## Plan self-review and acceptance mapping

| Spec acceptance | Implemented by | Required evidence |
| --- | --- | --- |
| A1-A2 shortcut drop/browse | Tasks 2, 3, 4, 6 | Real selected .lnk path, filled fields, shell-probe equivalence. |
| A3 executable drop/browse | Tasks 2, 3, 4, 6 | Correct path/name, decoded embedded icon, actual launch. |
| A4 folder launcher | Tasks 2, 3, 6 | Real folder selection/drop and shell folder opening. |
| A5 associated file | Tasks 2, 3, 6 | Correct file reaches its controlled Windows association. |
| A6 PNG | Tasks 4, 5, 6 | Saved alpha-bearing PNG and matching config reference. |
| A7 ICO | Tasks 4, 5, 6 | Decodable PNG conversion, alpha/dimensions and reference. |
| A8 duplicate names | Tasks 4, 5 | Case-insensitive/concurrent reservation with old bytes intact. |
| A9 locked restrictions | Tasks 1, 2, 5, 6 | Real locked editor rejection, late-request rejection, launcher clicks preserved. |
| A10 maintenance editing | Tasks 2, 5, 6 | Real password unlock, browse/drop/save, correct disk contents. |
| A11 old configs | Task 5 | Supported round trips and byte preservation for rejected configs. |
| A12 existing regressions | Task 7 | Full Windows workflow green on final revision. |

The plan was checked against the approved spec for scope, authorization lifetime,
source/destination boundaries, artifact/source matching and compatibility.
New file/interface names are shared through the contracts above. The UI page
never authorizes a write by itself. Tests separately exercise the production
controller, the actual Windows selection path and existing skin behavior.

## Execution and review handoff

Recommendation: native execution in this chat, with focused commits and a final
independent review after the tests. Tasks share lifecycle/storage contracts;
one implementer reduces coordination, while the final review specifically checks
the security and data-loss cases above. Alternatively the owner may select
subagent-driven implementation and review for each task.

This is a plan, not a claim that code/tests exist or pass. The local command runner
still fails before process launch. Remote documentation preparation is verified
through GitHub; local status/build/runtime results remain unverified.
Do not begin implementation before plan review and execution-method selection.

## Dependency references

- [Pinned WebView2 SDK](https://www.nuget.org/packages/Microsoft.Web.WebView2/1.0.4258.31)
- [WebView2 distribution and static loader](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/distribution)
- [WebView2 security guidance](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/security)
- [Windows shortcut selection](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/ne-shobjidl_core-_fileopendialogoptions)
