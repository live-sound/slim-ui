$ErrorActionPreference = "Stop"

$src = $PSScriptRoot
$build = Join-Path $src "build"

New-Item -ItemType Directory -Force -Path $build | Out-Null

Write-Host "[native] configure"
cmake -S $src -B $build -G "Visual Studio 17 2022" -A x64 -Wno-dev
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "[native] build"
cmake --build $build --config Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$dll = Join-Path $build "Release\SlimUi.Native.dll"
if (!(Test-Path $dll)) {
    throw "Native build did not produce $dll"
}

Write-Host "[native] done: $dll"
