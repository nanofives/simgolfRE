# S6.1 notes: classification of model_defects #1-#29 and #36

Output: `log/roadmap/S6/1_decisions.tsv` (30 rows, 17 columns per SCHEMA.md), `1_uncertain.tsv` (U-1550..U-1559).

## Method and its limit
Each row was classified from the cited instruction ranges as the model notes record them, the port source (golfers.cpp:198,
dialogs.cpp:878, shots.cpp:1898..1904, util.cpp:310, tick_frame.inc ball lists), UNCERTAINTIES.md and divergences.tsv.
I did NOT re-disassemble every range in this step: the effect columns that depend on reach (can a run get there) are
`unknown` or `no` with the reason in effect_note, and the open reach questions are U-1550..U-1559.

## Result
- recommendation: fix 1 (D11), ask 16, keep 13.
- fix: D11 spawnWalker's free-slot scan (golfers.cpp:198). It is the only row that is a hang/fault with a fix that cannot change a
  completing run. Even there the hang-or-fault outcome is unread (U-1556); the lead should confirm with a Unicorn run
  before S6.5 implements `spawnWalker.fullTable`.
- ask (Mariano): D1, D5, D6, D7, D8, D10, D12, D13, D17..D21, D23, D24, D26. Each is a silent overrun or wrong value whose fix
  changes a save, the simulation state or a frame on a run the original completes (or, D7/D8/D12/D13, on the runs that reach it).
- keep: D2, D3, D4, D9, D14, D15, D16, D22, D25, D27, D28, D29, D36 (harmless, dead state, or reach not shown).
- Switches: existing `sinScaled.readPastTable` (D10) and `shotPower.cacheIgnoresPutt` (D18); 15 proposed names marked
  `to-implement`, 13 rows need none. Switch names for `ask` rows only matter if Mariano chooses fixed.

## Judgement calls to review
- D36 and D13 sit at `keep` / `ask` because net code is outside the offline port and the overflow needs an input length
  (peer name of 0x43 or more, roster string of 19 or more) not shown to occur.
- D27 is dead-state but the save can capture the interval, so changes_save is yes and the recommendation stays keep (the value
  is overwritten before any read but the save, and the port must reproduce the saved word).
- D1 changes tile_build_code, which is saved; counted as changes_save yes, balance unknown (the cells' readers were not traced).

## Reproduce
`awk -F'\t' '{print NF}' log/roadmap/S6/1_decisions.tsv | sort | uniq -c` gives 31 x 17. No banned words (grep checked).
