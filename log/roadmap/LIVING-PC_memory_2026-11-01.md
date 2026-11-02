# LIVING-PC commit charge, 2026-11-01 (orchestrator, read-only measurements, not elevated)

Why: I22's Win32 suite waits for >= 16 GB free commit (w8_lock.py rule) and sat at 32/77 with 4-10 GB free.

## Numbers (GetPerformanceInfo, 12:30)
- RAM 15.9 GB, pagefile 48 GB (C:\pagefile.sys, in use 3.2 GB): commit limit 63.9 GB.
- Commit 53.6 GB (peak 61.8 GB). Physical available 2.2 GB. Uptime since 27/10 05:24.
- Process private bytes, all processes: about 18.3 GB (claude 17 processes 8.7 GB, node 33 processes 3.6 GB).
- Kernel paged pool 8.0 GB, nonpaged 1.3 GB.
- Not attributable to any process's private bytes or the pools: about 26 GB.
- Disk is not the problem: C: 155 GB free, D: 614 GB free.

## Leak 1: Section handles in ArmourySocketServer.exe (ASUS ArmouryDevice)
- NtQuerySystemInformation(64) handle census: PID 16052 ArmourySocketServer.exe holds 148,073 handles, 147,752 of them type index 49 =
  Section (NtQueryObject type list). The whole system has 151,035 Section handles and 161,724 Section objects.
- Started 27/10 05:25:30 by the scheduled task \ASUS\ArmourySocketServer (TaskSchedulerTool_ArmourySocketServer.exe, RunLevel Highest,
  logon trigger). Elevated: its path and command line are not readable from this session.
- Its handle count rose 147,949 -> 148,073 in about 20 minutes (about 6 per minute today).
- The about 26 GB of commit that no process's private bytes or pool explain matches pagefile-backed sections kept open; this session
  cannot read section sizes (not elevated), so the attribution is an inference from the counts (about 180 KB per section), not a measurement.
- Freeing: end ArmourySocketServer.exe from an elevated prompt (`taskkill /im ArmourySocketServer.exe /f`, or Task Manager run as
  administrator). It comes back at the next logon or with `schtasks /run /tn "\ASUS\ArmourySocketServer"`; ASUS device control
  (lighting) is what it serves.

## Leak 2: Token objects (paged pool)
- 1,622,276 Token objects alive with only 6,267 Token handles: referenced from the kernel, not through handles. At about 5 KB each this
  is the 8 GB paged pool.
- Rate: +1,050 Token objects in an idle-for-me minute (other sessions spawning processes); 100 spawns of `cmd.exe /c exit 0` added
  +1,011 Token objects and +1 handle, while the Process objects went back down (-3): about 10 leaked tokens per process launch, never
  freed. Heavy test work (pytest, bash, git spawns) drives it.
- Process objects 617 for 463 live processes: zombie processes are not the cause.
- The holder is not identified (needs poolmon / a kernel debugger, admin). Non-Microsoft drivers loaded: AsIO3 (ASUS), CtiAIo64
  (Creative), MsIo64 (MICSYS), HidHide / ViGEmBus (Nefarius), USBPcap, steamxbox, VB-Audio cable, logi_lamparray, AMD, Intel, NVIDIA,
  Realtek. The OS is build 26300 (Insider). fltmc (minifilters) is refused without admin.
- Freeing: only a reboot releases kernel-referenced tokens. Re-measure after it with
  `~/bin/py -3.12 <scratchpad>/leakrate.py 100` (copy kept in log/roadmap/LIVING-PC_memory_tools/).

## After the reboot (2026-11-01 14:23, orchestrator loop session)
- 14:40: commit 8.3 GB of 63.9, paged pool 0.68 GB, nonpaged 0.48 GB, Token objects 4,163 (1,021 handles), Section objects 21,662.
- Token leak: `leakrate.py 100` gives +0 Token objects after 100 `cmd.exe /c exit 0` launches (before the reboot: +1,011). The same
  third-party drivers are loaded (Win32_SystemDriver running: AsIO3 as Asusgio3, CtiAIo64, MsIo64, HidHide, ViGEmBus, USBPcap,
  steamxbox, VB-Audio cable, logi_lamparray), so no loaded driver leaks tokens on every launch by itself; the leak started at
  some point of the 5-day uptime. What changed since: the ASUS Armoury Crate back end (5 WiX bundles, ASUS Update Helper) was
  uninstalled before the reboot. Not settled which; the hourly Memory watch logs tokens with a rate per hour, and leakrate.py is
  re-run when the rate grows past the idle churn (+142 tokens in an idle minute at 14:40).
- Section leak: ArmourySocketServer.exe is running again (pid 13580, started 14:24:36 by the scheduled task
  \ASUS\ArmourySocketServer, which the uninstall left; the three InstallShield SDKs were never removed). 466 handles at 14:36,
  476 at 14:37. G22 (Mariano): end it and delete the task with the elevated script
  log/roadmap/LIVING-PC_memory_tools/asus_task_remove.ps1. Memory watch now logs its handle count and rate on every run
  (memory-watch.json `watchProcesses`) and alerts this session past 50,000 handles.
- 17:06 (G22, run by the orchestrator at Mariano's request, UAC accepted): ArmourySocketServer ended at 2,959 handles (+1,050/h since logon), task \ASUS\ArmourySocketServer deleted (asus_task_remove.ps1, LIVING-PC_asus_uninstall.log). Left in \ASUS\: AcPowerNotification, two ASUSUpdateTaskMachine tasks, NoiseCancelingEngine, P508PowerAgent_sdk.

## Token leak after the reboot (17:00-17:20, orchestrator; tools in LIVING-PC_memory_tools/)
- The leak is back: Token objects 4,266 (14:37) -> 13,214 (15:08) -> 32,818 (16:07) -> 62,799 (17:15), Token handles flat at about
  1,200, so the new tokens are kernel-referenced again. Rate 200-1,100 per minute (median about 450), with about 20 Claude sessions
  (Mashed and SimGolf children) working on the machine. At about 5 KB each that is about 2-3 GB of paged pool per day.
- NOT the cause (each measured against an idle control of the same length, per launch or per call, two rounds):
  leakkinds.py: launches of cmd, python, powershell, bash, git, node from python: -1.8 .. +4.5 (mostly within the idle noise);
  leakmsys.py: children launched inside a Git Bash loop (sleep, /usr/bin/true, cmd, hostname, subshells): -0.8 .. +1.7;
  leakwmi.py: Get-CimInstance Win32_OperatingSystem / Win32_Process and Get-Process inside one powershell: -2.0 .. +1.5.
  Before the reboot 100 cmd launches kept +1,011; at 14:40 +0; at 16:40 +158 (+1.6 each). So the per-launch figure of the
  morning was a symptom of the state then, not a constant property of process creation.
- tokensampler.py (20 min, ts_20m_1.tsv): a least-squares fit of token growth against new processes by image name gives
  sleep.exe +7.5, conhost.exe +9.7, py.exe +14 per launch but bash / git / python / cmd negative; the image names are collinear
  with the sessions' general activity, and the direct launches above keep about 0, so the fit does not name a culprit.
- objdelta.py over 2 min: Token +661 objects / -4 handles; File +4,046 objects / -464 handles (file objects referenced without
  handles: sections and the cache), Process +23, ALPC Port -33. No other type grows with Token.
- Next step (needs one reboot, Mariano's call): set FLG_MAINTAIN_OBJECT_TYPELIST (GlobalFlag 0x4000, elevated:
  `reg add "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager" /v GlobalFlag /t REG_DWORD /d 0x4000 /f`, check the value
  first, it may already hold other bits), reboot, then NtQuerySystemInformation(SystemObjectInformation) lists every Token object
  with its CreatorUniqueProcess: counting the live tokens by creator image names the process (or service) that makes them.
  Remove the flag afterwards. Until then: the Memory watch logs the rate hourly; a reboot every 2-3 days keeps paged pool under
  about 1 GB at the rate measured today.
- 17:25 Mariano: do it. GlobalFlag set to 0x4000 (was 0x0; elevated reg add, UAC accepted). Active from the next reboot.
  After that reboot: (1) start `~/bin/py -3.12 log/roadmap/LIVING-PC_memory_tools/pidlog.py <scratch>/pidlog.tsv 2` early (names
  creators that exit later); (2) after an hour or more of normal load, from an ELEVATED prompt:
  `~/bin/py -3.12 log/roadmap/LIVING-PC_memory_tools/objcreators.py Token --pidlog <scratch>/pidlog.tsv` (live Token objects by
  creator pid, with the count that has no handle); (3) then set GlobalFlag back to 0x0 (elevated) and reboot when convenient.
  objcreators.py today (flag not active yet) returns status 0xc0000001, as expected.
