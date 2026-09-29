# Stage 4: password Maintenance Mode

Rainmeter starts locked every time. Right-click the tray and choose **Unlock /
Enter Maintenance Mode**. On first use, create and confirm a local Cafe Lock
password. Later requests show **Enter Cafe Lock Password**, with Unlock and
Cancel. An incorrect password leaves the instance locked and displays an error.

Maintenance restores normal controls. **Lock Now** immediately relocks and closes
Rainmeter management/password dialogs. Manage > Settings includes **Security /
Cafe Lock settings**, current mode, Change password, and Lock Now. Changing the
password requires the current password and matching new entries. Restart never
retains authorization. There is no timeout or Windows-session-lock policy.

The tray stays available even with TrayIcon=0. While locked it exposes only
Unlock / Enter Maintenance Mode. Manage/Edit requests through bangs, skin commands,
tray commands, and direct entry points open the password dialog instead. They are
not queued: after unlocking, repeat the requested Manage/Edit action. This avoids
executing stale management commands after authorization. With custom tray actions,
Ctrl + right-click in Maintenance opens the normal menu containing Lock Now.

## Password storage and boundary

`Common/CafePassword.h` uses Windows CNG PBKDF2-HMAC-SHA256 with 600,000 iterations,
a fresh 32-byte BCryptGenRandom salt, and a 32-byte verifier. Password encoding is
UTF-16LE without the terminator, versioned explicitly in the record. Verification
uses a fixed-length XOR comparison without early exit. Password fields are masked,
limited to 256 UTF-16 code units, and cleared after every submission and on close;
local password buffers are explicitly wiped. Passwords are never logged, passed
on a command line, or serialized.

`CafeLock.ini` in Rainmeter's settings directory stores only Algorithm, Iterations,
Salt, and Verifier. It is separate from Rainmeter.ini so layout replacement cannot
erase credentials. Writes use a unique temporary file, flush, and atomic rename;
first setup cannot replace an existing record. Existing malformed/unreadable
records fail closed, rather than becoming first-run setup. Only an absent file
permits setup. A password change creates a new salt. Unlock state exists only in
process memory; no setting, bang, environment variable or startup flag unlocks it.

This local application lock is not an OS security boundary. A person able to modify
or delete the verifier file can reset it; arbitrary same-user code can also alter
the process or writable program files. Installer ACLs and deployment hardening are
explicitly deferred. First-use setup must be completed by the cafe operator before
customers use the machine. There is no recovery/backdoor password.

Microsoft API references:
- [BCryptDeriveKeyPBKDF2](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptderivekeypbkdf2)
- [BCryptGenRandom](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptgenrandom)

The former UAC executable/project, named-pipe authorization, and helper tests have
been removed. Rainmeter does not request elevation or change the launch token.

## Compatibility and validation

All Stage 3 guards remain. ShelfSuite files are unchanged. Its exact configurator
launcher path is denied while locked and allowed in Maintenance; ordinary app
launchers, hover, tabs, meters and Lua continue. OnKeyDown now gates only the
upstream selected-skin arrow-key movement block (the handler has no general skin
keyboard dispatch).

Windows CI builds x64, checks PE architecture and produces checksummed review
artifacts. Production-core tests cover setup, mismatch/empty rejection, correct and
wrong passwords, change requiring the current password, old/new verification,
fresh salts, relock/restart, ShelfSuite launch policy, malformed records, and no
plaintext configuration. Runtime tests operate native dialogs and actual Rainmeter
with both the runner and a restricted standard-user token: Manage/Edit prompts,
setup/mismatch/change/wrong/old/new, management restoration, launcher token,
configuration writes, Lock Now, restart, modifier dragging, groups, tabs, hover,
Lua, and config/log plaintext scanning.

End-session tests send query/cancel/confirmed messages, including logoff, shutdown
and critical flags. They do not actually reboot/sign out the runner. Actual Windows
sign-out/restart/shutdown and the complete ShelfSuite package still need hands-on
compatibility testing; no installer/deployment work is included here.
