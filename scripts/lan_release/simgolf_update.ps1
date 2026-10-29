<#
.SYNOPSIS
    SimGolf port: LAN auto-updater (copy of SimCity's sc3_update.ps1 / TD5RE's td5re_update.ps1, adapted).

.DESCRIPTION
    Downloads manifest.json from the mini-server, compares the SHA-256 of every file already in this folder and
    downloads ONLY what is missing or changed. Safe to run as often as you like. Close the game first: a running
    game locks its exe and DLLs.

    Saves and recordings are not in this folder (they live in %LOCALAPPDATA%\SimGolfPort) and are never touched.
    -Prune also deletes files under game\ that the server no longer ships.

.USAGE
    Double-click update.bat, or:
        powershell -ExecutionPolicy Bypass -File simgolf_update.ps1 [-BaseUrl http://192.168.68.112:8088/simgolf] [-Prune]
#>
[CmdletBinding()]
param(
    [string[]]$BaseUrl,
    [string]$InstallRoot,
    [switch]$Prune
)

$ErrorActionPreference = 'Stop'
$ProgressPreference    = 'SilentlyContinue'

# The install folder is the folder that holds this script, never the current directory (2026-10-29: run from another
# folder, $PSScriptRoot came back empty in the param default and the old cwd fallback updated the wrong folder).
if (-not $InstallRoot) { $InstallRoot = $PSScriptRoot }
if (-not $InstallRoot -and $MyInvocation.MyCommand.Path) { $InstallRoot = Split-Path -Parent $MyInvocation.MyCommand.Path }
if (-not $InstallRoot) {
    Write-Host "ERROR: cannot tell which folder to update. Run update.bat, or pass -InstallRoot <folder>." -ForegroundColor Red
    exit 1
}
$InstallRoot = (Resolve-Path -LiteralPath $InstallRoot).Path.TrimEnd('\')
if (Test-Path -LiteralPath (Join-Path $InstallRoot '.git')) {
    Write-Host "ERROR: $InstallRoot is a git checkout, not a SimGolf folder. Put update.bat in its own folder." -ForegroundColor Red
    exit 1
}
if (-not $BaseUrl -or $BaseUrl.Count -eq 0) {
    # the .local name on a flat LAN; the server's static IP when mDNS cannot cross a second router
    $BaseUrl = @('http://mariano-server.local:8088/simgolf', 'http://192.168.68.112:8088/simgolf')
}
$BaseUrl = @($BaseUrl | ForEach-Object { $_.TrimEnd('/') })

function Url-For([string]$rel) {
    $enc = ($rel -split '/' | ForEach-Object { [uri]::EscapeDataString($_) }) -join '/'
    return "$script:ResolvedBase/$enc"
}

# .NET SHA-256: some Windows PowerShell 5.1 installs lack Get-FileHash (seen on the TD5RE machines)
function Get-Sha256Hex([string]$full) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $fs = [System.IO.File]::OpenRead($full)
        try { $bytes = $sha.ComputeHash($fs) } finally { $fs.Dispose() }
    } finally { $sha.Dispose() }
    -join ($bytes | ForEach-Object { $_.ToString('x2') })
}

Write-Host ""
Write-Host "SimGolf port updater" -ForegroundColor Cyan
Write-Host ("  servers: {0}" -f ($BaseUrl -join ', '))
Write-Host "  target : $InstallRoot"

if (Get-Process -Name 'simgolf_port' -ErrorAction SilentlyContinue) {
    Write-Host "ERROR: SimGolf (simgolf_port.exe) is running. Close it and run the update again." -ForegroundColor Red
    exit 1
}

# --- 1. manifest -------------------------------------------------------------------------------------------------
$script:ResolvedBase = $null
$manifest = $null
$tmpManifest = Join-Path ([System.IO.Path]::GetTempPath()) ("simgolf_manifest_" + [guid]::NewGuid().ToString("N") + ".json")
foreach ($base in $BaseUrl) {
    try {
        Invoke-WebRequest -Uri "$base/manifest.json" -OutFile $tmpManifest -UseBasicParsing -TimeoutSec 30
        $candidate = (Get-Content -LiteralPath $tmpManifest -Raw -Encoding UTF8) | ConvertFrom-Json
        if (@($candidate.files).Count -ge 1 -and $candidate.files[0].path) {
            $script:ResolvedBase = $base
            $manifest = $candidate
            Write-Host ("  server : {0}" -f $base) -ForegroundColor Green
            break
        }
        Write-Host ("  ! {0}/manifest.json is not a valid manifest, trying next ..." -f $base) -ForegroundColor DarkYellow
    } catch {
        Write-Host ("  ! {0} unreachable ({1}), trying next ..." -f $base, $_.Exception.Message) -ForegroundColor DarkYellow
    }
}
if (Test-Path -LiteralPath $tmpManifest) { Remove-Item -LiteralPath $tmpManifest -Force -ErrorAction SilentlyContinue }
if (-not $manifest) {
    Write-Host "ERROR: no server answered with a manifest. Tried:" -ForegroundColor Red
    foreach ($base in $BaseUrl) { Write-Host "       $base/manifest.json" -ForegroundColor Red }
    exit 1
}
$files = @($manifest.files)
Write-Host ("  build  : {0} ({1} files, generated {2})" -f $manifest.label, $files.Count, $manifest.generated)

# --- 2. what to download ------------------------------------------------------------------------------------------
$toGet = New-Object System.Collections.Generic.List[object]
$upToDate = 0
foreach ($f in $files) {
    $local = Join-Path $InstallRoot ($f.path -replace '/', '\')
    $need = $true
    if (Test-Path -LiteralPath $local) {
        if ($f.seed) { $need = $false }
        elseif ((Get-Item -LiteralPath $local).Length -eq [int64]$f.size -and (Get-Sha256Hex $local) -ieq $f.sha256) { $need = $false }
    }
    if ($need) { $toGet.Add($f) } else { $upToDate++ }
}
$mb = ($toGet | Measure-Object -Property size -Sum).Sum / 1MB
Write-Host ("  up to date: {0}   to download: {1} ({2:N1} MB)" -f $upToDate, $toGet.Count, $mb) -ForegroundColor Green

# --- 3. download ------------------------------------------------------------------------------------------------
$done = 0; $failed = 0; $i = 0
foreach ($f in $toGet) {
    $i++
    $local = Join-Path $InstallRoot ($f.path -replace '/', '\')
    $dir = Split-Path -Parent $local
    if ($dir -and -not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    $tmp = "$local.dl_tmp"
    Write-Host ("  [{0}/{1}] {2}" -f $i, $toGet.Count, $f.path)
    try {
        Invoke-WebRequest -Uri (Url-For $f.path) -OutFile $tmp -UseBasicParsing -TimeoutSec 600
        if ((Get-Sha256Hex $tmp) -ine $f.sha256) {
            Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue
            Write-Host "      ! hash mismatch after download, skipped" -ForegroundColor Red
            $failed++
            continue
        }
        Move-Item -LiteralPath $tmp -Destination $local -Force
        $done++
    } catch {
        Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue
        Write-Host "      ! download failed: $($_.Exception.Message)" -ForegroundColor Red
        $failed++
    }
}

# --- 4. optional prune: only under game\, never anything else ----------------------------------------------------
$pruned = 0
if ($Prune) {
    $keep = New-Object System.Collections.Generic.HashSet[string]
    foreach ($f in $files) { [void]$keep.Add((Join-Path $InstallRoot ($f.path -replace '/', '\')).ToLower()) }
    $gameDir = Join-Path $InstallRoot 'game'
    if (Test-Path -LiteralPath $gameDir) {
        Get-ChildItem -LiteralPath $gameDir -Recurse -File | ForEach-Object {
            if (-not $keep.Contains($_.FullName.ToLower())) {
                Remove-Item -LiteralPath $_.FullName -Force
                Write-Host ("  - pruned {0}" -f $_.FullName.Substring($InstallRoot.Length + 1)) -ForegroundColor DarkYellow
                $pruned++
            }
        }
    }
}

# --- 5. the launcher: a shortcut to THIS PC's simgolf_port.exe on game\ --------------------------------------------
# A .lnk stores absolute paths, so it is made here rather than shipped. "Play SimGolf.bat" does the same with
# relative paths.
$exe = Join-Path $InstallRoot 'simgolf_port.exe'
$lnkPath = Join-Path $InstallRoot 'SimGolf (port).lnk'
if (Test-Path -LiteralPath $exe) {
    try {
        $shell = New-Object -ComObject WScript.Shell
        $lnk = $shell.CreateShortcut($lnkPath)
        $lnk.TargetPath = $exe
        $lnk.Arguments = ('--install "{0}"' -f (Join-Path $InstallRoot 'game'))
        $lnk.WorkingDirectory = $InstallRoot
        $lnk.IconLocation = "$exe,0"
        $lnk.Description = 'SimGolf (port) on this folder''s game files'
        $lnk.Save()
        Write-Host "  + shortcut 'SimGolf (port)' -> simgolf_port.exe --install game" -ForegroundColor Green
    } catch {
        Write-Host "      ! could not create the shortcut: $($_.Exception.Message)" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host ("Done. updated {0}, up to date {1}, failed {2}, pruned {3}." -f $done, $upToDate, $failed, $pruned) -ForegroundColor Cyan
Write-Host "Start the game with the 'SimGolf (port)' shortcut or 'Play SimGolf.bat' in this folder."
if ($failed -gt 0) { exit 1 }
exit 0
