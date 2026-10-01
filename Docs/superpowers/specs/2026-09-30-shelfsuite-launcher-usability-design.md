# ShelfSuite launcher usability design

Date: 2026-09-30
Branch: `feature/cafe-lock-usability`
Inspected base: `2a169e9fb567d1c3a0cb9d3cebd02aa2070bc283`
Status: conversational design approved; written specification awaiting owner review.

## Purpose and owner experience

The owner should be able to create and edit ShelfSuite launchers without editing
Lua or preparing icon files. In password-authorized Maintenance Mode, open the
existing settings gear, choose Add Item or Edit Item, browse or drop an item,
review the automatically filled Name, Launch Action and Icon, then Save.

The integration uses the existing installed Shelf Suite and its configuration
and icon directories. It does not install a second skin, create a second profile,
or change the pinned ShelfSuite version.

The user approved hosting an adaptation of the existing configurator in a Cafe
Lock window using WebView2, with native Windows file selection and icon handling.
The ordinary skin engine, launcher clicks, tabs, hover effects, meters and Lua
updates continue to work when locked.

## Baselines and inspection findings

- Rainmeter baseline: v4.5.26.3894, upstream commit
  `5a124b6a09e2f7f67f8be9232718c489100e6173`.
- ShelfSuite baseline: v2.1, commit
  `d4f186ba0b5c262c7559b80841132f5fd3884f3c`.
- Follow the current root `AGENTS.md`; preserve upstream history and licenses.
- `Library/CommandHandler.cpp::RunFile` currently checks the Cafe Lock launch
  policy before dispatching through the Windows shell.
- `Library/CafeLock.h` identifies the installed
  `Shelf Suite/@Resources/configurator.html` at that boundary.
- `Library/CafeLock.cpp` owns process-local password authorization, Lock Now and
  shutdown revocation.
- The stock configurator opens in an external browser and writes through the
  File System Access API. Blocking a later launch does not revoke permission
  already granted to an existing browser page.
- Its current regular-expression Lua parser can omit valid data. Its Save flow
  can announce success without awaiting a successful write.
- Existing ShelfSuite tests exercise real skin behavior and shell dispatch.
  They do not prove actual browser saving or Explorer drag/drop.
- ShelfSuite is currently a test dependency and is not bundled as an installed
  skin. The adaptation adds editor assets, not a second ShelfSuite installation.

## Selected architecture

Use an application-owned WebView2 window to display an adapted copy of the
pinned ShelfSuite configurator. Bundle the approved editor assets with Cafe Lock,
retain the original MIT notice, and record their source revision and changes.
Do not load user-editable configurator scripts with native authority.

Intercept the existing configurator launch after checking Cafe Lock state.
While locked, the gear remains blocked. In Maintenance Mode it opens or focuses
one editor associated with this Rainmeter process and this installed ShelfSuite
root. Do not alter ordinary HTML, application, folder or URL launching.

The page controls presentation. Native code owns file selection, selected-file
inspection, icon conversion, configuration validation and disk writes. There is
no listening HTTP server, external browser extension, unlock URL, UAC maintenance
helper or public command-line editing service.

Keep the implementation divided into:
1. Editor host and lifecycle.
2. Strict, operation-specific page/native messages.
3. Windows launcher inspection.
4. Icon decoding, extraction and PNG encoding.
5. ShelfSuite data parsing and safe storage.
6. The adapted configurator UI.

Existing non-import configurator operations must use the same guarded storage
route. Do not leave a fallback to direct browser writes. Existing shelf/tab/item
and theme controls remain available in Maintenance Mode where their data is
supported; native operations are scoped to known ShelfSuite files and templates.

## User interaction

Add Item and Edit Item provide:
- Browse for launcher/file, plus a folder-selection option.
- Browse for custom icon.
- Separate launcher and custom-icon drop targets.
- Automatic name and launch-action detection.
- Automatic icon selection and a preview.
- Editable name, existing action editing, Save and Cancel.
- Clear Maintenance Mode status and access to Lock Now.

Importing prepares an in-memory draft. No launcher is executed during inspection
or preview. Do not copy EXE files, normal files or folders into the skin.
Cancel or closing the editor leaves configuration and icon destinations unchanged.
Selecting a replacement icon does not delete an existing icon.

Save must await a native success response before showing success. On failure,
keep the draft available for correction while Maintenance Mode remains active.
A successful save refreshes the affected loaded shelf only; it does not reload
all layouts. Retain existing limits imposed by ShelfSuite's available meters.

## Launcher semantics

### Windows shortcuts

Use the Windows display name, without the .lnk extension, as the initial name.
Use a native picker configured with `FOS_NODEREFERENCELINKS` so selecting a
shortcut does not silently select its target instead.

Retain the original absolute shortcut path as the action. Let the Windows shell
open the shortcut on an ordinary launcher click. Do not reconstruct its target,
arguments or working directory into a new command string. Verify arguments,
working directory and window behavior against double-clicking the same shortcut.
If current Rainmeter shell dispatch overrides shortcut semantics, permit only
the narrow corrective change demonstrated by those tests.

Prefer the shortcut's configured icon; use the target's icon when appropriate,
then a generic icon if neither is available. Reading a shortcut must not launch
it or silently repair/write it.

If the original shortcut moves or disappears later, its launcher can fail.
Explain that condition clearly; do not silently copy or retarget the shortcut.
A shortcut that requires elevation retains normal Windows behavior; the
configurator itself never elevates Rainmeter.

### Executables, folders and files

For EXE files, use their absolute path as the action. Prefer a useful Windows
application description for the name, with the filename stem as fallback.
Extract the application icon without executing or normally loading the
application into Rainmeter.

For folders, use the folder display name and path. For normal files, use the
file display name and path. Activation uses the normal Windows shell/file
association. Preserve existing manually configured URL launchers.

Correctly handle spaces, Unicode and punctuation through the JSON, Lua and
Rainmeter action layers. Never interpret selected paths as shell scripts or
concatenate them into command-interpreter invocations. If an input cannot be
represented safely, give a precise error and make no changes.

## Icons

Accept PNG and ICO files, and icon sources in EXE and LNK files.
Use Windows shell/icon APIs and a Windows image codec to obtain an appropriate
icon and encode PNG with transparency preserved. The preview must represent the
actual PNG that will be saved. Do not silently use a document thumbnail as an
application icon.

Store imported icons only in the selected installation's
`Shelf Suite/@Resources/Icons/` directory and reference their filename in the
normal item `icon` field.

Sanitize generated filenames for Windows rules and reserve them using exclusive
creation, not only an existence check. Collision numbering is case-insensitive:
`spotify.png`, `spotify-2.png`, `spotify-3.png`. Never overwrite a pre-existing
icon, including during concurrent saves.

Set explicit decoder input-size and dimension limits in the implementation plan.
Malformed, oversized or unreadable images must produce a useful error, not a
crash. Failed automatic extraction offers a generic icon and custom-icon selection.

## Page/native boundary and authorization

Browser messages are untrusted input. Validate message type, schema, sizes,
origin, editor identity, request identity and current Maintenance generation.
Expose operations such as select launcher, select icon, inspect selection,
load shelf data and save edits. Do not expose generic read/write/delete,
execute-command, evaluate-Lua, shell-launch or unlock operations.

For file drops, use WebView2's supported additional-file-object messaging and
native path inspection. Reject fabricated files without a real usable path.
Prove folder and shortcut drops early on Windows; use native Windows drop
handling within the same editor if necessary. Do not introduce a localhost
service as a workaround. Failure to deliver the requested drops is a blocker,
not permission to silently ship Browse-only behavior.

Native selection produces session-bound selection records. Do not grant the page
arbitrary filesystem access or writable handles. Validate requested destinations
against the registered ShelfSuite root using actual filesystem identity and
containment checks. Reject traversal, device paths and reparse/hard-link
situations that could redirect a write into unrelated files.

Use only bundled local executable web content at a fixed approved origin.
Block unexpected navigation, new windows, frames with native access and remote
script execution. Deny direct browser file-writing permissions. Display user
names/paths as text, escape serialized data, and remove unsafe HTML interpolation
on affected UI paths. Privileged operations must never become available to
arbitrary pages loaded in the window.

Passwords remain exclusively in the existing native password flow. They never
enter page messages, logs, arguments or editor assets.

## Lock Now, restart and operation ordering

Every editor operation checks `CafeLock::IsLocked()` and the current process-local
editor generation. Closing the editor, Lock Now, Rainmeter shutdown or a new
editor lifetime invalidates pending requests and selected-file records.
A subsequent unlock cannot revive requests from an earlier generation.

Lock Now revokes authorization before closing the editor/pickers and rejecting
late callbacks. Slow extraction runs away from the UI thread and can only return
draft data. It cannot write skin files independently.

Final filesystem commits and the lock transition must be serialized. Define the
commit linearization point: a save committed before the lock transition remains
saved; after the transition no pending save may commit. Re-check authorization
after asynchronous work and before the coordinated commit. Do not postpone Lock
Now for an unbounded worker or network operation.

Restart and reboot always begin locked. Editor closure must not interfere with
normal Windows sign-out, shutdown or restart.

## Configuration and failure safety

Retain normal `ShelfConfig` data and item `label`, `action`, and `icon` fields.
Do not execute a Lua file to parse it. Support the stock examples and ordinary
configurator-generated tables, including supported quoted strings and escaping.
Preserve unrelated data/comments through focused edits where practical.

Detect unsupported dynamic Lua or ambiguous constructs before editing. Leave
such a file byte-for-byte unchanged and explain why editing is unavailable.
The skin must still load it through its existing engine behavior. Never replace
a parsing failure with an empty/default configuration.

Record the loaded file version and reject a save if another writer changed it.
Validate data before reserving final icon files. Save through temporary files and
atomic configuration replacement, with a uniquely named recovery backup of the
previous configuration.

An icon must be fully written before configuration references it. If config
commit fails, keep the previous config usable and clean up only files owned by
that save. A crash may leave an unreferenced new icon; it must not leave a config
pointing to a partial icon or destroy old content. Never describe the multi-file
operation as universally atomic. Do not automatically remove user icons.

## Dependency and packaging

Use the WebView2 Evergreen Runtime with a pinned stable SDK and reproducible
dependency acquisition selected in the detailed implementation plan. Detect
runtime absence and required API support. Package editor assets and required
loader components and account for the prerequisite in installation.

Do not silently download/install a runtime when opening the editor. If the
prerequisite is unavailable, explain it and keep existing locked launcher
operation working. The approved installer plan must explicitly cover prerequisite
installation, offline behavior, matching source archives and license notices.

Keep Rainmeter and editor operations under the current standard user. No new
deployment ACL changes or alterations to password security are part of this work.

## Security boundary and scope exclusions

Cafe Lock controls its application editing surfaces; it is not a Windows account
sandbox. An independently running program with write access can already alter a
skin. Existing unrestricted Lua and external programs are outside this feature's
security boundary and must not be misrepresented as sandboxed.

An externally opened original ShelfSuite configurator with browser filesystem
permission is also outside the new host. Do not claim to revoke those permissions.
The integration must not create a new external writing path.

Do not add timeouts, session-lock behavior, persistent unlock flags, password
backdoors, a ShelfSuite pin upgrade, unrelated refactors or a new skin profile.
Do not replace the Lua engine unless a narrowly scoped compatibility failure
requires a separately explained corrective change.

## Likely repository changes

- `Library/CommandHandler.cpp`: route the existing configurator launch.
- `Library/CafeLock.cpp` and `.h`: integrate editor revocation/lifecycle.
- New focused host, import, icon and storage components in the appropriate
  Library/Common locations.
- Bundled adapted configurator assets with pinned provenance and MIT license.
- Visual Studio project/build definitions for those components and WebView2.
- `Build/CafeInstaller/` for assets/runtime handling and matching source packaging.
- `Tests/`, `.github/workflows/cafe-lock.yml` and Cafe Lock documentation.

The detailed plan must name exact new files and interfaces before implementation.

## Acceptance and verification

Required owner acceptance tests:

| ID | Trigger | Expected result |
| --- | --- | --- |
| A1 | Drop a .lnk | Name/action/icon populate; activation matches the shortcut. |
| A2 | Browse to a .lnk | Same result without dereferencing the shortcut. |
| A3 | Browse/drop an EXE | App path/name/icon populate and activation works. |
| A4 | Browse/drop a folder | Launcher opens that folder. |
| A5 | Browse/drop a normal file | Launcher opens through its normal association. |
| A6 | Select a PNG icon | Copied on Save, transparency preserved, item references it. |
| A7 | Select an ICO icon | Converted to PNG and correctly referenced. |
| A8 | Reuse an icon filename | Unique suffix allocated; original bytes unchanged. |
| A9 | Use Locked Mode | Launchers work; hosted editing/import/saves are rejected. |
| A10 | Unlock Maintenance Mode | Browse/drop/edit/import/save work. |
| A11 | Load existing configs | Supported data preserved; unsupported code never overwritten. |
| A12 | Run existing regressions | Cafe Lock and pinned ShelfSuite behavior still pass. |

Also test custom EXE/LNK icon selection, shortcut arguments/working directory,
spaces/Unicode/metacharacters, malformed icons, extraction failure, cancellation,
concurrent filename collisions, disk failures, externally changed configurations,
path escape/link attacks, HTML injection, wrong-origin/invalid messages,
stale requests after re-unlock, restart locked, and Lock Now during each
asynchronous stage.

Maintain unchanged upstream ShelfSuite regression coverage; add integrated-editor
coverage rather than pretending an external HTML launch proves editor behavior.
Update the expected gear destination only where the integrated feature changes it.
Hash-check upstream engine, skins, themes and original assets for unintended edits.

Use disposable profiles and standard-user tokens. Never automate the owner's
installed Rainmeter or real configuration. Run meaningful failing tests before
the fix where practical. Validate x64 binaries and run the full existing Windows
workflow plus new editor tests on the final revision.

Real Explorer drops, native dialogs, visual transparency, shell associations and
shortcut comparison need an actual disposable Windows desktop. Automate where
reliable; record remaining manual results separately. Retain real sign-out,
restart and shutdown checks. A build alone does not prove them.

## Delivery sequence and gates

1. Review this written specification.
2. Produce the detailed, testable implementation plan and obtain its review and
   execution-method choice under Superpowers.
3. Verify local status/ownership, confirm the feature branch and establish the
   existing Windows baseline.
4. Implement and test the guarded editor boundary, including early Windows drop
   validation, before the import/storage UI.
5. Implement launcher/icon inspection and reliable Add/Edit/Save in focused
   commits with matching tests.
6. Integrate packaging, run complete Windows verification, review the full diff,
   and report revision, artifacts, evidence and remaining manual checks.
7. Stop for feature review; merging and releasing need separate authorization.

The local command runner failed before starting commands during planning.
Remote source and branch state were inspected through GitHub. No local clean-state,
local diff-check, build or runtime claim is made by this specification.

## Technical references

- [ShelfSuite pinned source](https://github.com/MartinSantosT/ShelfSuite/tree/d4f186ba0b5c262c7559b80841132f5fd3884f3c)
- [WebView2 security](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/security)
- [WebView2 file-object messages](https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2webmessagereceivedeventargs2)
- [Windows file-dialog options](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/ne-shobjidl_core-_fileopendialogoptions)
- [Windows shell icon extraction](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ishellitemimagefactory-getimage)
- [WebView2 runtime distribution](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/distribution)
