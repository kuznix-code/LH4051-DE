<#
.SYNOPSIS
  LH4051-DE Updater for Windows (7, 8, 8.1, 10, 11; x64 and ARM64).
.DESCRIPTION
  Downloads the LH4051-DE Installer (.exe) for the chosen mode and runs it.
    stable  newest non-prerelease GitHub release (default)
    beta    newest GitHub prerelease
    dev     installer from the latest successful GitHub Actions run on main
            (artifacts need a token: set GH_TOKEN or GITHUB_TOKEN)
  Needs PowerShell 3.0 or newer (Windows 7: install WMF 5.1).
.PARAMETER Mode   stable | beta | dev   (default: stable, or LH4051_UPDATE_MODE)
.PARAMETER Check  only report whether an update is available
.PARAMETER Yes    do not ask for confirmation; run the installer silently
#>
param(
    [ValidateSet('stable', 'beta', 'dev')][string]$Mode = $(if ($env:LH4051_UPDATE_MODE) { $env:LH4051_UPDATE_MODE } else { 'stable' }),
    [switch]$Check,
    [switch]$Yes
)
$ErrorActionPreference = 'Stop'
$Repo = 'kuznix-code/LH4051-DE'
$Api = if ($env:LH4051_API) { $env:LH4051_API } else { "https://api.github.com/repos/$Repo" }

# Windows 7/8/8.1 do not enable TLS 1.2 for .NET by default, and GitHub requires it.
try { [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor 3072 } catch { }

function Get-Arch {
    $a = $env:PROCESSOR_ARCHITEW6432
    if (-not $a) { $a = $env:PROCESSOR_ARCHITECTURE }
    if ($a -eq 'ARM64') { return 'arm64' }
    return 'x64'
}
$Arch = Get-Arch

$token = if ($env:GH_TOKEN) { $env:GH_TOKEN } else { $env:GITHUB_TOKEN }
$headers = @{ 'User-Agent' = 'lh4051-de-updater'; 'Accept' = 'application/vnd.github+json' }
if ($token) { $headers['Authorization'] = "Bearer $token" }

function Get-Json($url) {
    try { return Invoke-RestMethod -Uri $url -Headers $headers -UseBasicParsing }
    catch { throw "could not reach GitHub ($url): $($_.Exception.Message)" }
}

function Get-InstalledVersion {
    $candidates = @(
        (Join-Path $env:ProgramFiles 'LH4051-DE\lh4051-de.exe'),
        (Join-Path $env:LOCALAPPDATA 'Programs\LH4051-DE\lh4051-de.exe')
    )
    foreach ($exe in $candidates) {
        if (Test-Path $exe) {
            $out = & $exe --version 2>$null
            if ($out -match 'LH4051-DE\s+(\S+)') { return $Matches[1] }
        }
    }
    return $null
}

# Numeric parts decide; a suffix such as ".dev" or "-beta.1" sorts before the plain release.
function Test-Newer($candidate, $installed) {
    if ($candidate -eq $installed) { return $false }
    function Parse($v) {
        $m = [regex]::Match($v, '^(\d+)\.(\d+)\.(\d+)(.*)$')
        if (-not $m.Success) { return @(0, 0, 0, 1) }
        $pre = if ($m.Groups[4].Value -match '^[.-](dev|alpha|beta|rc)') { 0 } else { 1 }
        return @([int]$m.Groups[1].Value, [int]$m.Groups[2].Value, [int]$m.Groups[3].Value, $pre)
    }
    $a = Parse $candidate; $b = Parse $installed
    for ($i = 0; $i -lt 4; $i++) {
        if ($a[$i] -gt $b[$i]) { return $true }
        if ($a[$i] -lt $b[$i]) { return $false }
    }
    return $false
}

$installed = Get-InstalledVersion
Write-Host "LH4051-DE Updater  (mode: $Mode, windows/$Arch)"
Write-Host ("Installed version: " + $(if ($installed) { $installed } else { 'none found' }))

$assetPattern = "^LH4051-DE-Installer-.*-win-$Arch\.exe$"
$version = $null; $assetUrl = $null

if ($Mode -eq 'dev') {
    if (-not $token) { Write-Warning 'dev mode downloads GitHub Actions artifacts, which require a token (GH_TOKEN).' }
    $runs = Get-Json "$Api/actions/runs?branch=main&status=success&per_page=1"
    if (-not $runs.workflow_runs -or $runs.workflow_runs.Count -eq 0) { throw 'no successful Actions run found on main' }
    $run = $runs.workflow_runs[0]
    $version = 'dev-' + $run.head_sha.Substring(0, 7)
    $arts = Get-Json "$Api/actions/runs/$($run.id)/artifacts"
    $art = $arts.artifacts | Where-Object { $_.name -eq "lh4051-de-windows-$Arch" -and -not $_.expired } | Select-Object -First 1
    if (-not $art) { throw "run $($run.id) has no windows-$Arch artifact" }
    $assetUrl = $art.archive_download_url
} else {
    if ($Mode -eq 'stable') { $rel = Get-Json "$Api/releases/latest" }
    else {
        $rel = Get-Json "$Api/releases?per_page=30" | Where-Object { $_.prerelease -and -not $_.draft } | Select-Object -First 1
        if (-not $rel) { throw 'no beta release has been published yet' }
    }
    $version = $rel.tag_name.TrimStart('v')
    $asset = $rel.assets | Where-Object { $_.name -match $assetPattern } | Select-Object -First 1
    if (-not $asset) { throw "release $($rel.tag_name) has no Windows $Arch installer" }
    $assetUrl = $asset.browser_download_url
}
Write-Host "Available ($Mode): $version"

if ($Mode -ne 'dev' -and $installed -and -not (Test-Newer $version $installed)) {
    Write-Host 'LH4051-DE is up to date.'
    return
}
if ($Check) { Write-Host "An update is available: $installed -> $version"; return }

if (-not $Yes) {
    $answer = Read-Host "Install LH4051-DE $version? [Y/n]"
    if ($answer -match '^(n|no)$') { Write-Host 'Cancelled.'; return }
}

$work = Join-Path ([IO.Path]::GetTempPath()) ('lh4051-update-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
try {
    $installer = $null
    if ($Mode -eq 'dev') {
        $zip = Join-Path $work 'artifact.zip'
        Write-Host 'Downloading the latest build artifact'
        Invoke-WebRequest -Uri $assetUrl -Headers $headers -OutFile $zip -UseBasicParsing
        # System.IO.Compression exists from .NET 4.5; Windows 7 may only have the shell unzipper.
        $extract = Join-Path $work 'art'
        New-Item -ItemType Directory -Path $extract | Out-Null
        try {
            Add-Type -AssemblyName System.IO.Compression.FileSystem
            [IO.Compression.ZipFile]::ExtractToDirectory($zip, $extract)
        } catch {
            $shell = New-Object -ComObject Shell.Application
            $shell.NameSpace($extract).CopyHere($shell.NameSpace($zip).Items(), 20)
        }
        $installer = Get-ChildItem $extract -Recurse -Filter 'LH4051-DE-Installer-*.exe' | Select-Object -First 1 -ExpandProperty FullName
    } else {
        $installer = Join-Path $work ([IO.Path]::GetFileName(([Uri]$assetUrl).AbsolutePath))
        Write-Host "Downloading $([IO.Path]::GetFileName($installer))"
        Invoke-WebRequest -Uri $assetUrl -Headers $headers -OutFile $installer -UseBasicParsing
    }
    if (-not $installer -or -not (Test-Path $installer)) { throw 'no installer found in the download' }

    $args = if ($Yes) { @('/SILENT', '/NORESTART') } else { @() }
    $p = Start-Process -FilePath $installer -ArgumentList $args -Wait -PassThru
    if ($p.ExitCode -ne 0) { throw "the installer exited with code $($p.ExitCode)" }
    Write-Host "LH4051-DE $version installed."
} finally {
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}
