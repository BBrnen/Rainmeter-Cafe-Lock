# ShelfSuite launcher usability review

This feature uses the existing ShelfSuite v2.1 installation pinned at
`d4f186ba0b5c262c7559b80841132f5fd3884f3c`. No second skin or profile is installed.
The original engine, shelves, themes and icons remain unchanged unless an owner
explicitly saves a supported edit in Maintenance Mode.

## Using the editor

Unlock Cafe Lock with its password, then click the existing ShelfSuite settings
gear. Choose a shelf/tab and Add Item or Edit. Browse or drop one shortcut,
application, folder or file into the launcher area. The name, original launch
path and icon are filled where Windows can supply them. A shortcut keeps its
original `.lnk` path so Windows handles its arguments and working directory.
Browse/drop a PNG, ICO, EXE or LNK into the custom icon area to replace the draft
icon. Preview and correct the fields, then Save. Cancel does not save the draft.

Icons are normalized to PNG with transparency and copied only during Save.
Existing filenames are reserved safely; collisions receive a numeric suffix.
Configuration changes preserve unrelated supported literal data and comments,
and keep recovery copies. External changes require reloading or are preserved
and reported; the editor is not a universal filesystem transaction against
hostile programs running as the same Windows account.

Tabs, supported themes and shelves can also be managed here. Removed shelves
are moved intact into an `@Resources/CafeShelfRemoved-...` recovery folder.
After adding a shelf, use normal Rainmeter Manage / Refresh all to discover and
load it. Other running skins are not automatically refreshed.

Lock Now closes and revokes the editor, including outstanding picker/import/save
requests. Restart always starts locked. Existing launcher clicks remain available.

## Compatibility limits

Executable Lua configs, ambiguous custom layouts/includes, BOM-bearing configs,
unrepresentable characters under the pinned Lua bridge, unsafe action syntax,
new names containing `#`, `%`, `[` or `]` (interpreted by the pinned engine),
reparse points and hardlinked edit targets are rejected without rewriting them.
Existing unsupported configs can continue running; the editor explains that they
are read-only. Review the existing config before replacing it yourself.

WebView2 Runtime is required for the editor, not ordinary Rainmeter launchers.
The optional setup prerequisite is bundled for offline use and never downloaded
or installed by opening the editor. A missing/incompatible Runtime produces an
actionable native error. Declining it does not unlock or disable launchers.

## Automated evidence

At application revision `87d09a917483f7c2cb713a3204935524e1b67464`,
[run 36741276257](https://github.com/BBrnen/Rainmeter-Cafe-Lock/actions/runs/36741276257)
passed 299 editor assertions: protocol 56, host policy 25, controller 19,
selection 30, lifecycle 14, actual WebView 47, metadata 19, icons 16, config 31,
storage 42. Twelve executable prerequisite-policy checks and three actual signed
installer acquisition/reject-cache checks passed. The complete x64 build, password/policy, standard-user running-app,
pinned ShelfSuite, installer/upgrade/uninstall and artifacts also passed.
The installer artifact contains the offline prerequisite, matching source,
license notices, exact Runtime provenance and checksums. A later documentation-only
commit records this evidence; it does not change the application implementation.

Coverage includes real browser Add/Edit/Cancel/Save flows, typed Browse/drop
wiring, stale response/revocation, XSS-like labels, transparent PNG/ICO conversion,
EXE/LNK extraction without launching, duplicate reservation, Lua escaping,
concurrent edits/recovery and locked write refusal. The real loaded skin compares
shortcut arguments, working directory and standard-user token with a Windows
shell baseline. Tests do not prove every shell extension/icon handler or disk
failure; partial ReplaceFile failure recovery has not been fault-injected.
The WebView harness runs as the runner identity. The separate filtered-token
ShelfSuite test reported the permitted native Runtime error surface; it proves
guarded routing and launcher compatibility, not that the final editor rendered
under that standard-user token. Actual standard-user editor acceptance remains
mandatory on the spare PC/VM.

Independent whole-branch review identified three Important findings. Their
regressions were observed failing, then passed after the fixes: administrator-only
per-user Runtime no longer satisfies machine-wide setup; new item/tab/shelf names
cannot undergo Rainmeter expansion; slow mutations remain tracked until the
native result and cannot enable retry/Cancel merely because a UI timer expired.
The fresh complete workflow passed after the fix pass; no second review is implied.

## Manual standard-user acceptance

The owner's medium-integrity Explorer drop probe passed for LNK, EXE, folder and
associated-file original paths. That proves the Windows bridge feasibility,
not all final editor/import/save behavior. Final production acceptance remains
pending on a spare PC/VM with this review build and the pinned ShelfSuite.

| ID | On the spare PC/VM | Expected result | Status |
| --- | --- | --- | --- |
| A1 | Add by dropping an original LNK, then Save/click | Name/icon fill; arguments and working directory match double-click | Pending |
| A2 | Add by Browse to the same LNK | Same result, original shortcut retained | Pending |
| A3 | Browse/drop an EXE | Name/path/icon fill; saved launcher starts the app | Pending |
| A4 | Browse/drop a folder with spaces | Saved launcher opens that folder | Pending |
| A5 | Browse/drop an associated document | Saved launcher opens in its normal application | Pending |
| A6 | Browse/drop a transparent PNG custom icon | Preview and saved transparency match; icon copied/referenced | Pending |
| A7 | Browse/drop an ICO custom icon | PNG conversion and launcher image are correct | Pending |
| A8 | Import two icons with the same basename | Unique suffix; existing icon bytes unchanged | Pending |
| A9 | Lock Now; click icons/tabs/gear; try Manage/Edit | Launchers/hover/tabs work; editing/import is blocked | Pending |
| A10 | Password unlock; edit/save; lock during picker/import; unlock again | Maintenance works; late old requests never save | Pending |
| A11 | Open existing configs, including a custom unsupported one | Supported data retained; unsupported file stays unchanged | Pending |
| A12 | Repeat normal Cafe Lock/ShelfSuite use | No regression in password, movement lock, launchers or tabs | Automated checkpoint passed; manual pending |

Also check theme changes and shelf add/remove, restart into Locked Mode, real
Windows sign-out/restart/shutdown with editor/picker/password dialog open, and
cancel one shutdown. On a separate disposable VM missing WebView2, verify setup
offers it, decline leaves Rainmeter launchers usable, and accepting it works
offline. Silent setup without opt-in must not install it. An explicit failed
prerequisite install must report failure without launching Rainmeter elevated.
Do not remove the owner's shared Runtime to manufacture this environment.

## Merge gate

PR #6 remains draft and unmerged. A final green Windows run, independent code
review and the manual results above must be recorded before calling it ready to
merge. This document does not authorize merging, releasing or deployment.

The independent review's minor cache-cleanup finding is deferred: each editor
lifetime creates a private temporary WebView profile, which can consume disk over
repeated use. Automatic ownership-checked cleanup after browser-process exit is
not implemented in this feature. It does not grant new editing authority.

## Shelf-loading blocker retest (2026-10-01)

The owner's real installation exposed an all-or-nothing discovery bug. An
unreadable or missing Shelf.ini in one ShelfN folder discarded the complete
discovery result. The focused fix retains healthy shelves, reports each skipped
folder/file with a safe reason (including Windows read-error number), and reports
configuration parsing failures separately. Folder diagnostics shows the root
Rainmeter supplied. Discovery and reload never repair or rewrite installed files.

On a spare PC/VM using the new review artifact:
1. Back up the existing Shelf Suite folder. Install the review build on this
   disposable machine; use the same standard-user profile and existing skins.
2. Start locked. Confirm existing launcher clicks still work and the gear cannot
   open the editor. Unlock with the existing password and open the gear.
3. Expand Folder diagnostics and confirm the root is the installed Shelf Suite.
   Shelf1/Shelf2/Shelf3 and their ONLINE/OFFLINE/INTERNET tabs should be available.
   Record any warnings verbatim; do not manually repair existing folders yet.
4. Without changing existing files, create one new, empty, unused ShelfN folder
   (for example Shelf999 if it does not already exist). Click Reload shelves.
   The healthy shelves must stay usable; a warning must name Shelf999/Shelf.ini
   and Windows error 2. No automatic deletion or repair should occur.
5. Click Add shelf, enter a temporary test name and save. The new shelf must
   immediately appear and be selected after reload. The broken-folder warning
   must remain visible. New desktop loading still uses normal Rainmeter Manage.
6. Compare the original three config.lua files with the backup: loading, reload,
   and Add shelf must not change those bytes. Select each original shelf and
   check its existing tabs/items without saving edits.
7. Click Lock Now and confirm the editor closes and existing launchers work.
   Restart Rainmeter and confirm it starts locked again.
8. Send the result and any warning text. Leave original folders/configs intact.
   Test-only folders may be removed manually after recording results.

Automated mixed-shelf tests also cover locked files, actual Windows read-access
denial, malformed INI/Lua, missing config fallback, warning privacy, unchanged
configuration bytes, and actual WebView Add/reload. They do not establish which
specific file failed on the owner's machine. Manual acceptance remains pending.
