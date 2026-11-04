$ErrorActionPreference = "Stop"

$root = $PSScriptRoot

& (Join-Path $root "native\build.ps1")

Write-Host "[managed] build"
dotnet build (Join-Path $root "samples\SlimUi.Sample\SlimUi.Sample.csproj") -c Release

Write-Host "[managed] done"
