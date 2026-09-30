# ShelfSuite usability execution record

## Approved approach

The owner approved the design, plan and native implementation with final independent review. Work stays on feature/cafe-lock-usability; no merge or release is authorized.

## Baseline evidence

Unmodified approved plan revision 7715d631d2a2bbac6c1556281a1ca792327c1256 passed the full Windows workflow: https://github.com/BBrnen/Rainmeter-Cafe-Lock/actions/runs/36636308159 .

## Rulings

- Local shell process creation repeatedly fails before any command executes. Local checkout status is unknown; no local files are changed. Continue with authenticated GitHub Git-data operations and disposable Windows CI. Check the remote branch head before each non-force update. Remote diff inspection replaces local diff inspection for these commits; do not claim local checks ran.
- Task 1 adds fail-closed compile scaffolding and real contract tests first. These modules are not linked into Rainmeter yet. The expected failing assertion run must be recorded before replacing scaffolding with implementation.
- saveEdits remains Unsupported until the typed edit model in Task 5; no raw Lua or generic filesystem request is temporarily accepted.

## Task status

1. Session/protocol: RED observed: 49 checks, 17 expected assertion failures at 3039f337e744db32a6c793fac8762c3dafb01d3d, run 36638064658 / job 109643475043. Compilation succeeded. GREEN: 49 checks, 0 failures at e0fbf3c33105f2dc1c3bdb22ac2818f4d7d80686, run 36638270579 / job 109644150707; MSVC x64 /W4 /WX. Task 1 complete, excluding saveEdits as ruled above.
2. Host/selection: dependency acquisition verification started; host/drop tests pending.
3–7. Not started.

## Test infrastructure correction

The first test compilation (9a8d9da, run 36637946440) found MSVC C4310 in the invalid UTF-8 fixture's signed-char cast. Commit 3039f33 uses an explicit byte escape instead; no compiler warning was suppressed. Only the following assertion run counts as RED evidence.

## Interface preflight

- Tasks 1/2/5/6: native Ticket and live lock state must accompany every async operation, not values from JSON. Session checks do not independently confer Maintenance authorization.
- Tasks 2/3/4: SelectedFile comes from native pick/drop; pages cannot turn arbitrary strings into selected files.
- Tasks 4/5/6: icon preparation returns bytes only; Storage owns all writes. Cancel drops memory state.
- Tasks 5/6: typed EditBatch and source versions are required before saveEdits is enabled.
- Tasks 2/7: SDK restore and optional Runtime packaging stay distinct; missing Runtime must not break ordinary launchers.
- Ruling: record progress in this committed execution document while the local workspace helper cannot run. This preserves handoff evidence without claiming local execution; cost is extra documentation commits.

## Task 2 preflight ruling

Ruling: inspect the actual pinned SDK package on Windows CI before committing its digest. NuGet must verify the package signature before any SDK content is used. The temporary probe is CI-only and will be superseded by the digest-enforcing restore script; a missing or invalid package blocks host integration.

Ruling: upstream disables C++ exceptions globally. New standalone parser/host modules need a per-file exception setting when integrated; do not change global Rainmeter compiler behavior. Verify the same options in core tests and production integration.

## Task 2 dependency evidence and early drop probe

- Run 36638462606 / job 109644780680 verified the Microsoft author signature, NuGet countersignature, SHA-256 and x86/x64 static-loader layout for SDK 1.0.4258.31. The manifest records the observed digest and author certificate fingerprint.
- Ruling: run an isolated native WebView2/OLE drop feasibility probe before building the production host. It uses Windows shell data objects and actual mouse/drop delivery; a JavaScript-fabricated File must have no usable path. It neither writes ShelfSuite nor launches fixtures. This resolves the real shortcut/folder path uncertainty early; cost is temporary test-only harness code until it is incorporated into production-host tests.
- The drop probe is not evidence of production host security or UI completion. Those remain pending.

## Task 2 host policy tests

Ruling: isolate native origin/resource/lifetime validation into HostPolicy alongside the Windows Host. This lets the real production policy run without a browser, while the separate host tests still must verify actual events and drop handling. Cost: two focused files beyond the initial filename list, with no extra public application interface.

HostPolicy RED observed at 4508e44, run 36639323727 / job 109647582037: 25 host checks, 6 expected assertion failures, while all 49 protocol checks passed. HostPolicy GREEN observed at 848c590, run 36639573611: 49 protocol and 25 host checks passed together. Full existing x64 regressions also passed; Task 2 remains incomplete because real-drop validation is separate. The first real-drop probe compiled but linking exposed missing Advapi32 dependencies from Microsoft's static loader. Added the required library without suppressing errors; real drop execution remains pending.

## Real-drop diagnostic evidence

Run 36640033708 / job 109649870945 compiled the probe, created the real shell item/data object, placed the cursor and entered DoDragDrop. No release timer or target message was dispatched before the explicit 120-second timeout. The browser rejects fabricated Files before native delivery as expected. Hypothesis: the source's modal OLE loop blocks the target's browser UI apartment. Move the simulated Explorer source to its own STA thread while keeping every WebView callback on the UI thread; preserve actual Windows OLE delivery and original path assertions. This changes test architecture, not product behavior. Do not count a timeout as a successful drop test.

Run 36640427559 confirms that moving the source to its own STA restores the UI release timer, but the OLE source still does not return or deliver a file. The next test corrects a harness mismatch: hosted runners execute elevated, whereas the approved product runs at medium integrity. Reuse the repository's existing RunAsStandard launcher and require the probe's token to be below high integrity. This is a test-only adjustment, not an elevation/unlock change in Rainmeter. The watchdog now terminates only its own launched process tree.

The standard-user probe at 3405924 / run 36640876108 confirms medium integrity (RID 8192), but WebView2 131.0.2903.86 disconnects while creating its controller (HRESULT 0x80010108). Real drops therefore remain unverified. Add window-station/profile diagnostics and a standalone spare-PC/VM kit, with an explicit manual test option rather than faking GITHUB_ACTIONS. The kit is separate from Rainmeter, creates temporary fixtures only and has no unlock or skin-writing functionality. It is published even when the runner cannot host the interactive test. Production host integration and later import/storage/UI tasks remain gated on the real-drop result.

## Current external test gate

Run 36641339289 / job 109654062121 shows 49/49 protocol and 25/25 host-policy checks passing, a medium-integrity token, interactive Windows session 2, and a writable private browser profile. Despite those checks, the hosted runner's WebView2 131.0.2903.86 disconnects while creating the controller (0x80010108). This is not evidence that the requested drops work or fail in a normal logged-in user session. The isolated diagnostic compiled and uploaded successfully. No Rainmeter production integration, import, icon writing, configuration saving or deployment change has been made.

Next required evidence: run the packaged diagnostic normally on the owner's offered spare PC/VM and obtain its real-drop results. Do not invent a green result, silently waive drops, or treat a COM-launch failure as a product compatibility pass. If the browser drop path fails on that desktop, use the already approved native OLE drop-surface fallback and repeat all four categories before later UI work. Existing user approval still covers the full implementation; no renewed design approval is needed.

The diagnostic now verifies its own window is under the target point before sending mouse input. Its package records the tested source revision, executable SHA-256, source, instructions, dependency manifest and discovered SDK notices. The main Windows job runs git diff --check against the PR base; this is CI evidence, not a claim about the untouched local checkout.

## Manual probe hidden-window investigation

The owner reproduced the covered-window refusal on a normal standard-user desktop (WebView2 154.0.4258.37); the private profile and fabricated-file rejection passed. Repositioning other windows did not help. Source inspection found that the script launches with WindowStyle Hidden and the probe relies solely on WS_VISIBLE at creation. Microsoft documents that initial visibility can be overridden by STARTUPINFO (https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow). Add a separate-process Windows regression using the same creation function and actual SW_HIDE startup; establish RED before changing visibility. Do not remove the target hit-test safeguard or infer drop success.

The full existing x64 build, policy, password, standard-user, ShelfSuite and installer regressions passed at 12b26f1 in run 36641617591 / job 109654953682. The new editor job still fails at CI WebView controller creation; no new feature completion is claimed.

Hidden-window RED confirmed at 0a2ca37, run 36643038478 / job 109659529606: the probe compiles, all 74 core assertions pass, and actual Windows startup with SW_HIDE leaves the shared probe window invisible. Explicitly restore the diagnostic window after consuming startup visibility; preserve the covered-window refusal and log visibility/activation/window handles for any remaining failure.

The owner's screenshot also shows an empty process exit code. The packaged script uses built-in Windows PowerShell 5.1 and redirected Start-Process, matching PowerShell issue 5421 (https://github.com/PowerShell/PowerShell/issues/5421). Add known-zero and known-nonzero child-process regressions using the same launcher before applying its documented handle-retention workaround. These tests are CI-only and never grant test privileges to the installed application.

At 3c4b2fd, run 36643288480 / job 109660324703, the visibility regression is GREEN: visible=0 before explicit show, visible=1 afterward. Both existing core suites remain GREEN (49+25). The Windows PowerShell process regression is RED: a child returning 0 yields a null ExitCode, exactly reproducing the second issue in the owner's screenshot. Retain the process handle immediately after Start-Process using the documented workaround; rerun both zero/nonzero checks and preserve fail-closed handling of missing exit codes. Actual real drops remain a separate, unpassed gate.

## Replace manual handoff's synthetic drag source

At 96a9cb8, run 36643434877 / job 109660796374 confirms both hidden-window and Windows PowerShell 0/7 exit-code regressions pass, alongside all 74 protocol/host-policy assertions. The full existing x64 job 109660796422 also passed. CI's WebView controller still disconnects; this is not a successful real-drop result.

The owner's next manual run confirms medium integrity, visible/restored target, successful activation, correct hit-test root, fabricated-file rejection, entry into DoDragDrop and firing of the release timer. No drop callback or OLE return follows; native and external watchdogs expire. Visibility is no longer the failing boundary. This does not establish whether a real Explorer drag works. Stop changing focus settings or asking for repeated unattended runs.

Ruling: replace the spare-PC handoff's simulated in-process OLE source with the human's actual File Explorer drag. Keep the CI simulator's failure visible; do not count this test-architecture change as production feature progress. The manual diagnostic opens a temporary folder containing only four fixtures, shows the required next item, logs browser drag/drop event arrival, and validates the same native additional-object path for every item. It sends no mouse input and starts no synthetic drag thread. Ten minutes permits human interaction; closing the window cancels. No fixture is launched, and no Rainmeter/ShelfSuite files are changed. This isolates the uncertain browser path without another guess at the simulator deadlock. The production native OLE fallback remains available if actual Explorer drops fail the path checks.

## Real Explorer drop gate passed; Task 2 implementation resumes

The owner supplied a complete PASS from manual kit 0f615fe on a normal Windows desktop with medium integrity RID 8192 and WebView2 154.0.4258.37. Each real Explorer drop (.lnk, EXE, folder, normal file) produced one native additional object and preserved its original path. Fabricated browser File delivery was rejected. This is manual evidence for the early feasibility gate only, not completed import/name/icon/save functionality or production-host authorization. The full existing x64 job 109665347082 in run 36644854976 also passed; CI's standalone WebView probe still fails at controller initialization.

Task 2: Ruling: initial Controller owns HostPolicy, live native lock callback and lifetime-bound selection records. Selection consumes an explicit native authorization callback because a Ticket alone is never authorization. Its picker checks both before and after modal callbacks; Revoke closes a pending dialog and permanently revokes that Selection object. Tests exercise real Windows picker options without showing a dialog, and simulated native COM callbacks for validation/reentrant locking. They do not replace the recorded real Explorer evidence. Source-level compile scaffolds fail closed until RED is observed. Cost if this boundary were wrong: stale selections could escape a lock transition; explicit lifecycle assertions cover that failure mode.

Controller/Selection RED at 52f9eea, run 36645600612 / job 109667705801: compilation succeeded; original suites passed (49+25); Controller 19 checks had 7 expected failures, Selection 28 checks had 18 expected failures. Implement native session-bound records, real picker options preserving shortcuts, native additional-object path validation, and reentrant revocation. These modules still are not linked into Rainmeter until the guarded host is ready.

Controller/Selection GREEN at 0085da9, run 36645864271 / job 109668542835: 49 protocol + 25 host-policy + 19 Controller + 28 Selection assertions passed (121 total). Actual Windows file-dialog options preserve shortcuts; native callback revocation and all path checks pass. The separate CI browser-start failure remains visible.

Task 2 host preflight: modal native pickers must be posted to the host message queue after the WebView callback returns, per Microsoft's STA/reentrancy contract; never call Show directly inside WebMessageReceived. Browser initialization callbacks use weak lifetime references, and a stale arriving controller is explicitly closed. A native factory seam permits deterministic tests of delayed/missing initialization without granting any page-configurable capability. Actual browser event/security checks remain separate from those lifecycle tests. Add RED lifecycle scaffolds before the host implementation.

Host lifecycle RED at 73df157, run 36646124765 / job 109669389355: 12 assertions, 8 expected failures after successful compilation; all preceding 121 assertions remain GREEN. Implement the native host with weak async callbacks, revocation-first closure, a bounded posted-work queue, exact embedded resource serving, deny-by-default browser permissions/navigation/downloads/external launches, and priority Lock Now. The harness pumps the Windows message queue after async failures because error reporting is intentionally deferred beyond WebView callbacks; no modal UI is opened inside a browser event. Production linking/routing and actual hosted browser checks are still pending.

Host lifecycle GREEN at cd6c4b1, run 36646604922 / job 109670926538: all preceding 121 assertions plus 14 host lifecycle assertions pass. The real native window and pending initialization behavior are exercised; browser creation itself remains a separate failing CI prerequisite. No production source is linked yet.

Add the integration RED check before routing the gear: an unlocked stock ShelfSuite gear must not escape into the externally registered HTML handler. It must create the native editor or show a native actionable runtime error; either outcome leaves ordinary HTML association dispatch intact. Lock Now must close any editor. Accepting a runtime-error surface validates fail-closed dispatch only, never browser editing. Actual hosted browser operations remain an explicit separate acceptance check.

Integration RED at 283bbf5, run 36646835322 / job 109671654250: x64 compilation, architecture, password and both elevated/standard-user smoke passes succeeded. Existing ShelfSuite launcher/tab/hover/lock checks passed; the new check then timed out waiting for the missing native editor/runtime-error surface. The preceding cd6c4b1 main job had an unrelated tray-icon assertion failure before any editor code was linked; that same unmodified smoke check passed in this fresh integration RED run. Do not erase either result.

Wire the known configurator path to the native host, consume it even on runtime failure, and revoke it from LockNow/Shutdown. Add embedded original preview assets and pinned SDK build integration. Per-source exception handling is enabled only for the new isolated modules; upstream global options remain unchanged. This is a browse/drop host preview, not the final adapted configurator or saving feature; Task 3 onward remains unfinished. Existing original skin files remain untouched.

Integration build at 31e7d85 stopped at diff whitespace validation: existing upstream CRLF files had their newly added lines flagged solely for CR at EOL. Preserve upstream line endings and teach the check cr-at-eol; real trailing spaces, blank EOF lines and space-before-tab checks remain enabled. Standalone 135 assertions passed; no production build result yet.

Add actual embedded-browser policy tests beside deterministic lifecycle tests. The test executable alone observes the WebView through a native delegating environment; no ExecuteScript hook or browser factory switch is added to Rainmeter. The suite must load real embedded resources, exercise typed messages/path rejection/replay prevention and Lock Now, and fail if the runtime cannot start. It runs under the disposable CI runner identity, so it cannot substitute for standard-user desktop acceptance. The old filtered-token simulated-drop failure remains visible until replacement coverage is established.

Production integration build 4cb55be / run 36647819670 failed at LTCG with C4743: mixed _HAS_EXCEPTIONS values changed std::runtime_error and JSON exception vtable layouts. Keep upstream _HAS_EXCEPTIONS=0 consistently across the DLL; only compiler stack unwinding remains enabled for editor code. Isolate bundled JSON template symbols with a small namespace adapter (no vendor source edits), preventing exception-disabled UpdateCheck instantiations from coalescing with the editor. Compile standalone suites with the same STL macro, no RTTI and LTCG to expose integration differences sooner. Browser harness f046f99 also caught a WRL inherited-name shadow warning; rename that test-only parameter. No warnings are suppressed.

At 10ff8af / run 36648189895, all 135 native assertions pass with production STL/LTCG settings. Real WebView controller creation succeeds in the runner's own identity, but trusted page load fails because the host cannot find an embedded resource. Add direct resource-enumeration assertions to distinguish resource naming/type/linkage before modifying the resource definition. Extend the real browser suite with native popup-handler observation, fabricated File rejection, unexpected navigation and frame revocation.

Resource RED confirmed at f0e9be2 / run 36648424057 / job 109676715882: enumeration found type 10 names with literal surrounding quotation marks; all three unquoted FindResource assertions failed. Use unquoted identifier names in the RC declarations, preserving type, content and native allowlist. Production integration at 10ff8af / run 36648189895 / job 109675957462 is GREEN: x64, policy/password, both smoke identities, native gear routing, ordinary HTML and installer regressions pass. The filtered-token ShelfSuite test explicitly reported a native runtime error, so it proves fail-closed dispatch, not browser rendering. Real-browser policy checks are still required.

Task 2 real-browser GREEN at 8a3f5dd / run 36648601216 / job 109677275856: all 135 native assertions plus 20 real WebView/resource/security assertions pass, including fabricated File rejection, no path authority, replay rejection, popup/network blocking, Lock Now, unexpected navigation and frame revocation. The separate filtered-token simulated-drop step still fails controller creation with 0x80010108. Ruling: the plan's four-real-drop feasibility gate is satisfied by the owner's actual medium-integrity Explorer results from 0f615fe, and the real-host security gate is now satisfied by required Browser tests. Replace the unreliable simulated-drag default CI gate with an explicitly named build-only manual-kit step; retain simulation as an optional forensic mode with unchanged failure semantics. This is a documented coverage replacement, not a real-drop PASS inferred from compilation or a swallowed failure. Standard-user final production UI/manual acceptance remains pending. All existing x64/regression checks remain required.

Task 3 begins with metadata RED fixtures: original and broken-target .lnk files, versioned/unversioned harmless EXEs, folders/documents/Unicode paths, and punctuation that the pinned action representation cannot safely accept. Drafts retain the original shell item as icon source; icon extraction follows in Task 4. Tests hash all fixtures and reject launch markers. No production call uses the metadata scaffold; original selected paths remain unchanged. Real shortcut arguments/working-directory behavior through a loaded ShelfSuite meter remains a separate required integration check before Task 3 completion.

Launcher metadata RED at b6db66e / run 36649055868 / job 109678699759: 19 checks compiled and ran; 14 expected failures; all prior 155 editor assertions passed. Implement bounded Windows display/version metadata reads with exact original action/icon-source paths and explicit rejection of ambiguous Rainmeter action punctuation. No shortcut target is resolved, repaired or launched. Extend the harmless launch recorder and stock ShelfSuite integration fixture to compare original .lnk arguments/current directory against the Windows shell baseline while locked; production RunFile is unchanged pending that evidence.

At 95b3e9f / run 36649334654, all 174 native/browser/metadata assertions pass. The loaded ShelfSuite shortcut test reached its Windows baseline but timed out on the meter click. Inspection found a fixture encoding mismatch: LuaScript::Initialize treats only UTF-16LE-BOM engine scripts as Unicode; the unchanged pinned ASCII ShelfEngine uses the legacy-codepage LuaHelper bridge. A UTF-8 config path containing Café becomes a different native path. Encode the fixture's representable non-ASCII path bytes as Lua decimal escapes and rerun; do not alter RunFile, the skin engine or global Lua behavior. Task 5 must explicitly round-trip strings through the active pinned bridge and use literal byte escapes, rejecting unrepresentable characters clearly rather than saving broken actions. Native Unicode metadata success is not end-to-end Unicode launch proof.
