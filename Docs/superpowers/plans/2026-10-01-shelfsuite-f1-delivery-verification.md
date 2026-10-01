# ShelfSuite F1 compatibility delivery verification

## Scope and revisions

Execution was Native on `feature/shelfsuite-f1-delivery`, from verified F1
`c6ce82ab7c2e9f901b01c712a0efd22bdee6d5c9`. The approved delivery design and
implementation plan remain unchanged. No F2, Rainmeter application, password,
authorization, Locked Mode, launcher, pinned ShelfSuite, native shelf-template
or adaptive-tab patch changes were made. No merge, release or owner deployment.

Final executable/test revision: `8f06fd3e55c43a0eda50cfaeff32af83c3028388`.
The evidence document is a subsequent documentation-only commit.

## Commits

| Commit | Purpose |
| --- | --- |
| `6bfc21fd` | Approved conservative delivery specification |
| `82ac5b7d` | Approved implementation plan |
| `8d3eee1a` | Verified payloads and exact recognition catalog |
| `36824dd2` | Read-only installation validation |
| `1f23b5f0` | Verified backups, bounded replacement and recovery |
| `da88f3ab` | Guided native interface, launcher and README |
| `1b6e93f0` | Extracted-ZIP Windows verification job |
| `361d73db` | Identical builder payloads from Windows CRLF checkouts |
| `9d1c7404` | Native Git exit-code reporting |
| `7f0884dc` | Separate builder tests from owner-runtime tests |
| `5e987173` | Canonical path boundaries and alias refusal |
| `f233c1af` | Restricted-runner self-access and incomplete-backup guidance |
| `8f06fd3e` | Compare account identities as SIDs, including SDDL aliases |

## What was added

- `Build/ShelfSuiteF1/Package.ps1`: builder-only Git provenance checks,
  unchanged F1 patch application, exact byte recognition fingerprints,
  payloads, licence, ZIP and external checksum.
- `Build/ShelfSuiteF1/Updater.psm1`: conservative preflight, declared-target
  file operations, verified originals, replacement and bounded recovery.
- `Build/ShelfSuiteF1/Update-ShelfSuite.ps1` and `.cmd`: normal-user native
  folder selection/confirmation, clear success/refusal/failure messages.
- `Build/ShelfSuiteF1/README.md`: extraction, limits and manual restoration.
- Three `Tests/CafeShelfF1Delivery*.ps1` files: disposable fixtures,
  package/preflight/apply/UI tests and extracted-package restricted runner.
- `.github/workflows/cafe-lock.yml`: independent delivery job and review
  artifact; existing full verification remains in place.
- `Docs/CafeLock-Compatibility.md`: link to separate delivery/manual checks.

The ZIP contains exactly eight entries: manifest, licence, two shared payloads,
updater module, PowerShell entry point, CMD launcher and README. It contains no
complete skin, launcher configurations, icons, theme files, fixtures or tests.
Installation INIs are recognized by exact hashes; only the approved dynamic
window-size line is inserted. Existing source line endings and theme choices
are retained. User `config.lua` is never opened by the updater.

## RED/GREEN and local verification

Each feature gate was observed failing before its implementation:

| Gate | RED | GREEN |
| --- | --- | --- |
| Task 1 | Absent builder/package validator | 7 Package checks |
| Task 2 | Absent preflight interface | 36 accumulated checks |
| Task 3 | Absent apply interface | 49 accumulated checks |
| Task 4 | Absent guided interface | 57 accumulated checks |
| Task 5 | Absent archive verifier | 58 accumulated checks |
| CRLF transport | Mixed-line-ending build failure | 59 checks |
| Native Git invocation | Session-shadowed command failure | 60 checks |
| Owner-runtime fixture separation | Absent prepared-fixture parameter | 61 checks |
| Review path boundaries | Trailing-root containment and actual 8.3 alias failures | 64 checks |
| Review failure guidance | Incomplete backup referenced absent restore record | 65 checks |

Final local command, exit 0, **65 checks passed**:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tests/CafeShelfF1Delivery.ps1 -Suite All -UpstreamDirectory <disposable-pinned-checkout>
```

The local process-only policy override is for the disposable test harness on a
Restricted-policy host. The distributed launcher has no bypass and changes no
persistent policy. This command is not an owner installation recipe.

Coverage includes exact pinned/modified-source refusal, fixed file allowlists,
corrupt payload/manifest/ZIP rejection, all four themes and known generated
templates, LF/CRLF, malformed encoding, customized extra shelves, busy/unreadable
files, junctions/redirected ancestors/hard links, Unicode paths, package-inside-
skin refusal, trailing separators and available real short-name aliases.
Backup/recovery tests inject write, corruption, partial replacement, outside-edit
and restoration failures; sentinel files remain unchanged. UI tests cover
cancellation, process-check refusal, file-list confirmation, no-op, safe errors,
incomplete backups, the moved/spaced/Unicode CMD launch path and policy refusal.
Native interactive dialogs are substituted in test scope; owner visual testing
is still required. The process-inspection-failure UI test exercises the refusal
route; it does not simulate every Windows process API failure.

`git diff --check` passed. The full application/source and verified F1 paths
compare unchanged against `c6ce82ab`. No generated binaries/private data were
committed. The working tree was clean before this evidence document.

## Windows workflow and artifact

**Full workflow passed: all four jobs succeeded, duration 6m 32s.**

Full workflow:
https://github.com/BBrnen/Rainmeter-Cafe-Lock/actions/runs/36840511947

Code revision: `8f06fd3e55c43a0eda50cfaeff32af83c3028388`.

| Job | Result and evidence |
| --- | --- |
| `shelfsuite-f1-delivery` | Passed: built ZIP, actual extracted standard-user verification, artifact upload |
| `cafe-runtime-policy` | Passed: 12 compiled Runtime decision/result checks |
| `cafe-shelf-core` | Passed: 318 core/host/controller/selection/lifecycle/browser/metadata/icon/config/storage checks, zero failures; delivery-contract audit and diagnostic build/startup/exit-code regressions |
| `cafe-lock-x64` | Passed: Windows x64 build/architecture, policy/password, normal and standard-user running-app regressions, shutdown-message checks, real pinned ShelfSuite and F1 layout regressions, installer packaging/install/upgrade/uninstall/profile preservation |

Live F1 evidence confirms text measurement, compact 480 px short-label layout,
no overlap, and growth only when five long tabs need it. Live ShelfSuite evidence
confirms original shortcut arguments/working directory, normal-user launches,
locked icons/tabs/hover/Lua, blocked movement/menus/gear and password-unlocked
native configurator. Automated shutdown-message tests are not a fresh real PC
shutdown test. Actual Explorer drag/drop was not rerun by the diagnostic build
step and no new manual drop result is claimed.

Delivery job `110298254256`: 11 Package checks before token reduction, then
54 updater checks from the extracted ZIP under PowerShell 5.1, admin=false,
RID8192. Both moved-CMD launch and Restricted-policy refusal passed. An actual
available 8.3 alias was refused in both local and Windows fixture tests.

Review artifact: `ShelfSuite-Cafe-Lock-F1-Compatibility`, artifact ID
`11150937114`, retained until 2026-10-15. Inner package filename:
`ShelfSuite-Cafe-Lock-F1-Compatibility.zip`.

Inner ZIP SHA-256, independently downloaded and hashed against its checksum:

```text
6555083de3cff50e95196d9c23b04659ea93a3830fdf925f91752a9b6195dee4
```

The GitHub outer artifact digest is different and must not be substituted for
the inner ZIP checksum. The downloaded ZIP's exact eight-entry allowlist also
passed independently without running the updater on an installed skin.

The package-builder tests run before token reduction. All owner-runtime
Preflight/Apply/UI cases import the extracted ZIP module under Windows
PowerShell 5.1, non-admin, medium integrity RID 8192; prepared disposable fixtures
avoid invoking Git in the owner runtime. Git remains builder-only.

## Review and narrow implementation rulings

A fresh independent whole-branch review of `c6ce82ab..7f0884dc` found no Critical
issues, one Important path-boundary issue and one Minor failure-guidance issue.
Both were reproduced and fixed in the single review fix pass with full local
verification. The path fix verifies native lexical paths against handle-resolved
paths, refusing aliases and retaining sibling backups despite trailing slashes.
The recovery-guidance finding was treated as Important for this owner because
referencing a nonexistent restore record could lead to using an incomplete
backup. There are no deferred review findings.

No approved product design was changed. Implementation/test rulings:

- Package checksum validation was brought into Task 1 to make corruption tests
  exercise production validation; later tasks reused the same interface.
- Builder scratch copies normalize Git transport line endings; the verified F1
  files remain untouched. Builder Git exit codes come from the native process.
- The Windows runner split builder checks from standard-user updater checks.
  The All owner-runtime suite excludes already-run builder-only Package tests.
- Restricted-process diagnostics showed administrator/SYSTEM full process
  access but only limited logon access, preventing redirected child launches.
  The delivery test runner temporarily adds current-user self-access to its
  token's default process DACL before invoking the existing restricted helper,
  then restores the exact original. Admin-group disabling and medium-integrity
  checks remain intact. This is runner-only; no installed permissions, machine
  policy, helper binary source or production security behavior changes.
- A hosted built-in account serializes with an SDDL abbreviation. A failing
  textual ACL assertion was replaced by parsed SID identity comparison; an
  explicit abbreviated/numeric SID regression and exact restoration check pass.
- Historical delivery audits use PowerShell 7 in the existing Windows workflow;
  their PowerShell-7 API usage was not rewritten for the local 5.1 harness.

The three execution rulings and their costs were: (1) process-only local test
policy override, which cannot establish an owner's policy permits launch;
(2) bring checksum validation into Task 1, requiring later tasks to reuse that
production interface; (3) run builder-only checks before token reduction, so
builder checks do not claim restricted-token coverage while all updater cases do.
The Windows child-process ACL correction applies only to disposable runner
processes and preserves restriction checks; actual owner-machine launch remains
a manual acceptance gate. A fresh disposable upstream clone avoided changing
global Git trust for an old sandbox-owned checkout.

## Remaining limits

- Manual spare-PC/VM acceptance is pending; green automation is not merge or
  release approval.
- Only recognized pinned/trusted template bytes and UTF-8 without BOM with
  consistent LF/CRLF are supported. Customizations and aliases refuse safely.
- The update is not atomic across all files. Interruptions require inspection
  and simple manual restoration; there is no replay/transaction framework.
- Hashes establish consistency, not publisher identity. The package is unsigned.
- Machine script policy may prevent launch. The package does not bypass it or
  ask for elevation; the PC administrator decides permitted policy.
- Short-name alias testing uses an actual alias when the fixture volume creates
  one; otherwise the harness reports that environmental limitation.
- Native owner dialogs and real desktop layout/hover/selection require manual
  testing. Only known disposable fixture data was accessed locally and in CI.

## Exact spare-PC/VM acceptance steps

For the newly created shelf check, use the matching Cafe Lock F1 review build
(`c6ce82ab` or later) on the spare PC/VM. The successful workflow also provides
the Rainmeter x64/installer review artifacts. This compatibility ZIP does not
update Rainmeter; an older executable retains its older generated-shelf template.

1. Use the spare PC/VM and make your own personal backup. Download the review
   artifact from the final successful run, extract its outer GitHub artifact,
   verify the inner compatibility ZIP against its adjacent `.zip.sha256`, then
   extract that ZIP to a fresh local folder outside installed `Shelf Suite`.
2. With Rainmeter still running, double-click `Update-ShelfSuite.cmd` normally.
   Expect a clear refusal and no changed skin files. Do not run as administrator.
   If script policy blocks launch, stop and consult the PC administrator.
3. Close Rainmeter normally. Run the updater again. Cancel folder selection:
   expect no changes. Rerun, select the existing `Shelf Suite`, inspect the full
   resolved location and proposed file list, then cancel confirmation: again no
   changes and no updater backup.
4. Rerun and confirm the recognized installation. Expect verified success and a
   timestamped backup beside Shelf Suite. Keep the displayed backup path. The
   backup must list only changed shared files and shelf INIs, no `config.lua`.
5. Start Rainmeter yourself. Check existing ONLINE/OFFLINE/INTERNET labels,
   positions, launchers, icons and themes. In Maintenance Mode, test SCHOOL/WORK,
   several combined long labels and a newly created shelf: backgrounds fit,
   tabs do not overlap, shelf grows only when necessary, gear remains reachable,
   hover/selection/font/colours/height remain correct. Check all four supported
   themes on disposable shelves as practical.
6. Lock Cafe Lock: existing launcher clicks/tabs/hover still work, while editing
   and movement remain blocked. Unlock normally: maintenance editing returns.
7. Close Rainmeter normally and repeat the updater. Expect already-updated/no
   action and no additional backup.
8. On a disposable skin copy only, customize one proposed target INI or shared
   file. Expect a clear refusal identifying the target, with every file unchanged.
   Do not modify your working skin merely to provoke this test.
9. On the spare installation, close Rainmeter and inspect backup `RESTORE.txt`
   and `recovery.json`. Preserve any unexpected later edits separately. Copy only
   listed files from `original/` to the same relative installed paths; do not
   replace the whole skin or use `staged/`/`restore/` as originals. If originals
   cannot be verified against recorded hashes, stop. Start Rainmeter yourself:
   original layout should return, with positions/launcher data intact. Keep backup.

Stop after acceptance. No F2, merge, release or automatic deployment is implied.
