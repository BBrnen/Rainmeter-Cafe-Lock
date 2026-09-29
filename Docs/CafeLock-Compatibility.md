# Stage 6 compatibility checks

The Windows workflow builds x64 Rainmeter Cafe Lock and exercises the real
application, its native password dialogs, and the installer. This is a review
build, not a claim that every third-party skin or Windows environment is tested.

## ShelfSuite target

- Repository: https://github.com/MartinSantosT/ShelfSuite
- Version: repository v2.1, revision `d4f186ba0b5c262c7559b80841132f5fd3884f3c`.
- CI checks out that exact source, copies it to a disposable skin profile, and
  loads all three shelves. It creates only a new `Shelf1/config.lua` containing
  harmless test application paths, using ShelfSuite's normal configuration API.
- The actual stock icon/tab/gear actions and Lua engine run unchanged. File
  hashes verify that no existing ShelfSuite file was changed by the test.
- ShelfSuite is not bundled in the installer or maintained as a separate fork.

`Tests/CafeShelfSuite.ps1` checks launcher clicks, tab content, hover and leave
effects, Lua-driven meter changes, and application launches with a standard-user
token. It also checks blocked movement and context menus while locked, and
configurator dispatch before unlock, after password setup/unlock, and after
Lock Now. Test-only Lua observers are appended in memory after the original
hover/leave actions to record the resulting icon positions in the same callback;
this avoids missing a brief transition on the hosted desktop. The original
actions still execute, and no ShelfSuite source file is edited.
A temporary HTML handler routes real ShellExecute requests to a
harmless recorder instead of opening a browser. A control launch verifies this
association before the locked-gear assertion. The original association is
restored in `finally`, and the test refuses to run outside GitHub Actions.

## General regression coverage

The existing policy, password and runtime tests cover password creation,
confirmation mismatch, incorrect passwords, change requiring the current
password, old-password rejection, new-password acceptance, Lock Now, fresh
process startup locked, Manage/Edit prompting, configuration writes, and
absence of plaintext passwords in configuration/logs. They also cover modifier
dragging, group/keyboard movement, launcher actions, Lua timers, and hover.

The keyboard restriction applies to Rainmeter's selected-skin arrow-key
repositioning block. The handler has no general skin keyboard-event dispatcher;
the stock ShelfSuite examined here uses mouse actions and Lua for interaction.
There is no additional blanket suppression of all keyboard messages.

Installer checks exercise install, standard-user execution and permissions,
upgrade, refusal to replace running binaries, and uninstall preservation of user
files. The shared Startup shortcut points directly to Rainmeter without
arguments or an elevation flag, is exercised by the standard-user test, remains
after upgrade, and is removed on uninstall. Startup occurs at Windows **sign-in**,
not before a user signs in. Each Windows account has its own password/profile.

## Checks requiring a disposable Windows desktop

The automated runtime test sends Windows end-session queries for shutdown,
restart and sign-out, checks cancellation keeps Rainmeter alive, and checks a
confirmed sign-out message exits even while locked. It does **not** perform an
actual operating-system shutdown, restart or sign-out on the hosted runner.

Before deployment, on a spare PC or VM:

1. Install, sign out, and sign back in. Confirm automatic startup in Locked Mode.
2. Restart and then shut down Windows with Cafe Lock locked. Confirm there is no
   application-blocking prompt or abnormal delay. Repeat with a password dialog
   open, and cancel a shutdown once to confirm the application remains usable.
3. Unlock, restart Windows, and confirm Maintenance Mode did not persist.
4. In Maintenance Mode, open the actual ShelfSuite configurator in the intended
   browser, save a normal configuration, and refresh the shelf. Confirm the
   settings gear is blocked again after Lock Now. CI verifies launch dispatch,
   not browser-specific file permissions or the configurator's save UI.

These physical/session and browser checks must be recorded separately; they are
not implied by a green CI run. Do not restart or sign out an active user's PC as
part of the automated test.
