# S10 status (SMSG import and verification)

steps: 13 total, all accepted (wave 1 S10.1-S10.10: MERGE.md; wave 2 S10.11-S10.13: MERGE_w2.md), running 0
live workers: none (all worker sessions closed)
game slots held: 0 (no step needed the game)
gate: 1,104 new MATCH lines all 100% (exe 609, sound 489, jgld 6); match tier 484 passed; static 770 / 2 skipped; 9 names files 1,578 rows, 3 names tools 0 bad
C4: 165 promote commands listed, 163 to run (gameMain refused by open U-1351; sound strcpy 0x4a010 dropped, MERGE.md section 7)
last event: 2026-10-29 18:25 wave 2 accepted; MERGE_w2.md + files_w2.txt (126 paths) ready
next: idle; wave 1 merging as I16, wave 2 queued; respond only if a wave is REFUSED
open decisions: none (orchestrator answered D1 c, D2 keep C2 + match in hold reason, D3 / D4 accepted, gameMain skipped; U-1910 agreed)
merge: ready (MERGE.md + files.txt; MERGE_w2.md + files_w2.txt)
## Notes
- U ids: 5 per step (range U-1850..U-1899 is 50 ids for 9 steps); lead reserve U-1895..U-1899.
- PDB tables (procs / publics / modules / udts per module) regenerated from third_party/smsg/release/*.pdb with
  log/roadmap/S10/pdb/pdbread.py (identical to the 2026-10-29 investigation's output); investigation summary in
  log/roadmap/S10/pdb/investigation_summary.json.
- Evidence CSV rule for every step: write log/diff match CSVs only for 100% canonical MATCH lines.
