<#
  G22 (Mariano, 2026-11-01, after the reboot): end ArmourySocketServer.exe and delete the scheduled task
  \ASUS\ArmourySocketServer that restarted it at logon (the Section-handle leak, about 25 GB of commit on 2026-11-01).
  Run from an ELEVATED PowerShell:
    powershell -NoProfile -ExecutionPolicy Bypass -File C:\Users\maria\Desktop\Proyectos\SimGolf\log\roadmap\LIVING-PC_memory_tools\asus_task_remove.ps1
  Appends what it did to log\roadmap\LIVING-PC_asus_uninstall.log. Touches nothing else (the other \ASUS\ tasks stay).
#>
$log = 'C:\Users\maria\Desktop\Proyectos\SimGolf\log\roadmap\LIVING-PC_asus_uninstall.log'
function L([string]$m) { $line = '{0} {1}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $m; Add-Content -LiteralPath $log -Value $line; Write-Output $line }
L '== G22: end ArmourySocketServer and delete its scheduled task'
$p = @(Get-Process -Name ArmourySocketServer -ErrorAction SilentlyContinue)
foreach ($x in $p) { L "process pid $($x.Id) handles $($x.HandleCount)"; Stop-Process -Id $x.Id -Force -ErrorAction Continue }
Start-Sleep -Seconds 2
L ("process left: " + @(Get-Process -Name ArmourySocketServer -ErrorAction SilentlyContinue).Count)
$t = Get-ScheduledTask -TaskPath '\ASUS\' -TaskName 'ArmourySocketServer' -ErrorAction SilentlyContinue
if ($t) { Unregister-ScheduledTask -TaskPath '\ASUS\' -TaskName 'ArmourySocketServer' -Confirm:$false; L 'task \ASUS\ArmourySocketServer deleted' }
else { L 'task \ASUS\ArmourySocketServer not found' }
L ('\ASUS\ tasks left: ' + ((Get-ScheduledTask -TaskPath '\ASUS\' -ErrorAction SilentlyContinue | ForEach-Object { $_.TaskName }) -join ', '))
