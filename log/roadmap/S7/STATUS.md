# S7 STATUS (lead: session cmvmrorle3t0no115odl0j9k8, claude3 opus since the license move; row S7 of re/roadmap/sections.tsv; D1 Windows first)

steps: 9 of 9 accepted; resubmission = log/roadmap/S7/9_merge.md (waves 1-3 as one merge on 78ffb06d); play session waits on G8
workers alive: none; cap 1
game slots: none held (S7.4 released instance s7_4, 2026-10-25)
gate: the app plays: `simgolf_port.exe --install <dir>` title menu 0 px vs the original, sandbox reached by posted messages, 355 frame periods 87..100 ms, idle 3.1 % CPU, hover follows the cursor, recording replays 422/422 records (test_app_live 13); S7.1 equivalence 70 rows x64 = Win32, segments 205/205, boot 58/2/1 (rebased on 2c05f02a by S7.7: equivalence 68 PASS + 2 NO_FRAMES, segments 205/205, boot 0 rows differ)
waits on: G8 (Mariano plays after S3.5, S3.6 / S3.6b, S3.7 merge); I6 commit
last event: 2026-10-26 S7.9 accepted and closed; S7 DONE sent

## Plan
- S7.1 live run core (tick.cpp fed on demand by a host, every answer recorded as a schema v1 trace + framehash).
- S7.2 input translation (accepted): Windows messages -> root-handler commands / cursor / async keys.
- S7.3 Win32 app shell without the sim loop (window, present, input capture, saves outside the install, pacing module).
- S7.6 live integration (sim thread on S7.1, pacing, presents, recording).
- S7.4 app recordings in the boot gate, plus a cross-check on the original (1 game slot).
- S7.5 in-game presentation after S2.1 / S3.2 merge, then Mariano's play session (BLOCKED_DECISION with the start command).

## Workers
| step | session | state |
|---|---|---|
| S7.1 | cmvmrswhs3tflo1158kfsd918 | accepted, closed |
| S7.6 | cmvmxlyoo4sgbo115ylo9od09 | accepted, closed |
| S7.7 | cmvn5wspf62jbo1158i3ghts6 | accepted, closed |
| S7.2 | cmvmrt0pk3tg9o115x7tb66f4 | accepted, closed |
| S7.3 | cmvmsgnhg3w8ho1151to4vkeg | accepted, closed |
| S7.4 | cmvnb72ss6qhho115y44rofm6 | accepted, closed |
| S7.5 | cmvniobsx7dwno115if4x1l81 | accepted, closed |
| S7.8 | cmvnugsw48dfvo115qt4ke6oz | accepted, closed |
| S7.9 | cmvoyhw6kaoh9o115q9ifphce | accepted, closed |
