# Wave 8 heavy-run rule (2026-10-30; I20 holds the other heavy slot)

ONE heavy run (build, pytest file, census, segments, boot, glreplay batch) at a time across ALL S3 wave 8 workers.

- Take the lock: `mkdir log/roadmap/S3/w8_heavy.lock` (atomic; fails if held), write `owner.txt` in it = `<step> <what> <start time>`.
- Before starting, check free commit charge >= 16 GB:
  `~/bin/py -3.12 log/roadmap/S3/w8_commit_free.py` prints the free commit in GB and exits 1 below 16. Below 16 GB: release
  the lock, wait 5 min, retry.
- Release it as soon as the run ends (`rm -rf` the lock dir after reading owner.txt names you). Wait 2 min before re-taking.
- --jobs 1 always. Reading, editing and small Python analyses (no build, no pytest, no framegate) are free.
- Never touch port/build/I20 or another section's files in the shared tree.

## Fair turns (added 2026-10-30 21:50 after S3.23 waited 70 min)
- A worker that WANTS the lock and does not hold it creates `log/roadmap/S3/w8_wait_<step>` (e.g. w8_wait_S3.23) with the time.
- A worker that releases the lock may NOT take it again while a `w8_wait_<other step>` file exists: it waits until that file is gone.
- The waiting worker deletes its own `w8_wait_<step>` file right after it gets the lock.
- One lock hold = one bounded run (one build, one pytest file, or one census batch of a few scenarios), not a chain.
- ORDER (added 23:05): the OLDEST wait file goes next (time written inside it). Only the worker whose wait file is oldest may take a
  free lock; a worker that has just released must create a new wait file and queue behind every existing one before it re-takes.

## ENFORCED (2026-10-31 00:50, orchestrator): take the lock ONLY through w8_lock.py

From each worker's next hold on, nobody runs `mkdir w8_heavy.lock` or writes a wait file by hand.

    ~/bin/py -3.12 log/roadmap/S3/w8_lock.py run <step> --what "<run>" -- <command ...>   # take, run, release always
    ~/bin/py -3.12 log/roadmap/S3/w8_lock.py take <step> --what "<run>"                    # blocks; exit 0 = you hold it
    ~/bin/py -3.12 log/roadmap/S3/w8_lock.py release <step>
    ~/bin/py -3.12 log/roadmap/S3/w8_lock.py status

`take` writes w8_wait_<step> (epoch + its pid), blocks until that file is the oldest live wait file, the lock is
free and >= 16 GB commit is free, then takes the lock and removes the wait file. A wait file of a dead take is
removed; hand-written ones count from their mtime. `release` refuses a lock another step holds. Prefer `run`:
it releases on error and Ctrl-C. Re-queueing after a release puts you behind every existing waiter by construction.
Scripts that loop over holds call `run` once per hold (one hold = one bounded run).

## Priority takes (2026-10-31 02:20)

`w8_lock.py take <step> --priority` gives the wait file epoch 0, so it ranks before every waiter (the running hold
still finishes). Lead / orchestrator only. First use: step I20-rerun, the quiet window for I20's re-runs (watcher
w8_i20_window.py, PID 11080, log w8_i20_window.log): from `[queue done]` to `[rerun done]` in
log/roadmap/merges/I20/g_gates.log no S3 heavy hold starts; your `take` simply waits longer.

## Priority alternation (2026-10-31 11:00)

`--priority` no longer starves ordinary steps: `release` records the releasing step in w8_last_release.txt, and a `--priority` take by
the step that made the LAST hold queues right behind the oldest other waiter instead of ahead of everyone. So a priority step (I21) and
the others alternate one hold each; with nobody else waiting it goes straight on. Tested on a scratch copy (P holds, W waits, P
re-queues with priority: W takes next, then P).

## One file per hold (orchestrator, 2026-10-31 11:20)

A hold is ONE pytest file, ONE census batch, ONE segments or boot step. A `run` whose command loops over several pytest files is a
breach (S3.28's pytest_x64 hold, 29+ min, and the lead's own w8_gates.sh pytest_x64 / pytest_w32 steps had this shape). Scripts call
`w8_lock.py run` once per file.

Note (2026-10-31): `w8_lock.py run <step> -- <cmd>` starts <cmd> with CreateProcess, so `~/bin/py` (a shell script) fails there with WinError 193; wrap it: `-- bash -c "~/bin/py -3.12 ..."`. Killing a `run` process after it took the lock leaves the lock held: release it with `w8_lock.py release <step>` and kill its child by PID.
