# Cafe Shelf drag/drop diagnostic

This is a test tool, not a Rainmeter installer or the finished launcher editor.

1. Use a spare Windows PC or VM with a visible desktop.
2. Extract the entire downloaded ZIP to a folder.
3. Double-click RunDropProbe.cmd normally. Do not choose Run as administrator.
4. Press a key when asked, then leave the mouse alone while the test runs (up to two minutes).
5. Send back the PASS/FAIL text. If it fails, keep the two log files in the temporary folder printed by the script.

The tool creates its own shortcut, copied Windows application, folder and text document under a unique temporary directory. It automatically drags them into a test browser window and checks the original file paths. It also checks that a fabricated browser File cannot supply a native path. It does not launch the fixtures, edit your skins, unlock Rainmeter, change passwords or install software.

The tool needs the Microsoft WebView2 Runtime. If the output says it is unavailable, the official download is https://developer.microsoft.com/microsoft-edge/webview2/ . Opening ordinary Edge is not proof that the separate Runtime is installed.

Build details and SDK verification are included. The tool uses the Microsoft WebView2 SDK static loader; see https://www.nuget.org/packages/Microsoft.Web.WebView2/1.0.4258.31 and the included SDK notices. The test source is included under the repository's Rainmeter license.

This revision explicitly shows the browser test window even when the console launcher starts hidden. CI checks that behavior and verifies both successful and failed exit codes under built-in Windows PowerShell 5.1. The safeguard against sending input to another window remains enabled. If it still stops, the log now includes window visibility, activation and hit-test diagnostics; do not repeatedly rearrange windows or rerun the same failed kit.
