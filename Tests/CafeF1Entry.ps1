$ErrorActionPreference = 'Stop'
# Source-derived compilation tests the production entry guards themselves,
# without introducing a test message, authorization route or shipped seam.
$repo = Split-Path $PSScriptRoot -Parent
$work = Join-Path ([IO.Path]::GetTempPath()) ('CafeF1Entry-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
$service = [regex]::Matches([IO.File]::ReadAllText("$repo/Library/CafeLock.cpp"), '(?s)void CafeLock::OpenShelfSuiteF1Compatibility\(HWND owner\)\s*\{(?<body>.*?)\r?\n\tif \(!F1Authorized\(\)\) return;')
$handler = [regex]::Matches([IO.File]::ReadAllText("$repo/Library/DialogManage.cpp"), '(?s)case Id_CafeF1:.*?return TRUE;')
if ($service.Count -ne 1 -or $handler.Count -ne 1) { throw 'Production F1 boundary shape changed; review the test extraction' }
$body = $service[0].Groups['body'].Value
$case = $handler[0].Value
$snippet = "void CafeLock::OpenShelfSuiteF1Compatibility(HWND owner) { ++entries; $body (void)preview; }`nINT_PTR DispatchF1Settings(UINT id) { enum { Id_CafeF1 = 118 }; switch(id) { $case } return FALSE; }"
foreach ($scenario in @('service-mutation','handler-mutation','production')) {
 $source = $snippet
 if ($scenario -eq 'service-mutation') {
  $guard = 'if (!AllowsF1Compatibility() || f1Active) return;'
  if (-not $source.Contains($guard)) { throw 'Service guard changed; mutation must be reviewed' }
  $source = $source.Replace($guard,'')
 } elseif ($scenario -eq 'handler-mutation') {
  $guard = 'if (CafeLock::AllowsF1Compatibility()) '
  if (-not $source.Contains($guard)) { throw 'Handler guard changed; mutation must be reviewed' }
  $source = $source.Replace($guard,'')
 }
 [IO.File]::WriteAllText((Join-Path $work 'CafeF1EntrySnippet.inc'),$source)
 $exe = Join-Path $work ($scenario + '.exe')
 & cl /nologo /EHsc /W4 /WX /DNOMINMAX "/I$work" "$PSScriptRoot/CafeF1Entry.cpp" "/Fe:$exe" "/Fo:$work/$scenario.obj"
 if ($LASTEXITCODE -ne 0) { throw "F1 boundary compile failed: $scenario" }
 & $exe
 if ($scenario -eq 'production') {
  if ($LASTEXITCODE -ne 0) { throw 'Production F1 boundary regression failed' }
 } else {
  if ($LASTEXITCODE -eq 0) { throw "Mutation survived: $scenario" }
  Write-Output "PASS RED sensitivity: $scenario rejected by entry/inspection counters"
 }
}
