param([Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Path "$Directory/Shelf1", "$Directory/@Resources/Icons" | Out-Null
$lines = @('[Rainmeter]', '[MeasureEngine]', 'Measure=Script', 'ScriptFile=#@#ShelfEngine.lua')
foreach ($i in 1..5) { $lines += @("[MeterTab$($i)Bg]", 'Meter=Shape', "[MeterTab$($i)Text]", 'Meter=String') }
foreach ($i in 1..18) { $lines += @("[MeterIcon$i]", 'Meter=Image', "[MeterIcon$($i)Text]", 'Meter=String') }
[IO.File]::WriteAllLines("$Directory/Shelf1/Shelf.ini", $lines)
[IO.File]::WriteAllText("$Directory/Shelf1/config.lua", "ShelfConfig={defaultIcon='file.png',tabs={{name='Apps',items={{label='One',action='notepad.exe',icon='file.png'}}}}}")
Add-Type -AssemblyName System.Drawing
$bitmap = [Drawing.Bitmap]::new(2, 1, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
try {
    $bitmap.SetPixel(0,0,[Drawing.Color]::FromArgb(0,0,0,0))
    $bitmap.SetPixel(1,0,[Drawing.Color]::FromArgb(128,10,20,30))
    $bitmap.Save("$Directory/Transparent.png",[Drawing.Imaging.ImageFormat]::Png)
} finally { $bitmap.Dispose() }
