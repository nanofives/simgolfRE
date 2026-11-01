# S3.24 merge

## Code
`~/bin/py -3.12 log/roadmap/S3/24_assemble.py --root <tree at f2dff003 or later> --only-24` applies 24_edits.py (17 anchored hunks):
port/src/sim/terrain_overlay.cpp, port/include/portsim/sim/terrain_overlay.h, port/test/test_terrain_overlay.py. These are S3.24's own
files; no shared file (modules.inc, tick.cpp, commands.cpp, CMakeLists.txt, framegate.py, gl_raster.*, gfx_terrain.cpp) is touched.
Checked: a fresh f2dff003 tree plus the edits equals the mirror port/build/S3_24 byte for byte (port/build/S3_24_chk).

## UNCERTAINTIES.md
Append the four rows of log/roadmap/S3/24_uncertain.tsv (U-1826 .. U-1829). U-1370 (roadmap S3.8): set status `resolved` with
`[roadmap S3.24] the footprint is the per-column span of N = floor(w * L / |major|) + 2 pixels (log/roadmap/S3/24_notes.md); the
end-cap and tie remainders are U-1826 / U-1827`.

## CLAUDE.md (after the S3.20 paragraph)
Roadmap S3.24, the overlay line's footprint (2026-10-31, notes log/roadmap/S3/24_notes.md, edits 24_edits.py, U-1826..U-1829;
U-1370 resolved): Windows' generic GL draws an antialiased line per column of the major axis as at most N = floor(w * L / |major|) + 2
pixels, the centres whose minor offset o from the line centre times the tie sign (x-major sign(dx) * sign(dy), y-major sign(dx)) lies
in [-N/2, N/2); S3.8's coverage formula gives the values inside. Measured with glreplay.exe on 16-dither-phase probe sets (the cut
meets N/2 at every width and angle where S3.8's support reached past it) and fitted on exact ties. terrain_overlay.cpp's line rule 2
is the default (overlay.setRule / SG_OVERLAY_RULE: 0 = S3.8's model, 6 = no tie decision, 1 / 3 / 4 / 5 / 7 other tie signs;
control 11 = rule 0); SG_OVERLAY_DUMP=<file> dumps every drawn call with its box before / after. Results: S3.8's probe scenes 108 / 88 /
1 / 18 -> 7 / 12 / 0 / 5 px with none newly wrong; the game's own 9,534 dumped calls through the real GL 879 -> 104 px, none newly
wrong; the 31 overlay-line census rows (22_causes.tsv group 1) 29 PASS with the host-GL terrain (left: cov_a_build brush_465 3 px, the
second-vertex end-cap drop U-1826; cov_a_keys k_dlg_R 1 px value); the default GlRaster census 111 / 577 / 0 / 10 equal in status,
0 PASS lost, 0 rows with more px, 29 rows with fewer (71,871 -> 71,517); segments 205 / 205 and boot 66 / 2 / 2 unchanged; unit tests
33 on x64 and Win32. Open: the end-cap drop at the second vertex of wide lines (U-1826) and a few y-major / non-45-degree ties (U-1827).
