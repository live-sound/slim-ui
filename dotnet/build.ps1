$ErrorActionPreference = "Stop"

$root = $PSScriptRoot

& (Join-Path $root "native\build.ps1")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "[managed] build"
dotnet build (Join-Path $root "samples\SlimUi.Sample\SlimUi.Sample.csproj") -c Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "[managed] done"
