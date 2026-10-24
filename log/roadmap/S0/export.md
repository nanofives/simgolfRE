# S0 HEAD export (made by the S0 lead, 2026-10-24)

Root: port/build/S0_head (gitignored: inside port/build). Content: `git archive 4bfefc2a4efcd5ba712c4d676dbb4be46a4e34c6`
(HEAD_COMMIT holds the id), so every tracked file is the committed version, none of the S1 / S4 / S6 working-tree edits.
Python's ROOT (parents[2] of port/test/*.py) is the export root, so tests there read the export's sources.

Junctions to the shared tree (gitignored data the tests read; the tracked parts of these dirs are unmodified in the shared tree):
- original -> SimGolf/original (the user's install; never write it)
- log -> SimGolf/log (pinned refs log/c3/alt/refs, capture hashes log/p2/p24, earlier gate outputs log/p4, log/p5; outputs written
  here land in the shared log/, so write only under log/roadmap/S0/)
- port/corpus -> SimGolf/port/corpus (traces, segments, vectors, terrain recordings)
- tests/golden, tests/fixtures, re/versions, tools/vc6 -> the shared dirs
- re/fid is NOT a junction (its tracked files are in the export; the .fidb is absent)

Build dirs inside the export only: port/build/S0_head/port/build/S0_<k> (x64) and S0_<k>_w32 (Win32), tests pointed at them with
SG_PORTSIM_DLL. Never port/build/x64 of the shared tree.

Build-dir ownership inside the export: S0.1 owns its defaults (port/build/S0_head/port/build/x64 and .../Win32); S0.2 builds into
port/build/S0_head/port/build/S0_2 and always sets SG_PORTSIM_DLL to that DLL.
