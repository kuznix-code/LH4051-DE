<#
.SYNOPSIS
  LH4051-DE Updater for Windows (7, 8, 8.1, 10, 11; x64 and ARM64).
.DESCRIPTION
  Downloads the LH4051-DE Installer (.exe) for the chosen mode and runs it.
  Modes (default stable, or LH4051_UPDATE_MODE):
    stable        newest stable release
    beta          newest beta (GitHub prerelease)
    lts           newest release of the newest LTS line
    lts-0.1       newest release of one LTS line (0.1.x)
    lts-0.1.1     one exact release of an LTS line
    0.1.1         one exact release (supported releases only, see docs/supported-releases.txt)
    dev           CI build of the main branch:  <version>.YYYYMMDD.<commit>
    stable-dev    ... of the stable branch
    beta-dev      ... of the beta branch
    lts-dev       ... of the lts branch
    0.1-dev       ... of the branch that carries release line 0.1
  CI builds are GitHub Actions artifacts, which need a token: set GH_TOKEN (a personal access token).
  Needs PowerShell 3.0 or newer (Windows 7: install WMF 5.1).
.PARAMETER Mode   see above
.PARAMETER Check  only report whether an update is available
.PARAMETER Yes    do not ask for confirmation; run the installer silently
.PARAMETER Force  install even if the same or a newer version is installed
#>
param(
    [string]$Mode = $(if ($env:LH4051_UPDATE_MODE) { $env:LH4051_UPDATE_MODE } else { 'stable' }),
    [switch]$Check,
    [switch]$Yes,
    [switch]$Force
)
$ErrorActionPreference = 'Stop'
$Repo = 'kuznix-code/LH4051-DE'
$Api = if ($env:LH4051_API) { $env:LH4051_API } else { "https://api.github.com/repos/$Repo" }
$Raw = if ($env:LH4051_RAW) { $env:LH4051_RAW } else { "https://raw.githubusercontent.com/$Repo" }
$AppId = '{6E0C1B0A-4051-4B1D-9C42-4C48344F5330}_is1'   # Inno Setup uninstall key of LH4051-DE

# Windows 7/8/8.1 do not enable TLS 1.2 for .NET by default, and GitHub requires it.
try { [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor 3072 } catch { }

function Fail($msg) { throw $msg }

$arch = $env:PROCESSOR_ARCHITEW6432
if (-not $arch) { $arch = $env:PROCESSOR_ARCHITECTURE }
$Arch = if ($arch -eq 'ARM64') { 'arm64' } else { 'x64' }

$token = if ($env:GH_TOKEN) { $env:GH_TOKEN } else { $env:GITHUB_TOKEN }
function Get-Headers($withAuth) {
    $h = @{ 'User-Agent' = 'lh4051-de-updater'; 'Accept' = 'application/vnd.github+json' }
    if ($withAuth -and $token) { $h['Authorization'] = "Bearer $token" }
    return $h
}

function Get-Json($url) {
    try { return Invoke-RestMethod -Uri $url -Headers (Get-Headers $true) -UseBasicParsing }
    catch { Fail "could not reach GitHub ($url): $($_.Exception.Message)" }
}

function Get-Text($url) {
    try { return (Invoke-WebRequest -Uri $url -Headers (Get-Headers $false) -UseBasicParsing).Content }
    catch { return $null }
}

# ---- versions -------------------------------------------------------------------------------
# Numbers compare numerically; dev < alpha < beta < rc < (release) < more numbers; a non-numeric token
# such as a commit hash ends the comparison. Returns -1, 0 or 1.
function Compare-Version($a, $b) {
    $ta = ([string]$a).TrimStart('v') -split '[.-]'
    $tb = ([string]$b).TrimStart('v') -split '[.-]'
    $n = [Math]::Max($ta.Count, $tb.Count)
    function Tok($t) {
        if ($null -eq $t -or $t -eq '') { return [double]0 }
        switch ($t) { 'dev' { return [double]-4 } 'alpha' { return [double]-3 } 'beta' { return [double]-2 } 'rc' { return [double]-1 } }
        if ($t -match '^\d+$') { return [double]$t }
        return $null
    }
    for ($i = 0; $i -lt $n; $i++) {
        $x = Tok $(if ($i -lt $ta.Count) { $ta[$i] } else { $null })
        $y = Tok $(if ($i -lt $tb.Count) { $tb[$i] } else { $null })
        if ($null -eq $x -or $null -eq $y) { return 0 }
        if ($x -lt $y) { return -1 }
        if ($x -gt $y) { return 1 }
    }
    return 0
}

function Get-Line($v) {
    $m = [regex]::Match(([string]$v).TrimStart('v'), '^(\d+\.\d+)')
    if ($m.Success) { return $m.Groups[1].Value }
    return $null
}

# ---- what is installed ----------------------------------------------------------------------
# lh4051-de.exe is a windowed program: it prints nothing for --version when started from a console, so
# the version comes from the installer's uninstall entry (full version incl. date/commit for CI builds)
# or from the exe's version resource.
function Get-InstalledVersion {
    $keys = @(
        "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$AppId",
        "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\$AppId",
        "HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$AppId"
    )
    foreach ($k in $keys) {
        if (Test-Path $k) {
            $p = Get-ItemProperty $k
            if ($p.DisplayVersion) { return [string]$p.DisplayVersion }
        }
    }
    $dirs = @($env:ProgramFiles, ${env:ProgramFiles(x86)}, $env:ProgramW6432, (Join-Path $env:LOCALAPPDATA 'Programs')) | Where-Object { $_ }
    foreach ($d in $dirs) {
        $exe = Join-Path $d 'LH4051-DE\lh4051-de.exe'
        if (Test-Path $exe) {
            $v = (Get-Item $exe).VersionInfo.ProductVersion
            if ($v) { return [string]$v }
        }
    }
    $cmd = Get-Command 'lh4051-de.exe' -ErrorAction SilentlyContinue
    if ($cmd) {
        $v = (Get-Item $cmd.Source).VersionInfo.ProductVersion
        if ($v) { return [string]$v }
    }
    return $null
}

# ---- supported release lines ----------------------------------------------------------------
$lines = @()
function Load-Supported {
    $text = Get-Text "$Raw/main/docs/supported-releases.txt"
    if (-not $text) {
        Write-Warning 'could not read the supported-release list; using built-in defaults'
        $text = "0.2 dev main`n0.1 stable,lts stable"
    }
    $out = @()
    foreach ($l in ($text -split "`r?`n")) {
        $t = $l.Trim()
        if (-not $t -or $t.StartsWith('#')) { continue }
        $f = $t -split '\s+'
        if ($f.Count -ge 3) { $out += [pscustomobject]@{ Line = $f[0]; Channels = $f[1] -split ','; Branch = $f[2] } }
    }
    return $out
}
function Line-Has($line, $channel) { return [bool]($lines | Where-Object { $_.Line -eq $line -and $_.Channels -contains $channel }) }
function Lines-With($channel) { return @($lines | Where-Object { $_.Channels -contains $channel } | ForEach-Object { $_.Line }) }
function Branch-OfChannel($channel) { $r = $lines | Where-Object { $_.Channels -contains $channel } | Select-Object -First 1; if ($r) { return $r.Branch } return $null }
function Branch-OfLine($line) { $r = $lines | Where-Object { $_.Line -eq $line } | Select-Object -First 1; if ($r) { return $r.Branch } return $null }
function Line-Supported($line) { return (Line-Has $line 'stable') -or (Line-Has $line 'lts') }

# ---- mode -----------------------------------------------------------------------------------
function Test-VersionLike($s) { return $s -match '^\d+\.\d+(\.\d+)?(\.x)?(-[A-Za-z0-9.]+)?$' }
function Test-LineArg($s) { return $s -match '^\d+\.\d+(\.x)?$' }

$Kind = $null; $Channel = $null; $WantLine = $null; $WantExact = $null
switch -regex ($Mode) {
    '^(stable|beta|lts)$' { $Kind = 'release'; $Channel = $Mode; break }
    '^dev$' { $Kind = 'branch'; $Channel = 'dev'; break }
    '^(stable|beta|lts)-dev$' { $Kind = 'branch'; $Channel = $Mode -replace '-dev$', ''; break }
    '^lts-(.+)$' {
        $rest = $Matches[1]
        if (Test-LineArg $rest) { $Kind = 'release'; $Channel = 'lts'; $WantLine = Get-Line $rest }
        elseif (Test-VersionLike $rest) { $Kind = 'exact'; $Channel = 'lts'; $WantExact = $rest }
        else { Fail "unknown mode '$Mode'" }
        break
    }
    '^(.+)-dev$' {
        $rest = $Matches[1]
        if (-not (Test-LineArg $rest)) { Fail "unknown mode '$Mode'" }
        $Kind = 'branch'; $WantLine = Get-Line $rest
        break
    }
    default {
        if (-not (Test-VersionLike $Mode)) { Fail "unknown mode '$Mode' (stable, beta, lts, lts-0.1, 0.1.1, dev, stable-dev, beta-dev, lts-dev, 0.1-dev)" }
        $Kind = 'exact'; $WantExact = $Mode
    }
}

$lines = Load-Supported
$installed = Get-InstalledVersion
Write-Host "LH4051-DE Updater  (mode: $Mode, windows/$Arch)"
Write-Host ("Installed version: " + $(if ($installed) { $installed } else { 'none found' }))

$version = $null; $assetUrl = $null; $branch = $null; $isArtifact = $false

# ---- release lookup -------------------------------------------------------------------------
if ($Kind -ne 'branch') {
    $releases = @(Get-Json "$Api/releases?per_page=100" | Where-Object { -not $_.draft })
    $best = $null
    foreach ($r in $releases) {
        $line = Get-Line $r.tag_name
        if (-not $line) { continue }
        $ok = $false
        if ($Kind -eq 'exact') { $ok = ($r.tag_name.TrimStart('v') -eq $WantExact.TrimStart('v')) }
        elseif ($Channel -eq 'beta') { $ok = [bool]$r.prerelease }
        elseif ($Channel -eq 'stable') { $ok = (-not $r.prerelease) -and (Line-Has $line 'stable') }
        elseif ($Channel -eq 'lts') { $ok = (-not $r.prerelease) -and (Line-Has $line 'lts') -and ((-not $WantLine) -or $line -eq $WantLine) }
        if (-not $ok) { continue }
        if (-not $best -or (Compare-Version $r.tag_name $best.tag_name) -gt 0) { $best = $r }
    }
    if (-not $best) {
        if ($Kind -eq 'exact') { Fail "release $WantExact does not exist" }
        if ($Channel -eq 'lts') { Fail ("no LTS release found" + $(if ($WantLine) { " for line $WantLine" }) + "; LTS lines: " + ((Lines-With 'lts') -join ' ')) }
        Fail "no $Channel release has been published yet"
    }
    $line = Get-Line $best.tag_name
    if ($Kind -eq 'exact') {
        if ($best.prerelease) { if (-not ((Line-Has $line 'beta') -or (Line-Supported $line))) { Fail "release $($best.tag_name) is not supported (line $line)" } }
        elseif (-not (Line-Supported $line)) { Fail ("release $($best.tag_name) is not supported: line $line is not a stable or LTS line (supported lines: " + ((@(Lines-With 'stable') + @(Lines-With 'lts') | Select-Object -Unique) -join ' ') + ")") }
        if ($Channel -eq 'lts' -and -not (Line-Has $line 'lts')) { Fail "release $($best.tag_name) is not part of an LTS line" }
    }
    if ($Kind -eq 'release' -and $Channel -eq 'lts' -and $WantLine -and -not (Line-Has $WantLine 'lts')) {
        Fail "$WantLine is not an LTS line (LTS lines: $((Lines-With 'lts') -join ' '))"
    }
    $version = $best.tag_name.TrimStart('v')
    $asset = $best.assets | Where-Object { $_.name -match "^LH4051-DE-Installer-.*-win-$Arch\.exe$" } | Select-Object -First 1
    if (-not $asset) { Fail "release $($best.tag_name) has no Windows $Arch installer" }
    $assetUrl = $asset.browser_download_url
}
# ---- CI build lookup ------------------------------------------------------------------------
else {
    if ($WantLine) {
        $branch = Branch-OfLine $WantLine
        if (-not $branch) { Fail "release line $WantLine is not listed in supported-releases.txt" }
    } else {
        $branch = Branch-OfChannel $Channel
        if (-not $branch) { $branch = if ($Channel -eq 'dev') { 'main' } else { $Channel } }
    }
    if ($branch -notmatch '^[A-Za-z0-9._/+-]+$') { Fail "unsafe branch name: $branch" }
    if (-not $token) { Fail "CI builds ($Mode) are GitHub Actions artifacts and need a token: set GH_TOKEN to a GitHub personal access token. (Releases such as 'stable' need none.)" }
    $runs = Get-Json "$Api/actions/runs?branch=$branch&status=success&per_page=1"
    if (-not $runs.workflow_runs -or @($runs.workflow_runs).Count -eq 0) { Fail "branch '$branch' has no successful build yet" }
    $run = @($runs.workflow_runs)[0]
    $sha = $run.head_sha
    $base = $null
    $h = Get-Text "$Raw/$sha/src/longhorn.h"
    if ($h -and $h -match '#define LH4051_VERSION "([^"]+)"') { $base = $Matches[1] }
    $commit = Get-Json "$Api/commits/$sha"
    $day = ([string]$commit.commit.committer.date).Substring(0, 10).Replace('-', '')
    if (-not $base -or -not $day) { Fail "could not work out the version of build $sha on $branch" }
    $version = "$base.$day.$($sha.Substring(0, 7))"
    $arts = Get-Json "$Api/actions/runs/$($run.id)/artifacts"
    $art = $arts.artifacts | Where-Object { $_.name -eq "lh4051-de-windows-$Arch" -and -not $_.expired } | Select-Object -First 1
    if (-not $art) { Fail "build $($run.id) has no windows-$Arch artifact" }
    $assetUrl = $art.archive_download_url
    $isArtifact = $true
}
Write-Host ("Available ($Mode): $version" + $(if ($branch) { "  [branch $branch]" }))

# ---- compare --------------------------------------------------------------------------------
$downgrade = $false
if ($installed -and -not $Force) {
    $c = Compare-Version $version $installed
    if ($c -eq 0) { Write-Host 'LH4051-DE is up to date.'; return }
    if ($c -lt 0) {
        if ($Kind -eq 'exact') { $downgrade = $true }
        else { Write-Host "Installed version $installed is newer than $version ($Mode). Use -Force to switch anyway."; return }
    }
}
if ($Check) {
    if ($downgrade) { Write-Host "A different version is available: $installed -> $version" }
    else { Write-Host "An update is available: $installed -> $version" }
    return
}
if (-not $Yes) {
    $verb = if ($downgrade) { "Replace $installed with" } else { 'Install' }
    $answer = Read-Host "$verb LH4051-DE $version? [Y/n]"
    if ($answer -match '^(n|no)$') { Write-Host 'Cancelled.'; return }
}

# ---- download + run -------------------------------------------------------------------------
# The artifact URL answers with a redirect to blob storage. That second request must not carry the
# GitHub token (storage rejects it), so the redirect is followed by hand.
function Get-RedirectTarget($url) {
    $req = [Net.HttpWebRequest]::Create($url)
    $req.AllowAutoRedirect = $false
    $req.UserAgent = 'lh4051-de-updater'
    $req.Headers.Add('Authorization', "Bearer $token")
    $req.Accept = 'application/vnd.github+json'
    try { $resp = $req.GetResponse() } catch [Net.WebException] { $resp = $_.Exception.Response; if (-not $resp) { throw } }
    try {
        $loc = $resp.Headers['Location']
        if (-not $loc) { Fail "GitHub did not offer a download for this build (HTTP $([int]$resp.StatusCode)); is the token valid and allowed to read Actions?" }
        return $loc
    } finally { $resp.Close() }
}

$work = Join-Path ([IO.Path]::GetTempPath()) ('lh4051-update-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
try {
    $installer = $null
    if ($isArtifact) {
        $zip = Join-Path $work 'artifact.zip'
        Write-Host 'Downloading the build artifact'
        $target = Get-RedirectTarget $assetUrl
        Invoke-WebRequest -Uri $target -OutFile $zip -UseBasicParsing -Headers @{ 'User-Agent' = 'lh4051-de-updater' }
        $extract = Join-Path $work 'art'
        New-Item -ItemType Directory -Path $extract | Out-Null
        try {
            Add-Type -AssemblyName System.IO.Compression.FileSystem
            [IO.Compression.ZipFile]::ExtractToDirectory($zip, $extract)
        } catch {
            # .NET 4.5 is missing (Windows 7): use the shell unzipper
            $shell = New-Object -ComObject Shell.Application
            $shell.NameSpace($extract).CopyHere($shell.NameSpace($zip).Items(), 20)
        }
        $installer = Get-ChildItem $extract -Recurse -Filter 'LH4051-DE-Installer-*.exe' | Select-Object -First 1 -ExpandProperty FullName
    } else {
        $installer = Join-Path $work ([IO.Path]::GetFileName(([Uri]$assetUrl).AbsolutePath))
        Write-Host "Downloading $([IO.Path]::GetFileName($installer))"
        Invoke-WebRequest -Uri $assetUrl -OutFile $installer -UseBasicParsing -Headers @{ 'User-Agent' = 'lh4051-de-updater' }
    }
    if (-not $installer -or -not (Test-Path $installer)) { Fail 'no installer found in the download' }

    $installArgs = if ($Yes) { @('/SILENT', '/NORESTART') } else { @() }
    if ($installArgs.Count) { $p = Start-Process -FilePath $installer -ArgumentList $installArgs -Wait -PassThru }
    else { $p = Start-Process -FilePath $installer -Wait -PassThru }
    if ($p.ExitCode -ne 0) { Fail "the installer exited with code $($p.ExitCode)" }
    Write-Host "LH4051-DE $version installed."
} finally {
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}
