param([Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Path $Directory | Out-Null
New-Item -ItemType Directory -Path (Join-Path $Directory 'Folder with spaces') | Out-Null
foreach ($name in @('Report.txt', 'Café 日本.txt', 'Bracket[1].txt', 'Hash#name.txt', 'Percent%PATH%.txt')) {
    [IO.File]::WriteAllText((Join-Path $Directory $name), 'Metadata fixture; never launched.')
}
$source = Join-Path $Directory 'NeverRun.cpp'
[IO.File]::WriteAllText($source, @'
#include <windows.h>
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    HANDLE marker = CreateFileW(L"launcher-was-executed.txt", GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (marker != INVALID_HANDLE_VALUE) CloseHandle(marker);
    return 9;
}
'@)
$version = Join-Path $Directory 'Fixture.rc'
[IO.File]::WriteAllText($version, @'
#include <windows.h>
1 VERSIONINFO
FILEVERSION 1,0,0,0
PRODUCTVERSION 1,0,0,0
FILEOS VOS_NT_WINDOWS32
FILETYPE VFT_APP
BEGIN
 BLOCK "StringFileInfo"
 BEGIN
  BLOCK "040904b0"
  BEGIN
   VALUE "FileDescription", "Cafe Shelf fixture application\0"
  END
 END
 BLOCK "VarFileInfo"
 BEGIN
  VALUE "Translation", 0x0409, 1200
 END
END
'@)
Push-Location $Directory
try {
    & rc.exe /nologo /foFixture.res $version
    if ($LASTEXITCODE -ne 0) { throw 'Launcher fixture resource compilation failed' }
    & cl.exe /nologo /EHsc /W4 /WX /DNOMINMAX /utf-8 $source Fixture.res '/Fe:Versioned application.exe' /link /SUBSYSTEM:WINDOWS
    if ($LASTEXITCODE -ne 0) { throw 'Versioned fixture compilation failed' }
    & cl.exe /nologo /EHsc /W4 /WX /DNOMINMAX /utf-8 $source '/Fe:Without version.exe' /link /SUBSYSTEM:WINDOWS
    if ($LASTEXITCODE -ne 0) { throw 'Unversioned fixture compilation failed' }
    $shell = New-Object -ComObject WScript.Shell
    foreach ($name in @('Original shortcut.lnk', 'Unavailable target.lnk')) {
        $shortcut = $shell.CreateShortcut((Join-Path $Directory $name))
        $shortcut.TargetPath = if ($name -eq 'Original shortcut.lnk') { Join-Path $Directory 'Versioned application.exe' } else { Join-Path $Directory 'Missing.exe' }
        $shortcut.Arguments = '"argument with spaces" /fixture'
        $shortcut.WorkingDirectory = Join-Path $Directory 'Folder with spaces'
        $shortcut.IconLocation = (Join-Path $env:WINDIR 'System32/shell32.dll') + ',3'
        $shortcut.Save()
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shortcut)
    }
    [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell)
} finally { Pop-Location }
