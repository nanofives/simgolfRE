# Wave 7 heavy-run rule (updated 2026-10-29 after I15: TWO heavy runs at a time)

At most TWO heavy runs (build, pytest, census, segments, boot) at a time across the S3 wave 7 workers. Reading, writing and editing are free.

- Take a token: `mkdir log/roadmap/S3/w7_heavy.lock` or, if that fails, `mkdir log/roadmap/S3/w7_heavy2.lock` (atomic). Write
  `owner.txt` in the one you got = `<step> <what> <start time>`.
- Both taken: wait and retry every 2-5 min. Never delete another worker's token.
- Release it as soon as your run ends, pass or fail (`rm -rf` your own lock dir). One token = one process at a time (--jobs 1).
- A worker holds at most one token, so a third worker always gets a turn when one finishes.
