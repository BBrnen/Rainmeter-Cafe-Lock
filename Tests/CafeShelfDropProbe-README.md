# Cafe Shelf manual drag/drop diagnostic

This is a test tool, not a Rainmeter installer or the finished launcher editor.

1. Use a spare Windows PC or VM with a visible desktop.
2. Extract the entire downloaded ZIP to a new folder.
3. Double-click RunDropProbe.cmd normally. Do not choose Run as administrator.
4. Press a key when asked. File Explorer opens a folder containing four prepared test items.
5. Arrange File Explorer and the Cafe Shelf drop test window side by side.
6. Drag one item at a time into the test window, following its displayed instruction:
   - Shortcut (Shortcut.lnk; Windows may hide the extension)
   - Application (Application.exe)
   - Folder with spaces
   - Document (Document.txt)
7. Wait for the next instruction after each drop. Do not double-click or run the items.
8. After the four drops, the test closes and the console shows the result. Send back the PASS/FAIL text. If it fails, keep the two log files in the temporary folder printed by the script.

This manual version does not move your mouse or create a simulated drag source. You have up to ten minutes; closing the test window cancels it. The browser displays drag progress and native code verifies the exact original path of each dropped fixture. A browser message or synthetic JavaScript File cannot pass that verification. No drop result is inferred from merely displaying the window.

The tool creates a shortcut, copied Windows application, folder and text document under a unique temporary directory. It opens only that fixture folder in File Explorer. It does not launch the fixtures, edit your skins, unlock Rainmeter, change passwords or install software.

The tool needs the Microsoft WebView2 Runtime. If unavailable, the official download is https://developer.microsoft.com/microsoft-edge/webview2/ . Opening ordinary Edge is not proof that the separate Runtime is installed.

CI builds this manual kit with -BuildOnly and verifies its startup/exit-code plumbing; that step never claims real drops passed. Required CafeShelfTests -Suite All separately loads the actual embedded host and tests its browser security and revocation. The owner passed all four real Explorer drops on a standard-user desktop with kit 0f615fe; see the execution ledger. The earlier simulated-drag mode remains available by running the probe script without -BuildOnly on a disposable CI desktop, and still fails rather than suppressing controller/drop errors. That simulation is not a reliable substitute for Explorer interaction.

Build details, source revision, executable SHA-256 and SDK verification are included. The Microsoft WebView2 SDK static loader is used; see https://www.nuget.org/packages/Microsoft.Web.WebView2/1.0.4258.31 and the included SDK notices. Source uses the repository's Rainmeter license.
