"""S3.14: the Win32 suite (SG_BACKEND=win32, I7's 59 files log/roadmap/merges/I7/win32_files.txt, results in
log/roadmap/S3/14_suite_win32/<file>.txt) judged against the known failures log/roadmap/merges/I10/win32_failed_ids.txt (the 20
known + test_fttext's 2, U-1393). A file's re-run alone (14_suite_win32_alone/<file>.txt), when present, replaces its result.

    ~/bin/py -3.12 log/roadmap/S3/14_win32_compare.py
Prints one line per file and the totals: NEW failures (not in the known list), FIXED (known ids that passed), missing results."""
import pathlib
import re
import sys

S3 = pathlib.Path(__file__).resolve().parent
ROOT = S3.parents[2]
files = [pathlib.Path(l.strip()).stem for l in (ROOT / "log/roadmap/merges/I7/win32_files.txt").read_text().splitlines() if l.strip()]
known = {l.strip() for l in (ROOT / "log/roadmap/merges/I10/win32_failed_ids.txt").read_text().splitlines() if l.strip()}
new, seen_failed, missing, n_ok = [], set(), [], 0
for f in files:
    src = S3 / "14_suite_win32_alone" / f"{f}.txt"
    tag = "alone"
    if not src.exists():
        src, tag = S3 / "14_suite_win32" / f"{f}.txt", "suite"
    if not src.exists():
        missing.append(f)
        print(f"MISS {f}")
        continue
    t = src.read_text(encoding="utf-8", errors="replace")
    last = (t.strip().splitlines() or [""])[-1]
    failed = {m.group(0).split(" - ")[0].strip() for m in re.finditer(r"^(FAILED|ERROR) port/test/\S+", t, re.M)}
    failed = {x.replace("ERROR ", "FAILED ") for x in failed}
    seen_failed |= failed
    nf = sorted(failed - known)
    kf = sorted(failed & known)
    new += nf
    ok = not nf and (" passed" in last or " skipped" in last or "no tests ran" in last) and "error" not in last.split("in ")[0]
    n_ok += ok
    print(f"{'ok  ' if ok else 'FAIL'} {f:30s} [{tag}] known {len(kf)} new {len(nf)} :: {last}")
    for x in nf:
        print(f"      NEW {x}")
known_in_list = {k for k in known if pathlib.Path(k.split("::")[0].split()[-1]).stem in set(files) - set(missing)}
fixed = sorted(known_in_list - seen_failed)
print(f"files {len(files)}, judged ok {n_ok}, missing {len(missing)}; failed ids {len(seen_failed)}, "
      f"known {len(seen_failed & known)} of {len(known_in_list)}, NEW {len(new)}, known that passed {len(fixed)}")
for x in fixed:
    print(f"  passed (known) {x}")
sys.exit(1 if new or missing else 0)
