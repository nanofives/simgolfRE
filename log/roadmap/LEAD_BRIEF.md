# Section lead brief (roadmap orchestrator, re/ROADMAP_ORCHESTRATOR.md)

You lead one section S<n> of the SimGolf port roadmap. Your row in re/roadmap/sections.tsv states its gate. You plan the section
as steps, spawn one step worker per step, judge their results, and hand the orchestrator one merge request. You write no port code
yourself beyond small fixes to make your workers' pieces fit; your context is for judging, not for building.

## Read first
SimGolf/CLAUDE.md (the port sections from "PORT PIVOT" on), re/PORT_PLAN.md, the plan files your row names, port/CORE.md,
port/test/README.md, log/roadmap/WORKER_BRIEF.md (your workers' rules apply to you too), and your section's STATUS.md if it exists
(you may be continuing a stopped lead: then start from it, do not redo finished steps).

## 1. Plan the steps
Write re/roadmap/S<n>_steps.tsv: `step  title  depends  model  effort  needs_game  files_owned  gate  status  session  notes`.
- A step is one worker's job: one module or one screen family, one gate, finishable in one session (under ~150 k tokens of work).
  Split anything larger.
- `files_owned` lists the files only that worker may create or edit. Two steps never own the same file. Shared files (WORKER_BRIEF.md
  list) belong to nobody: changes to them go in merge notes.
- S2 and S3: the steps are the census causes in re/roadmap/frame_census.tsv, largest number of blocked checkpoints first.
- Uncertainty ids: your section's range from sections.tsv, ten per step (S<n>.1 gets the first ten, and so on).
Write log/roadmap/S<n>/STATUS.md: a 10-line top block (step counts by status, live workers, game slots held, last event) that you keep
current after every event. The orchestrator reads only that block.

## 2. Run the workers
- Spawn with mcp__happy__spawn_child, `account: "claude2"`, the alias model and effort from the step row, prompt:
  "You are step worker S<n>.<k> of the SimGolf port roadmap, in C:\Users\maria\Desktop\Proyectos\SimGolf. Read
  log/roadmap/WORKER_BRIEF.md and follow it. Your step: row S<n>.<k> of re/roadmap/S<n>_steps.tsv. <two or three lines of
  step-specific facts and file paths>. Uncertainty ids U-<a>..U-<b>."
  Check the model with read_session after spawning; respawn with the alias if it was downgraded.
- At most 3 workers alive at once. A step that runs the game needs a slot: ask the orchestrator (`NEED_GAME` in a report line, see
  below) and wait for the grant in your sections.tsv row before spawning it.
- Wait on completion notifications. Heartbeat: if no worker reported for 30 minutes, mcp__happy__list_children; a `stopped` worker
  (session limit) gets mcp__happy__resume_child once, else a fresh worker "continuing from log/roadmap/S<n>/<k>_notes.md", and the
  stopped one is closed.
- Close each worker (mcp__happy__close_child) once you have read its report and its files exist.

## 3. Judge each step
Before you accept a step, check on disk (not from the report): the step's gate output file, the negative controls' RED lines, that
the worker touched only its `files_owned` (compare its files list with the row), and that its notes use no banned word. Re-run its
gate yourself in a private mirror build when two steps' files meet (log/p3/p37b/mirror.py pattern: HEAD plus your section's files
plus your merge notes applied in the mirror, port/build/S<n>_mirror). A step that fails goes back to a fresh worker with the reason.

## 4. Hand over
When every step is accepted (or held with a measured reason that the gate allows):
- log/roadmap/S<n>/MERGE.md: every shared-file edit from the workers' merge notes, de-duplicated and in apply order, with exact text
  and location; the CLAUDE.md paragraph for the section (facts, numbers, files, findings in the original, U-rows); the section gate
  command lines and their expected numbers.
- log/roadmap/S<n>/files.txt: the union of the workers' files lists.
- Close every child you spawned (list_children; close all that are not stopped).
Do not commit, do not edit shared files, do not build into port/build/x64.

## Reports to the orchestrator (your final message of a turn; at most 15 lines, this format)
```
S<n> <DONE | PARTIAL <reason> | BLOCKED_DECISION <question> | NEED_GAME <count> | PROGRESS>
steps: <accepted>/<total> accepted, <held> held, <live> workers alive (claude2 sessions under you: <n>)
gate: <the section gate's numbers now>
files: log/roadmap/S<n>/files.txt   merge: log/roadmap/S<n>/MERGE.md
open: <one line per held step or open U-row, with the reason>
```
Send PROGRESS only when you need nothing (it costs the orchestrator context); send a report when a decision, a slot or the merge is
needed, or when your session approaches its limit (then make STATUS.md complete enough for a fresh lead first).

## Decisions are not yours when
A gate or tolerance would change, a step would be dropped, a third-party library would enter the port, a defect's default would
change, a C3 / C4 hook in shim/ would change, or original\ would be written. Report BLOCKED_DECISION and keep the other steps going.
