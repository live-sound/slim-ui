# build-tests.ps1 — configure, build, and run the unit tests.
#
# Usage:
#   .\build-tests.ps1            # configure (if needed) + build + run
#   .\build-tests.ps1 -Configuration Debug # the same suite in Debug
#   .\build-tests.ps1 -NoRun     # build only; skip the test exes
#   .\build-tests.ps1 -Filter X  # pass --filter=X to utest.h
#   .\build-tests.ps1 -List      # pass --list-tests to utest.h
#
# The test binaries are left in tests/build/<Configuration>/ so they can be
# re-run by hand without reconfiguring.

[CmdletBinding()]
param(
    [ValidateSet('Release','Debug')][string]$Configuration = 'Release',
    [switch]$NoRun,
    [string]$Filter,
    [switch]$List
)

$ErrorActionPreference = "Stop"
$src   = $PSScriptRoot
$build = Join-Path $src "build"

New-Item -ItemType Directory -Force -Path $build | Out-Null

Write-Host "[configure] $src -> $build" -ForegroundColor Cyan
cmake -S $src -B $build -G "Visual Studio 17 2022" -A x64 -Wno-dev
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "[build]   $Configuration" -ForegroundColor Cyan
cmake --build $build --config $Configuration
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($NoRun) {
    Write-Host "[done] built in $build\$Configuration (not running)" -ForegroundColor Green
    exit 0
}

$exeArgs = @()
if ($List)   { $exeArgs += "--list-tests" }
if ($Filter) { $exeArgs += "--filter=$Filter" }

foreach ($t in @('sui_tests', 'sui_omit_tests', 'sui_vsync_tests', 'sui_popup_clock_tests')) {
    $exe = Join-Path $build "$Configuration\$t.exe"
    Write-Host "[run]     $exe $($exeArgs -join ' ')" -ForegroundColor Cyan
    & $exe @exeArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
