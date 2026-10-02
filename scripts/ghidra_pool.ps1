# PowerShell wrapper around ghidra_pool.sh — same surface, native shell.
# Bash is the authoritative implementation; this just dispatches.

param(
    [Parameter(Position = 0)] [string] $Action = 'status',
    [Parameter(Position = 1, ValueFromRemainingArguments = $true)] [string[]] $Rest
)

$bash = Join-Path $PSScriptRoot 'ghidra_pool.sh'
if (-not (Test-Path $bash)) { Write-Error "ghidra_pool.sh not found at $bash"; exit 1 }

# `bash` on PATH here is WSL (C:\WINDOWS\system32\bash.exe), which cannot open a
# Windows path: it stripped the backslashes ('C:\Users\...' arrived as
# 'C:Usersmaria...') and rejected the forward-slash form too, so every call died
# with "No such file or directory". Resolve Git Bash explicitly and hand it a
# forward-slash path.
$gitBash = @(
    'C:\Program Files\Git\bin\bash.exe',
    'C:\Program Files (x86)\Git\bin\bash.exe'
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $gitBash) { $gitBash = 'bash' }

$argv = @(($bash -replace '\\', '/'), $Action) + ($Rest | Where-Object { $_ })
& $gitBash @argv
exit $LASTEXITCODE
