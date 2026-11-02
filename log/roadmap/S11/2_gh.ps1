# S11.2: one headless Ghidra run on the master.  2_gh.ps1 -Mod <program> -Script <X.java> [-ReadOnly] -Args <...>
param([string]$Mod, [string]$Script, [switch]$ReadOnly, [string[]]$ScriptArgs = @(), [string]$ScriptPath = 'C:\Users\maria\Desktop\Proyectos\SimGolf\log\roadmap\S11\2_ghidra')
# one directory only: the .bat splits a `;`-joined path into two arguments
Set-Location C:\Users\maria\Desktop\Proyectos\SimGolf
$ah = 'C:\Users\maria\Desktop\Proyectos\TD5RE\ghidra\ghidra_12.0.3_PUBLIC\support\analyzeHeadless.bat'
$a = @('C:\Users\maria\Desktop\Proyectos\SimGolf', 'SimGolf', '-process', $Mod, '-noanalysis', '-scriptPath', $ScriptPath,
       '-postScript', $Script) + $ScriptArgs
if ($ReadOnly) { $a += '-readOnly' }
"== $(Get-Date -Format o) $Mod $Script $($ScriptArgs -join ' ') ro=$ReadOnly"
& $ah @a 2>&1 | Where-Object { $_ -notmatch '^INFO  (Loading|Using|HEADLESS|Searching|Opening|REPORT|Headless|Analyzing)' }
"rc=$LASTEXITCODE"
