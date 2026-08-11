param([switch]$Clean)

$ErrorActionPreference = "Stop"
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Config = Get-Content -LiteralPath (Join-Path $ProjectRoot "release.config.json") -Encoding UTF8 -Raw | ConvertFrom-Json
$VsDevCmd = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
$CMake = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$CTest = Join-Path (Split-Path -Parent $CMake) "ctest.exe"
if (-not (Test-Path -LiteralPath $VsDevCmd) -or -not (Test-Path -LiteralPath $CMake) -or -not (Test-Path -LiteralPath $CTest)) {
    throw "Visual Studio 2026 CMake toolchain was not found."
}

$BuildRoot = Join-Path $ProjectRoot "build"
$ArtifactDirectory = Join-Path $ProjectRoot $Config.artifactsDirectory
$NativeExe = Join-Path $BuildRoot ("Release\" + $Config.publishedExeName)
if ($Clean -and (Test-Path -LiteralPath $BuildRoot)) {
    Remove-Item -LiteralPath $BuildRoot -Recurse -Force
}

$Command = 'call "{0}" -arch=x64 -host_arch=x64 && "{1}" -S "{2}" -B "{3}" -G "Visual Studio 18 2026" -A x64 -DBUILD_TESTING=ON && "{1}" --build "{3}" --config Release --parallel && "{4}" --test-dir "{3}" -C Release --output-on-failure' -f $VsDevCmd, $CMake, $ProjectRoot, $BuildRoot, $CTest
cmd.exe /c $Command
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not (Test-Path -LiteralPath $NativeExe -PathType Leaf)) {
    throw "Release executable was not produced: $NativeExe"
}
$Verification = Start-Process -FilePath $NativeExe -ArgumentList "--verify-release" -PassThru -Wait -WindowStyle Hidden
if ($Verification.ExitCode -ne 0) {
    throw "Release verification failed with exit code $($Verification.ExitCode)."
}

if (Test-Path -LiteralPath $ArtifactDirectory) { Remove-Item -LiteralPath $ArtifactDirectory -Recurse -Force }
New-Item -ItemType Directory -Force -Path $ArtifactDirectory | Out-Null
Copy-Item -LiteralPath $NativeExe -Destination (Join-Path $ArtifactDirectory $Config.publishedExeName)
Write-Host "Native Release package: $(Join-Path $ArtifactDirectory $Config.publishedExeName)"
