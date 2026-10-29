# S10.2 merge list

Files: re/match/smsg_golf_*.cpp (29 new canonical files of golf.exe/src, plus S10.1's three), re/match/wip/smsg_golf_GolferShot.cpp,
smsg_golf_Toolbar.cpp, re/tools/smsg_hand.py (unused helper, drop if unwanted), re/tools/smsg_dedupe.py (new, used by 2, 3, 4),
log/roadmap/S10/2_* and runs/summary.tsv, inventory_golf.tsv. Depends on S10.5's match.py (`literal_values`): see 2_notes.md.

Tracker commands (orchestrator runs them; log/diff CSVs are gitignored, regenerate first with the loop in 2_notes.md): the 25
lines of log/roadmap/S10/2_promote_cmds.txt (C3 -> C4 by 100 % match; one row, 0x00434cf0 drawAmenitiesPanel, is a miss and has none).
Decision for the orchestrator: mainLoop 0x0040f5c0 (C2, NO_PATH1) reads 100 % from their MainLoop.cpp, 75,108 bytes (U-1858).
