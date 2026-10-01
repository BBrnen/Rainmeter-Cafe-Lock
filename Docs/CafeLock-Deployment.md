# Rainmeter Cafe Lock: installation and cafe deployment

This is a modified Rainmeter build, not an official Rainmeter release. Based on
Rainmeter 4.5.26.3894; GPL v2 or later. The matching source archive accompanies the
installer. Build instructions: run upstream Build/Build.bat rainmeter-64 4.5.26.3894
with Visual Studio 2022 C++/Windows SDK, then Build/CafeInstaller/Package.ps1 using
NSIS 3.11. GitHub Actions performs those steps and tests the result.

## Install and first use

1. Use the x64 Setup.exe from the same artifact as SHA256SUMS.txt. This review build
   is unsigned; do not confuse it with the signed official Rainmeter installer.
2. Run Setup with Windows administrator permission. Files go into
   C:\Program Files\Rainmeter Cafe Lock. Setup never launches Rainmeter elevated.
3. Sign into the cafe's standard Windows account. Open Rainmeter Cafe Lock from
   the Start menu. Settings are separate: %APPDATA%\Rainmeter Cafe Lock. Existing
   ordinary Rainmeter settings and file associations are not overwritten.
4. Right-click its tray icon, choose Unlock / Enter Maintenance Mode, and create
   the local password before customer use. In Maintenance, configure skins and
   ShelfSuite using normal Rainmeter controls. Its settings gear opens the integrated
   editor with Add/Edit, Browse/drop and automatic icons. ShelfSuite is not bundled;
   the pinned installed skin engine and assets are retained. The F1 adaptive-tab
   compatibility action is available under Manage > Settings after unlock. Setup
   installs only its protected verification data; it never updates your shelves.
5. Select Lock Now. Setup enables automatic startup for all Windows users via the
   shared Startup folder. It starts locked after sign-in, not before the login
   screen, and runs as that user. Configure the password in each cafe account before
   customer use. An administrator can remove the shortcut from shell:common startup
   to disable automatic startup; running Setup again restores it.

The default Rainmeter skins directory is retained for skin compatibility. Explicit
INI-path launches still work. To migrate an earlier review profile, close Rainmeter
and copy that profile (including CafeLock.ini) into the separate settings directory.
Do not discard CafeLock.ini: removing it permits first-run password setup again.

## Permissions and maintenance

Program files grant Administrators/SYSTEM full control and Users read/execute.
Normal Rainmeter and launched programs stay under the signed-in user's token.
Only installing/updating/uninstalling requires Windows elevation; Maintenance uses
the Cafe Lock password. Setup never adds a service, UAC maintenance helper, or
administrator startup task. Ordinary Rainmeter installations remain separate.

Settings, skins and the password verifier remain user-writable so password-only
Maintenance can edit them. Windows ACLs cannot distinguish two processes running
as the same account on the basis of this application's password. Consequently,
this is an application-management lock, not protection against arbitrary code,
Explorer, editors, Task Manager, or deletion of the verifier by the same user.
Protecting that boundary would require a different privileged service/broker or a
restricted Windows deployment; it is not silently introduced by this installer.

## Updating and removing

### Optional ShelfSuite F1 compatibility

Use this once in the normal cafe Windows account after installing Cafe Lock.
Unlock with the existing password, open Manage > Settings, and select
**Apply ShelfSuite F1 compatibility**. It uses Rainmeter's configured skin path
and the existing `Shelf Suite\@Resources` directory. Review the installation,
complete file list and proposed backup location. Cancel makes no changes;
choose Apply only when the preview is correct.

Only exact approved ShelfSuite v2.1/F1 shared files and immediate `ShelfN\Shelf.ini`
variants are accepted. Additional shelves are recognized by contents, not number.
An unknown/customized, missing, linked or busy target refuses the entire update.
Do not edit files to make them look recognized. An already-compatible installation
requires no writes or new backup. No `config.lua`, icons, themes, launcher data,
passwords or saved positions are read or modified by this action.

Before changes, the action creates and verifies a timestamped sibling
`Shelf Suite-F1-Backup-...` folder containing only changed original files and
recovery instructions. Keep the whole folder. After success, reload the affected
skins in Maintenance Mode or exit and restart Rainmeter normally. Restart is
always locked. No automatic reload or skin repositioning occurs.

If a failure reports **Recovered after failure**, originals were restored; keep
the backup and resolve the reported cause before trying again. **Manual recovery
required** means the operation is incomplete: do not reload affected shelves.
Lock Now stops further writes, including automatic recovery. Close Rainmeter
normally after authenticating if necessary. Read `RESTORE.txt` and `PHASE.txt`
in the retained backup; phase entries record intentions, so check actual paths.
Preserve any competing/unexpected target separately. Restore only the listed
original files from the verified backup to their matching installation paths;
never overwrite an unexpected file without keeping it. Holding originals and
staged outputs are retained for inspection. Ask for help if the paths or hashes
do not match the record. Recovery never involves `config.lua` or other data.

The operation runs without elevation, scripts, downloads or security-policy
changes. F2 grid snapping and the experimental PowerShell updater are not included.

Close Cafe Lock using its normal Exit control in Maintenance before running Setup
again. Setup refuses updates/uninstall while its Rainmeter.dll is in use; it does
not force-close sessions. Upgrades preserve all profiles/passwords/skins. The
upstream Rainmeter update downloader and automatic installer launch are disabled
so they cannot replace Cafe Lock with ordinary Rainmeter. Use this project's
reviewed Setup.exe for updates.

Uninstall via Windows Apps or Uninstall.exe. Only packaged files, the Cafe Lock
Start menu/sign-in shortcuts and its own uninstall registration are removed. User profiles,
passwords, skins and unknown files are retained. No recursive profile deletion or
file-association takeover is performed.

## Optional editor prerequisite

Setup bundles Microsoft's signed Evergreen standalone x64 WebView2 installer for
offline use. The build rejects mismatched SHA-256, file version or Authenticode
signer. `Notices/WebView2-runtime-manifest.json` records those exact values; SDK
and ShelfSuite license notices accompany the installed payload and source archive.
The bundled payload reference is recorded separately from its installer version.
Evergreen may subsequently update independently through Microsoft's updater.

When machine-wide Runtime is detected, setup skips it. If missing, interactive setup asks
whether to install it; No is the default. Silent setup leaves it alone unless
the administrator explicitly supplies `/INSTALLWEBVIEW2=1`. After an attempted
installation, setup checks the result and Runtime presence; failure returns 1603
with an explanation while leaving Rainmeter installed. A restart-required result
is accepted only when the Runtime is detected. Setup never launches Rainmeter
elevated, and uninstall never removes the shared Microsoft Runtime.

Opening the editor never downloads/installs a prerequisite. If Runtime is missing,
existing launchers still work and the editor shows an actionable error. Check it
in the intended standard-user cafe account: a Runtime installed only for a
different Windows user may not be available there. The Windows installer uses
Microsoft's documented registry views and standalone `/silent /install` flow.
An administrator-only HKCU Runtime does not satisfy the shared prerequisite:
https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/distribution.

Final editor acceptance and supported-config limits are recorded in
`Docs/CafeLock-Usability-Verification.md`.

## Validation boundary

CI verifies x64 payloads, Stage 4 password/lock regressions, native installer build,
installation and upgrade, restricted-user denial of program-directory writes,
normal installed runtime/password behavior, profile preservation, and uninstall.
The NSIS bootstrap executable itself is 32-bit and installs only the x64 runtime;
this is normal NSIS packaging. Artifacts include hashes and matching source.
Physical Windows reboot/sign-out/shutdown and complete ShelfSuite interaction are
still Stage 6 hands-on compatibility checks. Do not deploy this review build to a
customer machine until those checks and your Windows restrictions are accepted.
