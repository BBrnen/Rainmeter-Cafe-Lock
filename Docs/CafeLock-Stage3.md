# Stage 3: locked-mode enforcement

This review build is always locked, from process startup. There is no unlock
setting, command, password, or Maintenance Mode yet. It is for testing an
already configured skin setup, not production deployment or first-time setup.

The central policy is `Library/CafeLock.h`. Existing input, command, dialog,
tray, and layout entry points consult that policy. User-requested skin loading,
unloading, layout replacement, editing, configuration writes, window movement,
selection, and management menus are denied. Bang aliases and group commands
are classified by their resolved enum, after normal command parsing. Startup
activation and invalid-skin cleanup use private internal paths so locking does
not interfere with loading saved skins or safely disposing broken skins.

Mouse hit testing and the Windows move command both enforce the lock. Ctrl,
Shift, Alt, Ctrl+Alt selection, keyboard movement, and group repositioning do
not unlock it. Tray creation/recovery and tray command callbacks are guarded,
including when there are no active skins. Game Mode is disabled while locked
because it can unload skins. Startup safe-mode layout replacement and automatic
upstream update checks are suppressed while locked.

Ordinary left-click down/up/double-click actions, app launching, in-memory meter
changes, hover effects, Lua, timers, and meter updates remain available.
Persistent `!WriteKeyValue` configuration changes require maintenance and are
blocked; `!SetOption` and `!SetVariable` still work in memory. Skin scripts that
depend on loading/unloading other skins or moving whole windows are intentionally
restricted. Moving meters inside a skin is unaffected.

The stock ShelfSuite gear launches `Shelf Suite/@Resources/configurator.html`
under the configured skin directory. Rainmeter blocks that target at its
shell-launch boundary, normalizing case, separators, and dot segments. Other
HTML documents and ordinary launcher targets remain allowed. ShelfSuite files
are not modified. A future Maintenance Mode will release this same guard.

This is an application interaction lock, not an operating-system sandbox:
arbitrary external programs, direct file editing, native plugins, and Lua file
I/O are not confined. Wrapped browser/shell commands or renamed copies of the
configurator are outside the stock gear guard. Protecting files and restricting
other software requires Windows account/ACL policies. No installer or ACL
changes are included in this stage.

## Validation

The Windows workflow builds x64 using the unchanged upstream Build.bat, checks
the architecture of the required binaries, runs the standalone policy tests,
and exercises the running application with a disposable skin. The smoke test
checks modifier-key hit testing and direct movement messages, keyboard and menu
routes, management through both skin and main-window IPC, tray command routes,
positive click/Lua/tab/hover behavior, and a harmless launcher action.

The artifact is a review build without an installer. Interactive testing of the
actual ShelfSuite package remains the separate compatibility stage. PR #1 stays
unmerged; this branch is based on its build-validation commit for traceability.
