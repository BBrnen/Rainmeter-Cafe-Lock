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
    Add-Type -AssemblyName System.Drawing
    $bitmap = [Drawing.Bitmap]::new(2, 1, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $bitmap.SetPixel(0, 0, [Drawing.Color]::FromArgb(0, 0, 0, 0))
        $bitmap.SetPixel(1, 0, [Drawing.Color]::FromArgb(128, 10, 20, 30))
        $bitmap.Save((Join-Path $Directory 'Transparent.png'), [Drawing.Imaging.ImageFormat]::Png)
    } finally { $bitmap.Dispose() }
    $pngBytes = [IO.File]::ReadAllBytes((Join-Path $Directory 'Transparent.png'))
    $icoStream = [IO.MemoryStream]::new()
    $writer = [IO.BinaryWriter]::new($icoStream)
    try {
        $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]1)
        $writer.Write([byte]2); $writer.Write([byte]1); $writer.Write([byte]0); $writer.Write([byte]0)
        $writer.Write([uint16]1); $writer.Write([uint16]32)
        $writer.Write([uint32]$pngBytes.Length); $writer.Write([uint32]22)
        $writer.Write($pngBytes)
        [IO.File]::WriteAllBytes((Join-Path $Directory 'Transparent.ico'), $icoStream.ToArray())
    } finally { $writer.Dispose(); $icoStream.Dispose() }
    [IO.File]::WriteAllText((Join-Path $Directory 'Corrupt.png'), 'not an image')
    $oversized = [IO.File]::Create((Join-Path $Directory 'Oversized.png'))
    try { $oversized.SetLength(32MB + 1) } finally { $oversized.Dispose() }
    Add-Content -LiteralPath $version -Value '2 ICON "Transparent.ico"'
    & rc.exe /nologo /foFixture.res $version
    if ($LASTEXITCODE -ne 0) { throw 'Launcher fixture resource compilation failed' }
    & cl.exe /nologo /EHsc /W4 /WX /DNOMINMAX /utf-8 $source Fixture.res '/Fe:Versioned application.exe' /link /SUBSYSTEM:WINDOWS
    if ($LASTEXITCODE -ne 0) { throw 'Versioned fixture compilation failed' }
    & cl.exe /nologo /EHsc /W4 /WX /DNOMINMAX /utf-8 $source '/Fe:Without version.exe' /link /SUBSYSTEM:WINDOWS
    if ($LASTEXITCODE -ne 0) { throw 'Unversioned fixture compilation failed' }
    $shell = New-Object -ComObject WScript.Shell
    foreach ($name in @('Original shortcut.lnk', 'Unavailable target.lnk', 'Target icon.lnk')) {
        $shortcut = $shell.CreateShortcut((Join-Path $Directory $name))
        $shortcut.TargetPath = if ($name -ne 'Unavailable target.lnk') { Join-Path $Directory 'Versioned application.exe' } else { Join-Path $Directory 'Missing.exe' }
        $shortcut.Arguments = '"argument with spaces" /fixture'
        $shortcut.WorkingDirectory = Join-Path $Directory 'Folder with spaces'
        if ($name -ne 'Target icon.lnk') { $shortcut.IconLocation = (Join-Path $env:WINDIR 'System32/shell32.dll') + ',3' }
        $shortcut.Save()
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shortcut)
    }
    [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell)
} finally { Pop-Location }
