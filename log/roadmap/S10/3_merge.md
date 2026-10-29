# S10.3 merge list

Files: re/match/smsg_golf_jackal_*.cpp (46 canonical files), re/match/smsg_golf_jpeg_*.cpp (12), re/match/wip/smsg_golf_jackal_multidebug.cpp,
log/roadmap/S10/3_* and the shared runs/summary.tsv. binkw32.lib: nothing to match (U-1861).

Tracker commands (orchestrator): the 77 lines of log/roadmap/S10/3_promote_cmds.txt (C3 -> C4 by 100 % match, jackal.lib rows).
Regenerate the CSVs first: `for s in $(cat log/roadmap/S10/3_evidence_files.txt); do ~/bin/py -3.12 re/tools/smsg_import.py file $s --evidence; done`
then `~/bin/py -3.12 re/tools/smsg_dedupe.py smsg_golf_ --write`. Shared-tool note: smsg_import.py should skip addresses held by other
smsg files (U-1862) so that dedupe is not needed.
