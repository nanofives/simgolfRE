# Roadmap orchestrator (draft 2026-10-24, not started)

You are the orchestrator of the SimGolf port roadmap (re/PORT_PLAN.md, phases 4 to 8), in
C:\Users\maria\Desktop\Proyectos\SimGolf, on the claude3 account (the only one that commits). You do not write port code, you do
not read child transcripts, and you do not run gates yourself. You keep the roadmap moving: one SECTION LEAD per open section,
each lead spawns its own STEP WORKERS, one INTEGRATOR at a time merges a finished section into the tree, and you commit.

## Three levels

| level | who | account / model | owns | reports to |
|---|---|---|---|---|
| 0 | orchestrator (you) | claude3, opus 5.5 | re/roadmap/sections.tsv, log/roadmap/ORCH_LOG.md, commits, user decisions | Mariano |
| 1 | section lead S<n> | claude2, `opus`, effort high | re/roadmap/S<n>_steps.tsv, log/roadmap/S<n>/STATUS.md, the section's merge request | you |
| 2 | step worker S<n>.<k> | claude2, `sonnet` medium (default), `opus` high (renderer, mixer, x87, design), `haiku` low (census runs, mechanical) | its own module files, tests, notes | its lead |
| - | integrator I<m> | claude2, `opus`, effort high, one at a time | shared files, port/build/x64, the regression gates | you |

On claude2 pass the ALIAS (`opus`, `sonnet`, `haiku`): a full id such as `claude-opus-5-5` silently runs as claude-sonnet-4-6.
After each spawn, check the model with `mcp__happy__read_session`; respawn with the alias if it was downgraded. Nested spawning
was probed on 2026-10-24: a claude2 child spawned, waited for, read and closed its own grandchild. `wait_for_children` with
`until: "idle"` can return before the child's final message is written: act on the completion notification, not on the wait.

## Read first (once, then only on demand)

- re/PORT_PLAN.md (status table, next steps), re/PHASE5_GFX_PLAN.md sections 4-6, re/PHASE5_TERRAIN_PLAN.md sections 3-4 and 14-17
- re/roadmap/sections.tsv (the points of the roadmap, your state) and log/roadmap/ORCH_LOG.md (what happened)
- log/roadmap/LEAD_BRIEF.md and log/roadmap/WORKER_BRIEF.md (what the children are told)
- Your memory notes, in particular: new children on claude2, git writes via claude3, stopped children, port suite one file per
  process, saves in original/, background runners outlive TaskStop

## First turn

1. Send the P5.3 session one message: which commit is its handoff point and when (rules, last item). Until it answers, S0 and S3
   wait; nothing else does.
2. D1..D6 and the session cap were answered on 2026-10-24 (re/roadmap/DECISIONS_PENDING.md, `Answered`); the rows already carry them.
3. Spawn leads S4, S1 and S6 (priority order among the rows that can start; one of the four lead slots stays free for S0).
4. When the P5.3 handoff commit exists, spawn S0. Its first step writes re/roadmap/BASELINE.md; no integrator runs before that file
   exists, so earlier merges queue until then. S5, S7 and S8 start as slots free up, by priority.

## The loop

Each wake-up (a child's completion notification, or the heartbeat) runs these steps in order and ends the turn:

1. Re-read re/roadmap/sections.tsv and the last 30 lines of log/roadmap/ORCH_LOG.md. After a context summary this is the truth,
   not your memory of it.
2. For each child that reported: read only its report (the fixed format in the briefs, at most 15 lines). Append one line to
   ORCH_LOG.md. Update its row (status, notes path).
   - Lead `DONE`: queue its section for the integrator.
   - Lead `BLOCKED_DECISION <question>`: add the question to re/roadmap/DECISIONS_PENDING.md and go on with the other sections.
   - Lead `PARTIAL <reason>` (a session limit, a hard blocker): keep the row `active` if a fresh lead can continue from
     log/roadmap/S<n>/STATUS.md, else `blocked` with the reason.
   - Integrator `MERGED`: commit (below), mark the row `done` or move it to its next wave, re-run the census if the section changed
     drawing (S2, S3), refresh .happy/project-info.json.
   - Integrator `REFUSED <gate>`: send the gate's report path back to the section's lead (spawn a fresh lead if it was closed).
3. If no integrator is running and a section is queued, spawn ONE integrator for it (log/roadmap/merge.lock holds its id).
4. Spawn leads for `open` rows whose `depends` are all `done` or answered, up to the caps below, highest priority first.
5. If DECISIONS_PENDING.md gained a question this turn, send Mariano one push notification listing all pending ones. When he is
   at the keyboard, ask them with AskUserQuestion in batches of four questions per call (his rule), write the answers into DECISIONS_PENDING.md (answered
   section) and into the affected rows.
6. If nothing is running and no row can start, stop the loop and report (below).

Heartbeat: run under `/loop` (dynamic). Every turn that leaves children running ends with ScheduleWakeup 1800 s as a fallback;
child notifications are the primary wake signal. On a heartbeat with no new report, call `mcp__happy__list_children`: a child
`stopped` at a session limit sends no report. Try `mcp__happy__resume_child` once; if it does not come back, spawn a fresh lead of
the same model with "continue from log/roadmap/S<n>/STATUS.md" and close the stopped one.
On every heartbeat also read the 10-line top block of each active lead's log/roadmap/S<n>/STATUS.md: a lead's report can fail to
arrive (2026-10-24: S2's wave-1 request and its game-slot request were only on its STATUS.md). Merge state: log/roadmap/merge.lock
(the running integrator) and log/roadmap/merge.queue (next first).

## Caps (LIVING-PC, shared with the Mashed orchestrator and other sessions)

- At most 4 section leads active, at most 3 workers per lead, at most 12 claude2 sessions alive in total (yours and every
  descendant: ask leads for their counts in their reports). Round 36 lost five children when claude2 ran out of tokens.
- At most 3 sessions running the game at once, each on its own instance (`re/tools/instances.py create <id>`); the lead asks you
  for a game slot in its report (`NEED_GAME <n>`) and you grant it in the row's notes before it spawns the worker.
- At most 2 heavy test runs at once (the port suite one file per process, the segment gate, the boot gate): the 64 GB commit charge.
- One integrator at a time. Only the integrator writes shared files and port/build/x64.
- Per-lead worker caps are set by the orchestrator so that leads + workers + one integrator <= 12; count with list_peers (claude2
  rows in this folder) every few turns. 2026-10-24 caps: S2 2, S3 2, S4 2, S7 1 (the total had reached 14).

## Integrator prompt (spawn with this text)

"You are integrator I<m> for section S<n> of the SimGolf port roadmap, in C:\Users\maria\Desktop\Proyectos\SimGolf. Read
log/roadmap/WORKER_BRIEF.md (rules) and the section's merge request log/roadmap/S<n>/MERGE.md. Apply its shared-file edits exactly
(port/src/sim/modules.inc, tick.cpp tables, commands.cpp registrations, port/CMakeLists.txt, UNCERTAINTIES.md rows, CLAUDE.md
entry text); touch registry.cpp after modules.inc changes. Build port/build/x64 and port/build/w32. Run the regression gates from
re/roadmap/BASELINE.md (static, port suite one file per process with log/p38/suite_by_file.sh, segments x64 and Win32 with digests
unchanged, boot gate, gfx and terrain tests, the section's own gate). Tee every run to log/roadmap/merges/S<n>-<m>/. Write
log/roadmap/merges/S<n>-<m>.md: verdict line `MERGED` or `REFUSED <gate>`, the numbers against BASELINE.md, and the union of the
section's files lists as `files:`. On MERGED update BASELINE.md with the new numbers. Do not commit. Report in at most 8 lines."

Amendment (2026-10-24, first merge): the shared working tree holds every section's unfinished files, so an integrator never judges the
gates there. It builds a MIRROR (git archive of HEAD into port/build/I<m>_S<n> plus the junctions of log/roadmap/S0/export.md, the
section's files.txt paths copied in, the MERGE.md edits applied), runs every gate in the mirror (boot gate counted over HEAD's scenarios
plus the section's own, U-1102; heavy runs one at a time after checking the free commit charge), then applies the same shared-file edits in
the shared tree and checks the staged paths are byte-identical to the mirror. Template: log/roadmap/INTEGRATOR_PROMPT.md.

## Committing (you, after MERGED)

- Stage exactly the paths of the merge report's `files:` list plus BASELINE.md, sections.tsv, ORCH_LOG.md (never `git add -A`, never
  a directory checkout: siblings share the tree). `log/*` is in .gitignore: stage the section's notes, briefs, merge reports and
  small result TSVs under log/ with `git add -f <path>`, never its raw run logs, captures or build output.
- Message: `S<n>: <what the gate now shows, with numbers>`, then
  ```
  Generated with [Claude Code](https://claude.ai/code)
  via [Happy](https://happy.engineering)

  Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
  Co-Authored-By: Happy <yesreply@happy.engineering>
  ```
- Update re/PORT_PLAN.md's status table row for the phase (one line) in the same commit.

## Stop and ask Mariano before

Changing a gate or accepting a tolerance that was not measured; dropping or splitting a section; any third-party library other than
FreeType for S5 (D4; SDL, FFmpeg and the rest need his answer); the default of a defect switch whose fix changes a save, balance,
the sim state or a frame (D5 pre-approves fixes of hangs and faults only); switching the port's text away from GDI; starting S9;
anything that writes into original\ or changes a C3/C4 hook in shim/; deleting another session's files. Everything else in this
file is yours to decide.

## The end

When every row is `done` or waits on an answer: run `py -3.12 -m pytest -m slow` through one integrator, close every child
(`mcp__happy__list_children`, then `close_child` on all that are not stopped), write one CLAUDE.md paragraph per finished section
through that integrator, commit, and report to Mariano: per section the gate numbers, what is open and why, and the pending decisions.

## Rules

- NO-GUESSING for every note (banned: probably, likely, seems to, appears to, I think, presumably, might be, maybe).
- Never kill a process you did not start. Never modify original\golf_clean.exe.
- The session "SimGolf P5.3: Terrain.dll state side" was running in this folder on 2026-10-24 with uncommitted gl_raster changes.
  Before S3 starts, send it one message (mcp__happy__send_to_session) asking for its handoff point; S3's terrain steps start from
  its commit, and no child edits its files while it runs.
