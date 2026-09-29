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

HostPolicy RED observed at 4508e44, run 36639323727 / job 109647582037: 25 host checks, 6 expected assertion failures, while all 49 protocol checks passed. Origin/resource policy implementation follows; GREEN pending. The first real-drop probe compiled but linking exposed missing Advapi32 dependencies from Microsoft's static loader. Added the required library without suppressing errors; real drop execution remains pending.
