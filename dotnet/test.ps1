$ErrorActionPreference = "Stop"

& (Join-Path $PSScriptRoot "build.ps1")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

ctest --test-dir (Join-Path $PSScriptRoot "native/build") -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

dotnet run --project (Join-Path $PSScriptRoot "tests/SlimUi.Tests/SlimUi.Tests.csproj") -c Release
exit $LASTEXITCODE
