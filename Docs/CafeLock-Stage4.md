# Stage 4: session Maintenance Mode

Rainmeter starts locked every time. Right-click its tray icon and select **Enter
Maintenance Mode (administrator)**. Approve Windows UAC (or supply administrator
credentials). The tooltip changes to **Maintenance**. Normal skin and management
behavior then resumes. **Lock Now**, at the top of Rainmeter's context menu,
relocks immediately. If a custom TrayExecute action replaces right-click, hold
Ctrl while right-clicking the tray to reach the normal menu.

The tray remains available even with TrayIcon=0 so authorization and Lock Now
cannot become inaccessible. While locked, the tray menu contains only the
maintenance request. Skin context menus remain suppressed.

There is no setting, bang, password, environment variable, registry key, startup
argument or file that grants maintenance. No timeout or workstation-lock policy
has been added. The only unlock path is the authenticated live helper connection.

## Authorization boundary

`CafeSecurity::Authorization` creates a fresh GUID-named, local-only, first-instance
named pipe for each request. Rainmeter launches the fixed sibling executable
`RainmeterCafeMaintenance.exe` using ShellExecuteEx `runas`. That executable also
has a `requireAdministrator` manifest. Rainmeter retains the returned process
handle; the helper does not launch Rainmeter or any user programs.

On receiving the fixed, non-secret request, Rainmeter verifies all of:

- The kernel-reported pipe client PID matches the still-running process handle
  returned by its own UAC launch.
- The actual pipe-client token is elevated, has an enabled Administrators SID,
  has high (or higher) integrity, and belongs to Rainmeter's Windows session.
- The helper checks the pipe server PID and process creation time against the
  requesting instance. PID reuse or a different running instance is not enough.

The helper opens the connection with `SECURITY_IDENTIFICATION`. Rainmeter can
inspect identity and privileges but cannot act with the helper's administrator
privileges. It reverts immediately after checking the token. See Microsoft's
[impersonation levels](https://learn.microsoft.com/en-us/windows/win32/secauthz/impersonation-levels)
and [named-pipe impersonation](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-impersonatenamedpipeclient).

Only completed, authenticated pipe I/O changes in-memory authorization. Timer
messages merely poll the pipe. No window message, copied payload, arbitrary PID
or pipe name grants authorization. Lock Now cancels pending I/O, so a late helper
response cannot unlock the relocked instance. The helper exits after its exchange;
its 30-second exchange bound is not a maintenance-session timeout.

Windows applies its configured UAC policy. A system configured to approve elevation
silently may not show a prompt. The code requires the elevated administrator token;
it does not invent a replacement credential dialog.

This is an application-management lock, not isolation from arbitrary code running
as the same Windows user. Process injection, replacing writable program files,
external editors/configurators already open when relocking, and native skin
plugins are outside this boundary. Program/install ACLs are intentionally deferred
to the deployment stage. Lock Now closes Rainmeter's own management dialogs and
cancels selection/dragging; it does not terminate unrelated user programs.

## Shutdown and keyboard behavior

The control window immediately accepts WM_QUERYENDSESSION and exits on confirmed
WM_ENDSESSION, without relying on WM_CLOSE. A cancelled shutdown preserves the
running session. This follows Microsoft's
[Windows end-session protocol](https://learn.microsoft.com/en-us/windows/win32/shutdown/wm-queryendsession).

The upstream Skin::OnKeyDown handler only moves selected skins with arrow keys;
it does not dispatch keyboard events to skins. Its lock check now surrounds only
that movement block. InputText and other plugins have their own input windows.
ShelfSuite files are unchanged; its existing configurator launcher guard applies
only while locked and naturally stops applying after authorization.

## Validation and review

CI builds Release x64 including the maintenance helper, verifies PE architecture,
and runs the locked policy, actual pipe/token, and running Rainmeter tests. Tests
cover a forged request from a restricted token, authenticated helper success,
late-response cancellation, repeat authorization, relock, movement, Manage/Edit,
configuration writes, activation/unload, and process restart. The CI standard-user
launcher is a test executable only and is not included in the release directory.
The ordinary hosted desktop pass covers authenticated maintenance transitions and
modifier input. The restricted-token pass verifies locked enforcement, standard-user
app launching, and rejection when its helper starts without elevation. On this
hosted runner, runas from the synthetic restricted token does not produce an
elevated helper; the helper exits with code 2 and Rainmeter stays locked. This is
**not** proof of successful credential-based unlocking from a real standard-user
account. That remains required manual validation before Stage 4 approval.

End-session tests send Windows' query, cancellation and confirmation messages to
the test process; they do not reboot or sign out the runner. A hosted runner also
cannot verify a human-approved/cancelled secure-desktop credential prompt.

Before approving Stage 4, validate on a disposable Windows standard-user session:

1. Cancel the actual UAC prompt: dragging, menus and settings remain locked.
2. Supply administrator credentials: unlock only the requesting instance. Confirm
   Rainmeter and an app launched by a skin remain standard-user processes.
3. Open ShelfSuite settings, exercise tabs/hover/launchers and any installed input
   plugins. Lock Now; its settings launcher is blocked again.
4. Exit/relaunch while in maintenance: the new process is locked.
5. Sign out, restart and shut down Windows with locked Rainmeter running, and
   repeat with an authorization prompt pending. Windows must finish normally.

Installer work and deployment ACLs are not part of Stage 4. PRs remain stacked
and unmerged for review.
