# Decisions pending (Mariano)

Open questions the orchestrator batches (AskUserQuestion, four per call). Each has the sections it blocks. Answers go in the
`Answered` part with the date.

## Open

(none)

## Answered

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
