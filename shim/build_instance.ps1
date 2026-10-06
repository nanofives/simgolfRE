# Build the shim for one parallel C3 batch into its own install copy (CLAUDE.md, parallel C3):
#   powershell -NoProfile -File shim/build_instance.ps1 -Id c3e
# Links the core reimplementations plus the batches listed in log/c3/<Id>_batches.txt (created with just <Id>.cpp
# when missing), objects in shim\build_<Id>, deployed to instances\<Id>\winmm.dll. Never touches original\.
# (Calling build.bat through `cmd //c` from Git Bash splits its quoted Visual Studio path; PowerShell does not.)
param([Parameter(Mandatory = $true)][string]$Id)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$inst = Join-Path $root "instances\$Id"
if (-not (Test-Path (Join-Path $inst 'golf_clean.exe'))) { throw "no install copy at $inst (py -3.12 re/tools/instances.py create $Id)" }
$list = Join-Path $root "log\c3\${Id}_batches.txt"
if (-not (Test-Path $list)) { Set-Content -Path $list -Value "$Id.cpp" -Encoding ascii }
$env:SIMGOLF_BUILD_DIR = "build_$Id"
$env:SIMGOLF_DEPLOY_DIR = $inst
$env:SIMGOLF_RE_BATCHES = $list
& cmd /c "$root\shim\build.bat"
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }
