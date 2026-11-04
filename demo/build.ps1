$ErrorActionPreference = "Stop"
$src   = $PSScriptRoot
$build = "$src\build"

New-Item -ItemType Directory -Force -Path $build | Out-Null

Write-Host "Configuring..."
cmake -S $src -B $build -G "Visual Studio 17 2022" -A x64 -Wno-dev

Write-Host "Building..."
cmake --build $build --config Release

$exe = "$build\Release\sui_demo.exe"
if (Test-Path $exe) {
    Write-Host "`nDone: $exe"
    Write-Host "Run: & '$exe'"
} else {
    Write-Host "`nBuild failed -- check output above." -ForegroundColor Red
    exit 1
}
