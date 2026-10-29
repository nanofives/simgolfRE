# Decisions pending (Mariano)

Open questions the orchestrator batches (AskUserQuestion, four per call). Each has the sections it blocks. Answers go in the
`Answered` part with the date.

## Open

(none)

## Answered

- **G12, network play, 2026-10-29: start S9 now** (Mariano: "Start S9 network play too"). Supersedes D6's `later`. Plan
  re/roadmap/S9_PLAN.md; it needs two game slots for original host + client recordings.
- **G11, suite reuse, 2026-10-29: reuse the lead's suite results when the trees are byte-identical.** An integrator whose mirror equals
  the lead's gated tree in port/src, include, test, app and CMakeLists.txt skips the x64 / Win32 port suite re-run and cites the lead's
  results; every other gate still runs (re/ROADMAP_ORCHESTRATOR.md, amendment G11). Applied first to I14 (S4), whose suite re-run was
  stopped at 15 of 95 x64 files, all PASS.
- **G10, app priority in test runs, 2026-10-28: Normal for the app only.** The simgolf_port.exe process that the app tests
  launch (app_drive.launch) runs at NORMAL_PRIORITY_CLASS, as an Explorer launch gives it, switchable (SG_APP_PRIORITY=inherit restores
  the test slot's class); pytest and every other test stay at BelowNormal under the workspace test-slot rule.

- **G9, merge order and the play test, 2026-10-27: S3 before S4, no preview build.** The S3 resubmission (I12) merges before the
  S4 audio resubmission (I11) so the play test (G8) comes sooner; Mariano plays only the gated, merged build (no ungated preview of S7 + S3).

- **D1, phase 8 (S7), 2026-10-24: yes, Windows first.** A playable host joins the roadmap as phase 8: a Win32 window, the GDI text
  provider, the port's software rasteriser for the terrain. A non-Windows host comes later, after S4 and S5.
- **D2, Bink movies (S8), 2026-10-24: skip path for now.** S8 documents and tests a skip path (the tests already boot with skip_intro)
  plus any FLIC cutscenes outside S3. A decoder (FFmpeg as a system library) is a later decision, not taken.
- **D3, audio recordings (S4), 2026-10-24: postponed.** No recordings of the original's audio output in this round. S4 builds the
  audio engine model from the C3 / C4 sound.dll hooks and their path-1 evidence only. The output-device question (TV or a virtual
  device) comes back when recordings are planned.
- **D4, text without GDI (S5), 2026-10-24: FreeType.** FreeType may be used as a system library for S5's rasteriser. S5 measures it
  against the 11,459 recorded widths and every recorded label pixel; switching the port's text away from GDI still needs that result
  brought to Mariano.
- **D5, defect defaults (S6), 2026-10-24: fix hangs and faults only.** Defects that hang or fault the game may be fixed in the `fixed`
  profile without asking. Any defect whose fix changes a save, a balance value, the simulation state or a frame comes to Mariano.
  The `original` profile stays the test oracle.
- **D6, network play, 2026-10-24: a section later.** Row S9 in re/roadmap/sections.tsv, status `later`, after S7. Not started in this
  round.
- **Session cap, 2026-10-24: 12** claude2 sessions alive at once under the orchestrator (leads and workers; the orchestrator excluded).
- **G1, S4's gate reading, 2026-10-24: the emulated original.** Each ported sound.dll function is judged against the ORIGINAL
  function run in Unicorn over the user's sound.dll (relocated) on its fixture replayed into a virtual address space: return and
  every compared region byte for byte; the emulated original is tied to the live path-1 CSV on every address-independent field
  (scalar return, state_changed, distinct_results, hashes of regions without run addresses); default runs check golden digests
  pinned from the emulator (3.1a / 3.7b pattern). Detail: log/roadmap/S4/STATUS.md section G1.
- **G2, S3's terrain criterion, 2026-10-24: 4bfefc2a's constants.** A terrain region passes with one-level pixels <= 1.5 % of
  the drawn pixels (glraster.MAX_ONE_LEVEL) and <= 70 pixels more than one level off (MAX_BIG), as the code enforces since the P5.3
  handoff 4bfefc2a (measured over 49 passes: one-level mean 0.47 %, worst 1.05 %; beyond one level 139 px in total, worst pass 50).
  It replaces the row's max(2 %, 0.4 % x median rho) / 0-beyond criterion. U-1076's remaining per-triangle term stays open as work.
- **G3, S6's 26 `ask` defects, 2026-10-24: keep 24, fix the two hangs.** (a) the memory overruns D1 D5 D6 D10 D11 D12 D47b D48a
  D49a, (b) the wrong values in shots, golfers and stories D17 D18 D19 D32 D37 D38 D39 D44 D48b and (c) the course, terrain and money
  rows D20 D21 D23 D24 D26 D52a stay `keep`: both profiles reproduce the original, the switches stay at 1 and can be revisited row by
  row. (d) the two hangs whose reach is not proven, D42c (loadGolfPro with difficulty 5 / 6) and D52b3 (generateRandomCourse's
  search), become `fix` and join the `fixed` profile (S6 wave 2); `original` keeps the hang.
- **G4, FreeType install route, 2026-10-24: the orchestrator runs vcpkg.** Done the same day from the claude3 session: the clone
  C:\Users\maria\vcpkg (microsoft/vcpkg 0c2a9ace) bootstrapped with `-disableMetrics` (vcpkg-tool 2026-09-26, signature validated),
  then `vcpkg install freetype[core]:x64-windows freetype[core]:x86-windows`: FreeType 2.14.3 built from source for both triplets
  under C:\Users\maria\vcpkg\installed\<triplet> (logs log/roadmap/S5/vcpkg_*.log). License FTL or GPL-2.0-or-later.
- **G5, the port's text default, 2026-10-24: keep GDI.** GDI stays the default text provider on Windows; FreeType stays in the build
  as an option (SG_TEXT_METRICS=freetype, configured with SG_FREETYPE_ROOT) for a host without GDI, judged by S5's measured numbers
  (widths and metrics equal, label pixels differ, U-1520).
- **G6, a `cmake` allow rule on claude2, 2026-10-24: yes.** `Bash(cmake:*)` is in C:\Users\maria\.claude-account2\settings.json
  (it was already there when the orchestrator went to add it: the file had been edited at 16:38 the same day).
- **G7, where children run after claude2 hit 100 %, 2026-10-24: claude3, cap 12.** All 12 roadmap children were moved to claude3
  (a Terminal session, `Stop claude2 sessions at 100%, move to claude3`). New leads, workers and integrators spawn on claude3 with the
  alias models, under the same cap of 12 roadmap sessions, until claude2 resets; then new spawns go back to claude2.
- **G8, the play session (S7's last gate), 2026-10-25: wait for the sprites and the HUD.** The app was ready
  (log/roadmap/S7/5_play.bat, guide log/roadmap/S7/5_notes.md section 6: title menu, location screen and terrain drawn as the original;
  HUD panels, radial toolbar, build palettes, message boxes and sprites not drawn yet, U-1641; zoom frames 144..186 ms, U-1640; no
  sound before S4). Mariano plays once S3.5 (sprites), S3.6 / S3.6b (HUD panels) and S3.7 (ticker, cursor, tooltip) are merged; S7's
  gate stays open until then.
