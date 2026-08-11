param(
    [string]$Version = "",
    [string]$ReleaseNotes = ""
)

$ErrorActionPreference = "Stop"
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$SharedScript = Join-Path $ProjectRoot "third_party\DesktopUpdateKit\tools\Prepare-ReleaseAssets.ps1"
if (-not (Test-Path -LiteralPath $SharedScript)) {
    throw "DesktopUpdateKit submodule is missing. Run: git submodule update --init --recursive"
}
& $SharedScript -ProjectRoot $ProjectRoot -ConfigPath (Join-Path $ProjectRoot "release.config.json") -Version $Version -ReleaseNotes $ReleaseNotes
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
