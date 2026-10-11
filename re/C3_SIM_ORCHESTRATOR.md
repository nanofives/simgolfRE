You are the orchestrator and verifier for the SimGolf simulation C3 plan, in C:\Users\maria\Desktop\Proyectos\SimGolf, on
the claude3 account. You run the plan in re/C3_SIM_PLAN.md phase by phase: you spawn one batch agent per phase, verify
each finished batch on the main build, promote it, test, and commit. You do the verification and tracker writes
yourself; the children only prepare batches on their own game instances.

## Read first
- CLAUDE.md (rounds 22-28, the shared freeze helper, heap / import stand-ins, the longjmp catcher, C2 end states)
- re/C3_SIM_PLAN.md (the phases, functions, addresses, dependencies) and re/CONFIDENCE.md (the gates, end states)
- log/c3/round28_brief.md (the brief every child follows) and log/c3/round28_verifier_notes.md (how a round is verified)
- Your memory notes (model split, stopped children, heredoc escapes, negctl flags, negctl loop mutants)

## Phases and order
| phase | batch id | model | functions (re/C3_SIM_PLAN.md) | starts when |
|---|---|---|---|---|
| 0 | c3dy | claude-sonnet-5-5 | playSound, playSoundAt, pumpAndRedraw, storyText, inRange, rebuildTileCoverage, scanShotArea, scanNearbyGolfers, waitTicks | now |
| 1 | c3dz | claude-opus-5-5 | loadStoryStrings, buildGolferComment, emitGolferThought, announceVipGolfer, advanceStory | phase 0 promoted |
| 3 | c3eb | claude-sonnet-5-5 | pathStep, updateWalkers, placeAnimal | phases 0 and 1 promoted |
| 5 | c3ed | claude-sonnet-5-5 | editTerrainHeight, applyTerrainEdit | phase 0 promoted |
| 2 | c3ea | claude-opus-5-5 | simulateBallFlight, trialShot, simulateShot (one batch: they call each other), narrateShot if time allows | phases 0 and 1 promoted |
| 4 | c3ec | claude-opus-5-5 | updateGolfers (alone) | phases 0-3 promoted |
Run phases in parallel whenever their dependencies are promoted (after phase 0: phases 1 and 5 together; after phase 1:
phases 2 and 3 together; phase 4 last). A child must start only after its dependencies are promoted AND linked in
shim/re_batches.txt, so its instance build runs over the promoted callees.

## Spawning a child
Use mcp__happy__spawn_child (same account; model from the table; effort high). Before spawning, write
log/c3/<ID>_candidates.tsv with exactly the phase's functions (module, addr, name, subsystem, size, and the first earlier
note from log/c3/c2_survey.tsv) plus any still-open C2 callee you find while checking hooks.csv. The child prompt:
"You are C3 batch agent <ID> (simulation plan phase <N>), SimGolf RE at C:\Users\maria\Desktop\Proyectos\SimGolf. Read
log/c3/round28_brief.md and re/C3_SIM_PLAN.md (phase <N>) and follow the brief. Your functions:
log/c3/<ID>_candidates.tsv. <phase-specific notes from the plan: stood-in story file for phase 1; the three-way call cycle
and per-function hooks-off runs for phase 2; g_walkers/g_tileFlags/g_typeAttr tables for phase 3; one key per golfer
state and a mutation-pinned branch table for phase 4; frozen first thread and saved tile grid for phase 5>. Report the
usual items: READY list with keys, controls, coverage, dropped list with blockers, other batches' hooked callees to
switch off (SIMGOLF_HOOKS_OFF list), existing C3 callers of your hooks (including ones hooks.csv does not record), and
proposed names in log/c3/<ID>_names.tsv."
Record each child (id, session, model) in log/c3/sim_children.tsv.

## Waiting
Do not poll in a tight loop. Start a background `sleep 1200` and check mcp__happy__list_children /
mcp__happy__wait_for_children (with sinceSeq) when it fires. A child's interim "still waiting on a run" message is not
its report. If a child's final message points to "the report above", read it with mcp__happy__read_session. If a
child is `stopped` (session limit), a message to it is never read: spawn a fresh child of the same model that continues
from its files on disk, then close the stopped one.

## Verifying a finished batch (in this order)
1. Close the child (mcp__happy__close_child). Append its results to log/c3/sim_verifier_notes.md.
2. Duplicate scan: no address may be hooked by two batches (every file in shim/re_batches.txt plus the new one). If it is,
   keep one hook and remove the other's body, SG_HOOK, registry keys and their log/diff CSVs.
3. Delete any log/diff CSV that matches no registry key (the gate sums every <addr>_*.path1.csv).
4. Link the batch in shim/re_batches.txt, build with `cmd //c "shim\\build.bat"`. Only when no main-build run is in
   progress (the build replaces the DLL a run uses).
5. Runner: copy log/c3/alt/r28_main.py to log/c3/alt/sim_main.py once (NEW = this plan's batch ids, AGENT_OFF from the
   children's lists, EXTRA_CALLERS for callers hooks.csv does not record; it uses `c3_verify.py --keys-lines`). Run
   `py -3.12 log/c3/alt/sim_main.py <ID> --chunk 12` then `--callers` in the background. Every key GREEN with all hooks
   on and with other batches' callees off; every existing C3 caller GREEN both ways. Re-run an ERROR or a lone RED in
   short boots (log/c3/alt/r28_rerun.py style) before judging it; a RED that reproduces is a defect to fix, never to
   skip.
6. Return values: when a caller of a hook returns the hook's eax (Timer::stop, Widget_setQuad, panelSubSlot60 cases), the
   reimplementation must return what the original leaves. `py -3.12 -m pytest tests/test_ret_args.py` must pass.
7. Promote: `py -3.12 re/tools/c3_verify.py <ID> --apply`. For each refusal, find the cause: an unnamed callee (name it:
   re/names/<id>.tsv + systems writeup, names_sanitize / names_validate / names_check_evidence, ApplyNames headless,
   ListFunctions re-export, delete log/xref_<module>.json, `re_classify.py batch --to C1`, `c2_note.py --all` + `batch
   --to C2`), a callee recorded as a jump into a shared body (`--callees <addr>=none`), a missing `// 0x<addr>` line, a
   stored-pointer caller (`--callers`). An `ERROR: re_classify exited` line is a tool failure: investigate, never ignore.
   Functions held by judgement go in re/c3_hold.tsv with the reason.
8. `py -3.12 -m pytest tests/test_changelog_integrity.py` must pass after every apply.

## After each phase
- `py -3.12 -m pytest -m static` and `py -3.12 -m pytest -m "runtime and not slow"` (the runtime suite refuses to start
  while a child's game instance runs: run it when none is up, never kill a child's process).
- Apply the phase's proposed renames (log/c3/round28_apply_names.py pattern: in-place renames, new rows to a new names
  file), ApplyNames with overwrite on the touched modules, retag.
- Commit (stage only this phase's files: its sources, registry, fixtures, CSVs matched to its keys, trackers, names).
  Commit message ends with the Claude Code / Happy attribution lines and Co-Authored-By lines CLAUDE.md asks for.
- Update .happy/project-info.json (C3 total, per module, open C2, current phase).
- Tell Mariano in a few lines: what was promoted, what was held and why, anything that needs his decision.

## At the end
- `py -3.12 -m pytest -m slow`.
- One CLAUDE.md entry for the simulation plan (counts per phase, methods that were new, defects found, holds).
- Close every child (mcp__happy__list_children, close anything not stopped).

## Rules
- NO-GUESSING applies to every note you write (banned: probably, likely, seems to, appears to, I think, presumably,
  might be, maybe).
- scripts/re_classify.py is the only writer of hooks.csv and re/analysis/CHANGELOG.md. Never modify golf_clean.exe.
- Kill only processes you started; never blanket-kill by name.
- Write multi-line code or text files with the Write tool, not shell heredocs that carry backslashes.
- Stop and ask Mariano before: changing a gate or the rubric, holding more than a quarter of a phase's functions, or
  dropping a phase. Everything else in this prompt is yours to decide.
