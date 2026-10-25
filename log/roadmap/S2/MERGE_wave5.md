# S2 merge request, wave 5: boot screens, reports, dialogs (S2.7, S2.8, S2.9) integrated by S2.13, plus the U-1254 fix

Apply AFTER waves 1-4 (integrator I5). The complete instructions are log/roadmap/S2/13_merge.md: the 7 new files from
port/build/S2_13_head and ONE patch, 13_shared.patch (16 files). It REPLACES 7_shared(_wt).patch, 8_shared.patch and 9_shared.patch.
Lead check (2026-10-25): on a scratch copy of today's working-tree files, 3_shared.patch, 4_shared.patch, then 13_shared.patch apply.
Once I5's commit lands: `~/bin/py -3.12 log/roadmap/S2/13_tools/check_on_commit.py HEAD <scratch>` must print rc 0 and
`equal to the mirror: yes` (13_merge.md section 0). No boot.py KNOWN entry. S2.12's call logs (12_*) are evidence only, no code.
Gate (S2.13 on the combined tree): x64 static 6, gfx 25, credits 47, routing 35, boot_screens 32, reports 36, dialogs 32, commands 83
(44 skipped), tick 250, test_reports 14, s2_5 3, s2_6 4; Win32 gfx files pass; segments 205/205 x64 + Win32 0 digests changed; boot 70
rows: 65 PASS / 2 KNOWN / 2 NO_FRAMES / 1 FAIL (cov_r_s2_screens, now equal through record 1156, stops at the unported showBuyLandScreen,
U-1251); gfx.check() 0 duplicates; census 83 PASS (no PASS lost; 69 UNSUPPORTED rows grew in px from stale root content).
Do not commit any *_img* dir or *.tsv.d dir. Files: files_wave5.txt. UNCERTAINTIES rows: 7_, 8_, 9_, 12_, 13_uncertain.tsv; CLAUDE.md
paragraphs: the ones in 7_merge.md, 8_merge.md, 9_merge.md and 13_merge.md (13's supersedes the shared-body ownership statements).
