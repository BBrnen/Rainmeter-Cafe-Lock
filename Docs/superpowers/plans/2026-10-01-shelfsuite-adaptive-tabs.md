# ShelfSuite Adaptive Tabs Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the existing ShelfSuite tab background fit each rendered label while preserving the 85 px minimum and normal 480 px shelf layout.

**Architecture:** Keep the approved ShelfSuite v2.1 source as the declared base and store a small, reviewable F1 patch rather than copying a second skin. The patch changes only ShelfSuite's shared tab-layout Lua, shared variables, and standard shelf INIs; Rainmeter's existing Cafe Lock code remains unchanged. The real-skin regression fixture checks out that exact upstream revision, applies the patch, and verifies live meter geometry under Rainmeter.

**Tech Stack:** Rainmeter 4.5 Lua skin API, ShelfSuite v2.1, PowerShell disposable Windows fixture, GitHub Actions Windows x64 build.

**Spec:** `Docs/superpowers/specs/2026-10-01-shelfsuite-adaptive-tabs-design.md`

## Global Constraints

- F1 only; do not implement F2 grid snapping.
- Keep `TabWidth=85` as the minimum width and add exactly 12 px horizontal padding on each side of measured text.
- Preserve current tab height, font, colours, rounded shape, hover, selection, and centre alignment.
- Keep short-tab shelves at 480 px; grow only when the calculated tab row requires it.
- Preserve existing `config.lua`, shelf positions, launcher behavior, Cafe Lock password handling, and Locked Mode policy.
- Retain ShelfSuite v2.1 attribution and MIT licence; do not silently claim modified files are unchanged upstream source.
- Do not bundle, release, deploy, or automatically alter a real user's ShelfSuite installation in F1.
- Run fixture/browser/core tests, `git diff --check`, and the full Windows x64 workflow before claiming F1 ready.

## Review Focus

- Labels whose measured text is under 61 px still render as an 85 px tab; Task 2 live-meter test owns this.
- A long label followed by another tab has spacing after its calculated width and never overlaps; Task 2 owns this.
- A five-tab row grows its shelf and moves its gear without changing a short 480 px row; Task 2 owns this.
- Existing or new shelves retain their `config.lua` bytes and load without a migration; Tasks 2 and 3 own this.
- A real user never receives a CI-only patch with no safe delivery path; Task 4 records the versioned compatibility-package contract.

---

### Task 1: Establish the version-locked compatibility patch and RED fixture

**Files:**
- Create: `ThirdParty/ShelfSuite/patches/0001-cafe-lock-adaptive-tabs.patch`
- Modify: `.github/workflows/cafe-lock.yml`
- Modify: `Tests/CafeShelfSuite.ps1`
- Modify: `Docs/CafeLock-Compatibility.md`

**Interfaces:**
- Consumes: upstream `MartinSantosT/ShelfSuite` revision `d4f186ba0b5c262c7559b80841132f5fd3884f3c` checked out at `work-package/ShelfSuite`.
- Produces: an applied disposable `Shelf Suite` fixture whose patch base and exact changed paths are asserted before Rainmeter starts.

- [ ] **Step 1: Write the failing live-layout assertions in `Tests/CafeShelfSuite.ps1`**

Add `Read-ShelfLayout($window)` using the existing Lua command-measure route. It must record each present `MeterTabNBg` x/width, each `MeterTabNText` measured width, `SKIN:GetW()`, and the settings-gear x. Configure one disposable shelf with `ONLINE`, `OFFLINE`, `INTERNET`, and another with `SCHOOL/WORK` followed by a short label. Assert the required 85 px minimum, 12 px sides, non-overlap using the existing `TabSpacing`, 480 px short width, required long width, and gear x of widget width minus 28.

- [ ] **Step 2: Run the existing fixture through the new assertions to verify RED**

Run the existing disposable `cafe-lock-x64` workflow with the unmodified upstream checkout and the new assertions enabled.

Expected: the geometry assertions fail because upstream always uses fixed `TabWidth` and leaves `WidgetWidth` at 480 px. Existing Cafe Lock policy and launcher assertions remain meaningful.

- [ ] **Step 3: Add the initial, intentionally incomplete patch manifest**

Create a unified patch whose header identifies the exact upstream commit, upstream MIT licence, and only these targets: `Shelf Suite/@Resources/ShelfEngine.lua`, `Shelf Suite/@Resources/Variables.inc`, and `Shelf Suite/Shelf1` through `Shelf3` `Shelf.ini`. Do not copy icons, user `config.lua`, configurator files, or Rainmeter code.

- [ ] **Step 4: Make CI apply and verify the patch before the fixture starts**

In `.github/workflows/cafe-lock.yml`, after the pinned checkout, run `git apply --check` and apply this exact patch. In `Tests/CafeShelfSuite.ps1`, retain the upstream revision check, replace the blanket unchanged-source hash assertion with checks that only the declared patch paths differ, and continue hashing user configuration and untouched upstream assets.

- [ ] **Step 5: Run the focused fixture to verify it still fails only for the missing layout behavior**

Run the `cafe-lock-x64` ShelfSuite fixture path on a disposable Windows runner.

Expected: patch provenance and protected-file checks pass; adaptive-geometry assertions stay red until Task 2.

- [ ] **Step 6: Commit the RED test and patch scaffold**

Run: `git add Tests/CafeShelfSuite.ps1 .github/workflows/cafe-lock.yml Docs/CafeLock-Compatibility.md ThirdParty/ShelfSuite/patches/0001-cafe-lock-adaptive-tabs.patch` then `git commit -m "test: define adaptive ShelfSuite tab layout"`.

### Task 2: Calculate widths from live text meters

**Files:**
- Modify: `ThirdParty/ShelfSuite/patches/0001-cafe-lock-adaptive-tabs.patch`
- Modify: `Tests/CafeShelfSuite.ps1`

**Interfaces:**
- Consumes: the applied F1 patch fixture from Task 1 and `SKIN:GetMeter(name):GetW()`.
- Produces: `UpdateTabs()` that assigns each background/text meter a computed width and x position, and assigns the runtime `WidgetWidth`/gear position for the resulting row.

- [ ] **Step 1: Confirm the long-label live assertion remains RED**

Run the targeted ShelfSuite fixture created in Task 1.

Expected: `SCHOOL/WORK` is still 85 px or the next tab is positioned with the fixed-width calculation.

- [ ] **Step 2: Implement the minimal patch to `ShelfEngine.lua` and `Variables.inc`**

Add `TabHorizontalPadding=12` while retaining `TabWidth=85`. In `UpdateTabs()`, set each label, update its meter, read the measured width, choose `max(TabWidth, measured + 2 * TabHorizontalPadding)`, then position the current tab from a running x coordinate and advance by that width plus existing `TabSpacing`. Use a separate immutable base-width variable of 480 px so a prior long tab cannot make a later short layout remain expanded. Set the background shape and settings gear from the calculated runtime width. Preserve the existing active/inactive colours and meter actions.

- [ ] **Step 3: Enable dynamic window resizing in the standard ShelfSuite INIs**

Add `DynamicWindowSize=1` to the `[Rainmeter]` section of patched Shelf1, Shelf2, and Shelf3 `Shelf.ini` files. Do not modify any `config.lua` file or write user positions.

- [ ] **Step 4: Run the targeted live fixture to verify GREEN**

Run the targeted Windows ShelfSuite fixture.

Expected: short labels remain 85 px and 480 px wide; the long label and its following tab do not overlap; its shelf widens only when needed; the gear remains at width minus 28; normal hover, tab switching, launcher dispatch, Locked Mode and Lock Now checks pass.

- [ ] **Step 5: Commit the adaptive layout patch**

Run: `git add ThirdParty/ShelfSuite/patches/0001-cafe-lock-adaptive-tabs.patch Tests/CafeShelfSuite.ps1` then `git commit -m "feat: adapt ShelfSuite tab widths to labels"`.

### Task 3: Give Cafe Lock-created shelves the same layout contract

**Files:**
- Modify: `Library/CafeShelf/ShelfTemplate.h`
- Modify: `Tests/CafeShelfStorage.cpp`
- Modify: `Tests/CafeShelfBrowser.cpp`
- Modify: `Tests/CafeShelfTests.ps1`

**Interfaces:**
- Consumes: the existing native `Storage::Prepare(EditKind::AddShelf)` template path.
- Produces: every shelf created by the guarded Cafe Lock editor contains `DynamicWindowSize=1`, matching the compatibility patch without changing a pre-existing shelf.

- [ ] **Step 1: Write failing template assertions**

In the existing storage/browser test fixtures, create a shelf through the real Add Shelf operation. Assert that its generated `Shelf.ini` has a `[Rainmeter]` `DynamicWindowSize=1` entry, its generated `config.lua` remains valid, and it appears after the existing reload path.

- [ ] **Step 2: Run the focused native editor suite to verify RED**

Run: `pwsh -NoProfile -File Tests/CafeShelfTests.ps1 -Suite All` on the disposable Windows runner.

Expected: the new generated-INI assertion fails because the original template omits the setting; existing storage and browser tests pass.

- [ ] **Step 3: Add the single template setting**

Insert `DynamicWindowSize=1` beside the existing `[Rainmeter]` defaults in `Library/CafeShelf/ShelfTemplate.h`. Do not change storage authorization, path checks, browser protocol, or generated `config.lua` structure.

- [ ] **Step 4: Run the focused native editor suite to verify GREEN**

Run: `pwsh -NoProfile -File Tests/CafeShelfTests.ps1 -Suite All`.

Expected: template/add/reload assertions and all existing editor security checks pass.

- [ ] **Step 5: Commit the new-shelf template change**

Run: `git add Library/CafeShelf/ShelfTemplate.h Tests/CafeShelfStorage.cpp Tests/CafeShelfBrowser.cpp Tests/CafeShelfTests.ps1` then `git commit -m "feat: enable adaptive sizing for new ShelfSuite shelves"`.

### Task 4: Record deployable same-skin delivery and verify the whole branch

**Files:**
- Modify: `Docs/CafeLock-Compatibility.md`
- Modify: `Docs/CafeLock-Deployment.md`
- Modify: `Docs/superpowers/specs/2026-10-01-shelfsuite-adaptive-tabs-design.md`
- Create: `Docs/CafeLock-ShelfSuite-F1-Delivery.md`
- Create: `Tests/CafeShelfSuiteDelivery.ps1`

**Interfaces:**
- Consumes: the exact upstream revision and F1 patch from Tasks 1--2.
- Produces: a future release contract for an owner-visible, same-name compatibility update; it is documentation and audit only and does not package or install anything in F1.

- [ ] **Step 1: Write the failing delivery audit**

Create `Tests/CafeShelfSuiteDelivery.ps1`. It must fail unless the F1 patch declares its upstream base, changed paths, MIT attribution, and an explicit future delivery artifact name. It must also fail if the documentation says the normal Cafe Lock installer already deploys the patch.

- [ ] **Step 2: Run the audit to verify RED**

Run: `pwsh -NoProfile -File Tests/CafeShelfSuiteDelivery.ps1`.

Expected: it fails until the versioned delivery contract and non-installer status are documented.

- [ ] **Step 3: Document the future compatibility-update package**

Specify a future separately versioned `ShelfSuite-Cafe-Lock-F1-Compatibility.zip`, generated only from the committed patch and exact v2.1 base. Its installer/update step must require the owner to back up `Shelf Suite`; verify upstream engine/variables hashes before modification; make timestamped backups; update the same `Shelf Suite` directory only; add `DynamicWindowSize=1` only to recognised `[Rainmeter]` sections of existing `ShelfN/Shelf.ini` files; never read, rewrite, or upload `config.lua`; and stop with an actionable message on unrecognised or customised files. A later release task, not F1, implements and ships it.

- [ ] **Step 4: Run the audit to verify GREEN**

Run: `pwsh -NoProfile -File Tests/CafeShelfSuiteDelivery.ps1`.

Expected: provenance, delivery, backup, hash-verification, and no-installer claims are explicit.

- [ ] **Step 5: Run complete verification**

Run `git diff --check`, `pwsh -NoProfile -File Tests/CafeShelfTests.ps1 -Suite All`, then the complete `cafe-runtime-policy`, `cafe-shelf-core`, and `cafe-lock-x64` Windows GitHub Actions workflow.

Expected: all existing policy, password, editor, standard-user, ShelfSuite, installer and new adaptive-layout checks pass.

- [ ] **Step 6: Commit documentation and verification records**

Run: `git add Docs Tests` then `git commit -m "docs: define ShelfSuite adaptive tab delivery"`.

## Manual acceptance after automated verification

1. On a spare PC/VM, back up the existing `Shelf Suite` folder and apply the reviewed F1 compatibility update when its separate delivery package exists.
2. In Maintenance Mode, verify Shelf1, Shelf2, and Shelf3 load without changing their `config.lua` or desktop positions.
3. Compare short `ONLINE`, `OFFLINE`, and `INTERNET` tabs: each remains compact with the 85 px minimum and the shelf remains 480 px wide.
4. Rename a tab to `SCHOOL/WORK`, refresh it, and verify its coloured background contains the label, later tabs do not overlap it, and the gear stays at the far right.
5. Test five long labels; verify the shelf grows only enough to show every tab and stays usable on the active monitor.
6. Create a shelf with the existing editor, add a long tab name, refresh it, and repeat the width/gear check.
7. Lock Cafe Lock, confirm launchers and tabs still work and configuration remains blocked; unlock and confirm normal editing still works.

## Plan self-review

- **Spec coverage:** Tasks 1--3 cover minimum sizing, measured labels, padding, sequential positions, dynamic width, gear placement, legacy configurations, and new shelves. Task 4 covers the explicitly requested deployable path without performing prohibited release work. F2 and Cafe Lock policy changes are excluded.
- **Step scan:** Each task starts with a behavior-specific red test, then its minimal implementation, green check, and focused commit.
- **Type consistency:** The plan uses existing Lua `SKIN:GetMeter(name):GetW()`, existing `UpdateTabs()`, existing `Storage::Prepare(EditKind::AddShelf)`, and existing Windows fixture entry points; no new Rainmeter native API is invented.
- **Review focus:** Each listed risk has a named task and live/fixture or template test.
- **Proportion:** The plan names exact files and commands while leaving Lua/patch mechanics to the implementation tasks, avoiding an unnecessary redesign.
