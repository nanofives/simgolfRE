# Decisions pending (Mariano)

Open questions the orchestrator batches (AskUserQuestion, four per call). Each has the sections it blocks. Answers go in the
`Answered` part with the date.

## Open

(none)

## Answered

- **G32, widescreen, 2026-11-03.** (1) Sim visibility (worldToScreen's on-screen test: playSoundAt's RNG draw, story advances): similar to the original but adapted, a centred window that is the same percentage of the native wide view as the original's 800 is of its view (a `play` switch, default 1 = the 800 window). (2) HUD panels move to the edges of the wide view and scale up so they keep the same size relative to the window as at 800 x 600 (stage 3 now, not later). (3) `--wide` ON by default from day 1, and dynamic: the game is played mostly windowed, so the wide width follows window resizes during a session.
- **G31, the + / - keys, 2026-11-03: replace them.** Both keyboards' + / - zoom (the app default `--zoom-keys all`, P5); the original's hidden + (money) / - and = (terrain) key actions are dropped in the app. `--zoom-keys numpad` stays available.
- **G30, after the roadmap: diverge from the original with fixes, 2026-11-03.** I25 (S11 wave 2) runs with fast gates only (no full suites, census or slow suite). Fixes go into a new profile `play` (= `fixed` + gameplay fixes, the app default; `fixed` stays hangs and faults only; `original` stays every switch at 1 and the oracle). First batch: the five existing switches (sinScaled.readPastTable, shotPower.cacheIgnoresPutt, chooseShotTarget.strictFlagSticky, prepareShot.hazardCellTransposed, narrateShot.maskNotRestored), money and reports (D23, D24), golfers and stories (D19, D26, D48b, D44, D17), memory overruns (D11, D12, D47b, D48a, D49a, D5, D6, D21). Save files may change format (no need to stay loadable by the original). Testing light: build, the fast regression gates under `original`, a play check.
- **G26, fillSpan C4 evidence (U-2000), 2026-11-02: not evidence.** An SMSG body that is one inline `__asm` block inside a compiled C
  frame matches by construction (only the frame could fail), as round 36's naked-asm rule: fillSpanDown 0x00493000 / fillSpanUp
  0x00493080 stay C3 held until scenario evidence (S11 wave 2 rewords their c3_hold.tsv reason).
- **G27, S3's last FAIL row (U-1978), 2026-11-02: close S3 with cov_a_keys k_dlg_R held.** 1 px one level on a routing-map hole
  line, antialiased-line arithmetic not determined; no host-GL overlay step (G13 stands).
- **G28, reboot timing for the token-holder search, 2026-11-02: after S3 wave 10 merges.** The orchestrator tells Mariano when the merge
  is committed and no heavy run is active; GlobalFlag 0x4000 (set 2026-11-01 17:25) is then active, pidlog.py starts early,
  objcreators.py (elevated) after an hour of load, then GlobalFlag back to 0.
- **G29, after S3 and S11, 2026-11-02: wrap up the roadmap.** No new section: the end steps of re/ROADMAP_ORCHESTRATOR.md ("The end":
  the slow suite through one integrator, the final report per section, every child closed).
- **G22, ASUS ArmourySocketServer after the reboot, 2026-11-01: delete the task and end the process.** It came back at logon from the surviving scheduled task \ASUS\ArmourySocketServer (the Section-handle leak, about 25 GB before); Mariano runs the elevated command the orchestrator gives (log/roadmap/LIVING-PC_memory_2026-11-01.md, after the reboot).
- **G23, the RE track after S3, 2026-11-01: RE upkeep (new section S11).** The deferred S10 renames in port / shim keys (D1 `P` rows), the g_animalRecords / g_recordBanks symbols, ApplyNames leftovers, the last C3 rows to C4, then a triage of the open C2 rows (dead / trivial / reachable). Port-key renames wait for S3's last wave to merge.
- **G24, golfer_events' 10 NOT_RUN checkpoints (U-0681), 2026-11-01: extend the trace schema and re-record.** A DELIVER kind for injected calls in trace_rec.js / trace_format / the port's replay; golfer_events re-recorded and judged like every other scenario (S3 wave 10).
- **G25, the stale shared working copy, 2026-11-01: back up, then reset.** `git diff` of the working copy saved to log/roadmap/worktree_pre_reset.patch, then the tracked port/ files and the stale docs (equal to an older commit) checked out from HEAD; orchestrator files with newer content and untracked files untouched.
- **G21, the long-session memory test's limit, 2026-11-01: (a) 8 MB now, the 0.97 MB mode investigated in the background.** test_app_present.py GROWTH_LIMIT 4 << 20 -> 8 << 20 (applied in I22's mirror); the control still has to grow past ten times it (139 MB); step P3 looks for what makes some runs gain about 4.28 MB instead of about 3.3 MB.
- **G20, I22's suite scope, 2026-11-01: no answer before the gates ended, so the full suites ran (the default).** Recorded for the record.
- **G19, picker tool, 2026-10-30: withdrawn.** Mariano: the request was meant for the SimCity project; S7 wave 5 dropped (log/roadmap/S7/PICKER.md kept as a plan only).
- **G17, I20's suite scope, 2026-10-30: the changed-code block.** I20 runs every regression gate plus the audio, defect, profile, integrator, static, live and app test files on x64 and Win32, then stops (the rest of both suites is skipped for this merge); the sound build ships tonight.
- **G18, census terrain-origin rule (S3 W8.1), 2026-10-30: both in parallel.** W8.1 makes the census judge terrain-origin pixels under G2 and 2D pixels exactly, reporting the raw and the split verdict; W8.2 (the time-boxed GlRaster residue hunt, U-1392) runs at the same time.
- **G16, music folders (U-1608), 2026-10-30: extend the alias to them.** The exe opens six tracks under `sounds\music\Desert Music\`, `Links Music\`, `Tropical Music\`; the extracted install has `Desert_Music` / `Links_Music` / `Tropical_Music` (the Golf_Sfx rename). G15's switch also tries the underscore folders for these three (profile `fixed`, app only, sim unchanged). The seven names wrong on the CD itself stay as the original.
- **G15, course sfx folder name (U-1499), 2026-10-30: the port finds both names.** The stock install has `sounds\Golf_Sfx`, the exe
  opens `sounds\Golf sfx`, so the original never plays those 26 sounds. A defect switch (default 1 = the original's failed opens; 0 =
  also try `Golf_Sfx`) in profile `fixed`, used by the app; install files untouched; simulation unchanged.
- **G13 revised, 2026-10-30: GlRaster stays the default; host GL (with its self-check) is opt-in only, no banded work.** Single-context
  host GL took zoom frames to 95 ms against the 87 ms budget; four-band host GL fixed the time (37-41 ms) with equal pixels but hung
  twice inside opengl32 (U-1819). Mariano chose to keep GlRaster as the default.
- **G14, S9 network play, 2026-10-29: close S9 as "the original has no network play".** The net layer is dormant in v1.00 and v1.03 (no
  session store, DirectPlay never loaded, no dialogs; re/analysis/net/NET_MODEL.md). Merge the survey, port the dormant layer as
  specification (S9.3, judged by its C3 rows), drop the recording / interop steps.
- **G13 (final), 2026-10-29: Windows GL default with a startup self-check.** Full census: GlRaster PASS 100 / FAIL 537 / UNSUPPORTED 51; host GL (opengl32) PASS 612 / FAIL 61 / UNSUPPORTED 15, 0 PASS lost. Host GL becomes the Windows default; at startup the app draws a small reference terrain pass and falls back to GlRaster if it differs (or without a 555 format); proven on LIVING-PC only (U-1810). Earlier the same day: **decide after the full census.** GlRaster stays the default, wgl opt-in, until S3.15 has both full-census counts; then ask again. S3.15 found the census HUD / bottom_panel / view one-level residue is
  all GlRaster's leftover rasteriser difference (U-1392; no simple rule closes it). Windows' generic opengl32 on the original's 16-bit
  555 DIB is pixel-exact (sandbox_basic sandbox_00 / sandbox_40: 0 px in every region, I14 had 125). Options: keep GlRaster the
  default with wgl opt-in (S3 lead's recommendation until the full census numbers are in; one renderer for every host, S5 GDI /
  FreeType precedent), or wgl default on Windows and GlRaster elsewhere. wgl is built opt-in either way (one-line switch later).

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
