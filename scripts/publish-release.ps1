param(
    [Parameter(Mandatory = $true)]
    [string]$Version,
    [switch]$Finalize
)

$ErrorActionPreference = "Stop"
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Config = Get-Content -LiteralPath (Join-Path $ProjectRoot "release.config.json") -Encoding UTF8 -Raw | ConvertFrom-Json
$SharedScript = Join-Path $ProjectRoot "third_party\DesktopUpdateKit\tools\Publish-Release.ps1"
if (-not (Test-Path -LiteralPath $SharedScript)) {
    throw "DesktopUpdateKit submodule is missing. Run: git submodule update --init --recursive"
}
& $SharedScript -ProjectRoot $ProjectRoot -ConfigPath (Join-Path $ProjectRoot "release.config.json") -Version $Version -Finalize:$Finalize
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Finalize) {
    $Version = $Version.TrimStart('v', 'V')
    $Latest = gh api "repos/$($Config.repository)/releases/latest" | ConvertFrom-Json
    if ([string]$Latest.tag_name -ne "v$Version") { throw "GitHub latest release does not point to v$Version." }
    $Response = Invoke-WebRequest -UseBasicParsing -Uri "https://github.com/$($Config.repository)/releases/latest/download/update.json" -Headers @{ "Cache-Control" = "no-cache" }
    $Content = if ($Response.Content -is [byte[]]) {
        [Text.Encoding]::UTF8.GetString([byte[]]$Response.Content).TrimStart([char]0xFEFF)
    } else {
        [string]$Response.Content
    }
    $Manifest = $Content | ConvertFrom-Json
    if ([string]$Manifest.version -ne $Version -or [string]$Manifest.asset -ne [string]$Config.releaseAssetName) {
        throw "Public update.json does not match the finalized release."
    }
    Write-Host "Public latest update manifest verified."
}
