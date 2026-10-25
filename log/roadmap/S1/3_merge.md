# S1.3 merge text (shared files the integrator edits)

## UNCERTAINTIES.md
Append the three rows of log/roadmap/S1/3_uncertain.tsv (U-1170, U-1171, U-1172). In row U-1066 replace the status cell `open` by
`parts 2 and 3 see U-1170 / U-1171 (S1.3); part 1 (tutorial pages 1..29) open` and append to its description:
"S1.3: showThemePackSelect's Cancel is judged by cov_r_theme_cancel (control 2 RED); palette hotspots 21 / 22 are drawn by
showInfoCursorPanel only, so loadInterfaceArt control 3 stays GREEN in the boot run and the cuts are judged as state (U-1170)."

## CLAUDE.md (one paragraph after the last Phase 5 paragraph)
Roadmap S1.3 (2026-10-24, log/roadmap/S1/3_notes.md): scenarios cov_r_theme_cancel (the theme screen's Cancel after a row press in the
same visit, then a career: showThemePackSelect control 2 RED at record 0; listThemes resets g_theme to Standard on entry, a sandbox
ignores g_theme) and cov_r_palette_hot (the Undo hotspot 0x15 and the wrench tab 0x16 of the Build Course palette) pinned on
instances\s1_3, both run from boot with every record equal (123 / 123, 565 / 565). The palette hotspots' SpriteRefs 84 / 88 are read by
drawing only (showInfoCursorPanel), so loadInterfaceArt control 3 reads GREEN by hash and test_s1_3_scenarios.py compares all 96 cut
fields with the original's .fh.bin images instead (control 3 differs in exactly those two).

## re/PORT_PLAN.md / sections.tsv
Nothing. port/test/test_boot.py's NEGCTL list can add ("cov_r_theme_cancel", "showThemePackSelect", 2, "Cancel keeps the theme") and
its docstring's "no scenario cancels the theme screen" sentence can go; left untouched (not in files_owned).
