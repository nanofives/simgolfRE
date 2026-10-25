# S2.6: pinned scenario cov_r_s2_screens (2D screens no pinned checkpoint showed)

Status: DONE except the scroll-text family (U-1250). Pinned (det_a == det_b == det_c, 32 checkpoints), c_a captured (SAME as
det_b), trace recorded (validate OK) and replayed from the trace alone (byte-identical trace, 31 of 32 checkpoints SAME, theme_packs
frame-only DIFF: U-1252). Port boot: every record 0..1155 equal except the cursor tile at record 771 (U-1253, KNOWN entry proposed),
then the run stops at the unported showBuyLandScreen (U-1251). scenario.py's hook still imports only scenario_s1_*.py, so every
game run went through re/tools/s2_6_runs.py (the lead's instruction), which imports scenario_s2_6 first.

## The scenario (re/tools/scenario_s2_6.py)
Name `cov_r_s2_screens` (s36a family through the `cov_r` prefix). One boot, about 261 virtual s, 32 checkpoints. Order:
boot loading screen, title-menu screens, a career (Start New Game -> Easy -> Northeast: Monterey's $700,000 is above the
$100,000 this device-less timeline gives, probe1), a par 3 built and opened, then the in-game screens, buy land last.

Two deliberate state writes (c.write32, kept in the schedule like cov_b_build's):
- g_eventFlags567a1c 0x00567a1c = 1: the frame tests bit 0 at 0x0041e2a0 and shows the land popup (showPopup 0x0041e325);
  option 0 runs showBuyLandScreen (0x0041e331). In play patronEvent sets the bit (0x00426b90..0x00426b9a).
- g_date 0x00834170 = (date | 0x1fff) - 0x30: the year end runs when date % 8192 == 0 (0x0041822b) and calls
  showEndOfYearScreen at 0x004182ee unless g_flags bit 0x4000000 is set (0x004182e6; clear in a career).

Measured positions (each from a probe frame, constants at the end of the file):
- hole tee (220, 340), green (520, 330): the tee at (700, 400) was refused in probe3 / probe4 (outside the owned centre
  tract; buy_land's map shows tract 5 "Already purchased");
- hire button (270, 558): hitEmployee436b00's table 0x004c7af0 entry 0 = (254, 542), hot spot offset 0x10; code 0 sets
  [0x0059ca54] = 0 (handleEmployeeAction 0x00436c61) and drawEmployeeDetailPanel then runs hireEmployee 0x0040aa80
  (0x00437706). ui_tour's (368, 530) hits an empty employee slot and opens nothing on this course (probe6 / probe7); the
  action icon (650, 513) shows "Move this employee" (probe8);
- land popup option 0 "Yup, I've got big plans." (380, 222) (probe3 frame land_popup);
- end of year: any left click ends the loop (0x0044e45c); the check mark (576, 246) is clicked (probe6 frame end_of_year);
- the boot loading screen: showLoadingScreen(0) at 0x0040f919 runs before loadInterfaceArt(1), before the title menu; the
  checkpoints at T 100, 200, 300 each show it (three different pictures and hints, "Loading..." bar). After the location
  click no loading screen is drawn (probe2 / probe3: 400 ms and the button-up instant both show the course).
- Play a Championship goes Select Difficulty -> Easy -> Select Championship Course -> OK -> Pick A Pro; one back arrow
  from the pro list returns to the title menu (probe2 pro_back0).

## Checkpoints and the screen each shows (viewed on det_b: log/roadmap/S2/6_img/det_b/sheet_*.png)
| checkpoint | screen | family (re/roadmap/S2_screens.tsv) |
|---|---|---|
| loading_t200 / _t300 | loading screen over the title art, quote + "Loading..." bar | loading screen (showLoadingScreen) |
| menu | title menu | (pinned elsewhere) |
| theme_packs | Select a Theme Pack grid | theme packs (showThemePackSelect) |
| difficulty | Select Difficulty | difficulty (selectDifficultyScreen) |
| champ_courses | Select Championship Course list (Ocean's Edge MC.cse) | pick a pro / championship browser |
| pick_a_pro | Pick A Pro (Gary Golf.pro) | pick a pro (pickAProScreen) |
| title_back | title menu | - |
| course | Northeast course, hole 1 built, Build Course palette | none (2D) |
| radial_hover | "Build Course" hover label of the radial menu | main radial menu (showMainRadialMenu) |
| rep_comments | PLAYER COMMENTS REPORT (F2) | reports F1-F10 |
| rep_histograph | HISTOGRAPH (F3) | reports F1-F10 |
| rep_financial | FINANCIAL REPORT (F4) | reports F1-F10 |
| rep_report1 | REPORT of the SIM GOLF ASSOCIATION (F7, showCourseReport(1)) | SGA report (pinned as k_dlg_J with (0)) |
| rep_shortcuts | KEYBOARD SHORTCUTS (F8) | shortcuts / roster |
| rep_roster | Membership Roster (F9) | shortcuts / roster |
| bulletin | Professional Accomplishments trophy board (F10) | bulletin board alone |
| hole_stats | HOLE STATS for Hole 1 | reports (showHoleStatsScreen) |
| best_scores | Best 1 Hole Scores over the Information menu | reports (showBestScoresScreen) |
| system_menu | the wrench (system) menu alone | system menu |
| preferences | Preferences over the system menu | preferences |
| hire | HIRE AN EMPLOYEE | hire employee |
| end_of_year | END of YEAR: 2001 with "March: Hole 1 opened." | end of year |
| skill_points | Gary Golf skill points dialog | skill points |
| skill_popup | "You haven't used all your skill points!" popup | popups |
| practice | practice round, shot panel | none |
| cancel_prompt | cancel-match popup over the system menu | popups |
| editor | character editor for Gary Golf (Golf Pro) | character editor (pinned elsewhere) |
| pre_land | course | none |
| land_popup | "Do you wish to purchase additional land" popup | popups |
| buy_land | TRACTS FOR SALE with the lot map | buy land (showBuyLandScreen) |
| end | course | none |

Not covered: scroll text (U-1250).

## Probe tooling
- log/roadmap/S2/6_probe.py: imports scenario_s2_6 then runs scenario_refs.py's command line; used ONLY with
  `run --record --schedule <scratchpad>` for the probes (outputs log/c3/alt/refs/cov_r_s2_screens/probe*, logs
  log/roadmap/S2/6_probe*.log). Delete the probe* run dirs before pinning if they get in the way.
- log/roadmap/S2/6_montage.py: 2x2 contact sheets of checkpoint PNGs (log/roadmap/S2/6_img/<run>/).
- Port DLL: HEAD mirror (git archive HEAD port re/model -> port/build/S2_6_mirror), built with log/roadmap/S2/build.py
  into port/build/S2_6 (log/roadmap/S2/6_build.log): the working tree's audio_*.cpp (a sibling's, half written) does not compile.

## Reproduce
```
~/bin/py -3.12 re/tools/instances.py create s2_6 ; cp instances/p4q/winmm.dll instances/s2_6/winmm.dll
export SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none
rm -rf log/p2/p24/cov_r_s2_screens port/corpus/traces/cov_r_s2_screens.trace*      # only when re-recording
~/bin/py -3.12 re/tools/s2_6_runs.py refs regen cov_r_s2_screens --rerecord --jobs 1 --instances s2_6     (6_regen.log)
~/bin/py -3.12 re/tools/s2_6_runs.py seg --instance s2_6 capture --scenario cov_r_s2_screens --label c_a  (6_capture.log)
~/bin/py -3.12 re/tools/s2_6_runs.py seg --instance s2_6 trace --scenario cov_r_s2_screens                (6_trace.log)
~/bin/py -3.12 re/tools/s2_6_runs.py trace replay --scenario cov_r_s2_screens --label t_rep1 --trace port/corpus/traces/cov_r_s2_screens.trace --instance s2_6   (6_replay.log)
SG_PORTSIM_DLL=port/build/S2_6/Release/portsim_c.dll ~/bin/py -3.12 -m port.test.boot cov_r_s2_screens
SG_PORTSIM_DLL=... ~/bin/py -3.12 -m port.test.framegate cov_r_s2_screens --out log/roadmap/S2/6_framegate.tsv
SG_PORTSIM_DLL=... ~/bin/py -3.12 -m pytest port/test/test_s2_6_scenario.py
```

## Results
- Pinning (6_regen.log): `cov_r_s2_screens PINNED b/c SAME (32 checkpoints) a/b SAME (32 checkpoints)`; manifest entry in 6_merge.md.
- Capture c_a (6_capture.log): 32 checkpoints, SAME as det_b, 67 s wall.
- Trace (6_trace.log): port/corpus/traces/cov_r_s2_screens.trace, 2,301,259 bytes, 1,268 frames, 2,581 polls, validate OK.
  The recording arm differs from det_b at theme_packs only, frame 2,170 px, state SAME (U-1252).
- Replay from the trace alone (6_replay.log): trace BYTE-IDENTICAL to the source, site mismatches 0, overrun 0, deliveries
  3330/3330; checkpoints vs det_b: the same theme_packs frame DIFF, the other 31 SAME.
- Trace pitfall: a checkpoint at the first poll (T 101) is written into the trace with an empty name (trace_rec.js dumpRegions
  waits for a `trecname` reply that is empty there), so validate read `END counts 1268/2581/33 != 1268/2581/32`; the first
  version had loading_t100, now removed (loading_t200 / _t300 remain).
- Port boot (6_boot.log, HEAD-mirror DLL): `FAIL 1156/1268 record 771: g_cursorTileX, g_cursorTileY`. With the proposed KNOWN
  entry (771, 772) the run is KNOWN through record 1156 and then stops with `missing: showBuyLandScreen` (sg_run_boot -11).
  Record 771 = T 1220177 (virtual 171601), the first frame after the end-of-year screen's exit click at (576, 246).
- Framegate (6_framegate.tsv, without the KNOWN entry): PASS 2 (menu, title_back), UNSUPPORTED 17 (first unsupported is
  `Image_draw: a SpriteRef the backend has no sprite for` on every one: difficulty, champ_courses, pick_a_pro and all in-game
  rows up to hire), FAIL 3 (loading_t200 479,810 px, loading_t300 479,832 px, theme_packs 466,918 px; whole frame, no unsupported
  drawing call logged before the snapshot: showLoadingScreen and showThemePackSelect draw nothing in the port and are not in
  gfx.log's unsupported list; these are frame FAILs, the boot's sim records are equal there), NOT_RUN 10 (end_of_year .. end:
  the boot stops at record 771). framegate runs each scenario in a child process, so a KNOWN entry patched in from outside
  does not reach it (6_framegate_known.tsv is identical); the 8 rows from end_of_year to land_popup become judgeable once the
  KNOWN entry is merged; buy_land and end stay NOT_RUN until showBuyLandScreen is ported.
- Tests (6_test.log): port/test/test_s2_6_scenario.py 3 passed (boot equal up to record 1156 with the KNOWN entry; boot stops
  only at showBuyLandScreen; control). Control showEndOfYearScreen mode 1 RED at record 771 on g_memberTypeHistory (plus the
  cursor tile): 1/1 RED, 0 blind.
- Segment / boot gates of the existing scenarios: no port code changed in this step (scenario, driver, test and notes only), so
  they are unchanged by construction; not re-run.

## Open
- U-1250 scroll text: no checkpoint (the editor's load control stops the lockstep clock).
- U-1251 showBuyLandScreen unported: the boot stops after record 1156.
- U-1252 the trace recorder changes theme_packs' frame (2,170 px); first-poll checkpoint name empty.
- U-1253 cursor tile at record 771; merge the KNOWN entry (6_merge.md item 3).
- Framegate FAILs for showLoadingScreen / showThemePackSelect: they should read UNSUPPORTED (a drawing body with no gfx body
  should be logged as unsupported) - for the S2 gfx streams.
