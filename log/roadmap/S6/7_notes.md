# S6.7 notes: D42c, loadGolfPro with difficulty 5 / 6 (decision G3: `fix`)

Status: DONE.

## What the original does
loadGolfPro (0x0046c970) draws a pro per golfer in a rejection loop. A draw is accepted when
`v <= tries / (4 - d) + rep + cash` (division at 0x0046cc63, d = g_difficulty 0x00822c88) and
`v >= rep - slack + cash` (slack = tries for golfer g != 0, tries / 4 for g == 0); rep = g_tournamentPurse / 10,
cash = g_cash_div100 / 200. With d = 5 / 6, 4 - d < 0, so the upper bound falls as tries grows. For golfer 0 at d = 5 the
window [R - t/4, R - t] is empty for every t >= 1, so any state with hole 1 open never ends; other states end only when a try
lands v inside the window. Probe: log/roadmap/S6/6_probes/d42_golfpro.py (course_season d = 5 / 6 hang, sandbox_basic has
no open hole and completes).

## The switch
`loadGolfPro.negativeWindowBound` (port/src/sim/golfers.cpp, g_defect_pro_window_bound, row in kDefects; a separate switch
from wave 1's divideAndRejectGuard). Code in port/src/sim/golfers_pro.inc. When 4 - d < 0 the port computes
vmax = max|skill - 10|^2 / |(d * 5 + 0x14) * 2| * (1 + |d|) + 0x10000 over the 100 pro records (a bound on |v|: the
g == 0 adjustment and the flag-0x4000000 draw rnd(v) & 0xffff stay inside it) and, at each accepted-candidate try, the
upper bound `tries / (4 - d) + rep + cash` as int64. Once it is under -vmax no later try can be accepted (the bound never
rises), so:
- 1 (default, the original): throw NoReturn{"loadGolfPro"}, the port's convention for endless loops (before this step the
  port spun for ever there, like the original);
- 0: end the draw with pick 0 (`gave_up`), the arm wave 1 chose.
On every state where the original completes, the check never fires, so 1 and 0 give the same result.
Negctl mode 3 of loadGolfPro (wrong body): gives up when the bound is under 1 instead of -vmax.

## Evidence (build port/build/S6_7, SG_PORTSIM_DLL=port/build/S6_7/Release/portsim_c.dll)
- port/test/test_defects_golfers_w2.py: 45 passed (log 7_test_defects_golfers_w2.log): probe states (course_season start
  image, pros 1..99 or only 1 filled, d = 5 / 6) NoReturn at 1, ok at 0; 18 synthetic states (holes (1), (2), (1,2,3);
  purse 0 / 50 / 5000; d = 5 / 6): 11 NoReturn at 1 and ok at 0, 7 complete identically at 1 and 0; completing controls
  d = -1, 0..3 on course_season and sandbox_basic images, sandbox d = 5 / 6 (no hole), wave 1's synthetic controls, the
  menu-style call on a zero state: identical whole-image hash at 1 and 0; d = 4 still SG_E_DIVIDE with wave 1's switch on.
- Original in Unicorn (log/roadmap/S6/7_emu_check.py, log 7_emu_check.log): over the 36 synthetic states (d = 5, 6, 2, -1)
  the original hangs in exactly the 11 states where the port raises NoReturn; in the other 25 it gets past the draw (it
  stops at loadThemeFile's KERNEL32 file I/O, which the emulator does not serve: U-1592).
- Wave 1 unchanged: test_defects_golfers 25 passed, test_golfers 60 passed.
- Golfers vectors (log/roadmap/S6/7_vectors.py, log 7_vectors.log): all 16 functions GREEN, 367 / 367, with the switch at 1
  and at 0 (loadGolfPro 2 / 2).
- Controls: wrong bound (mode 3) RED on the d = 5 completing state (hole 2, purse 50); 1 / 1 RED, 0 blind.
- Segment and boot gates were not re-run: the switch's default path changes nothing on a state the original completes
  (argument above), and no corpus scenario plays at difficulty 5 / 6.

## Reproduce
```
cmake -S port -B port/build/S6_7 -A x64 && cmake --build port/build/S6_7 --config Release --target portsim_c
SG_PORTSIM_DLL=$PWD/port/build/S6_7/Release/portsim_c.dll ~/bin/py -3.12 -m pytest port/test/test_defects_golfers_w2.py -q
PYTHONPATH=. ~/bin/py -3.12 log/roadmap/S6/7_emu_check.py
```

## Merge
log/roadmap/S6/7_merge.md: the defect_profiles.tsv row, the integrator.py evidence-map line, U-1591..U-1592.
