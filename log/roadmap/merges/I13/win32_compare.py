"""I13: the Win32 suite of the mirror port/build/I13 (SG_BACKEND=win32, log/roadmap/merges/I13/win32_files.txt = I7's 59 files plus the
7 test files S3 adds, results in log/roadmap/merges/I13/suite_win32/<file>.txt) judged against the known failures
log/roadmap/merges/I10/win32_failed_ids.txt (BASELINE's 20 + test_fttext's 2, U-1393). Adapted from log/roadmap/S3/14_win32_compare.py.

    ~/bin/py -3.12 log/roadmap/merges/I13/win32_compare.py
One line per file (I7 = in I7's 59-file list, S3 = a file S3 adds), then the totals: NEW failures, known ids that passed, missing results."""
import pathlib
import re
import sys

N = pathlib.Path(__file__).resolve().parent
ROOT = N.parents[3]
i7 = {pathlib.Path(l.strip()).stem for l in (ROOT / "log/roadmap/merges/I7/win32_files.txt").read_text().split() if l.strip()}
files = [pathlib.Path(l.strip()).stem for l in (N / "win32_files.txt").read_text().split() if l.strip()]
known = {l.strip() for l in (ROOT / "log/roadmap/merges/I10/win32_failed_ids.txt").read_text().splitlines() if l.strip()}
new, seen_failed, missing, n_ok, n_ok_i7 = [], set(), [], 0, 0
for f in files:
    src = N / "suite_win32" / f"{f}.rerun.txt"   # a file re-run alone replaces its suite result
    if not src.exists():
        src = N / "suite_win32" / f"{f}.txt"
    grp = "I7" if f in i7 else "S3"
    if not src.exists():
        missing.append(f)
        print(f"MISS {f} [{grp}]")
        continue
    t = src.read_text(encoding="utf-8", errors="replace")
    last = ([l for l in t.strip().splitlines() if not l.startswith("rc=")] or [""])[-1]
    failed = {m.group(0).split(" - ")[0].strip() for m in re.finditer(r"^(FAILED|ERROR) port/test/\S+", t, re.M)}
    failed = {x.replace("ERROR ", "FAILED ") for x in failed}
    seen_failed |= failed
    nf = sorted(failed - known)
    kf = sorted(failed & known)
    new += nf
    ok = not nf and (" passed" in last or " skipped" in last or "no tests ran" in last) and "error" not in last.split("in ")[0]
    n_ok += ok
    n_ok_i7 += ok and grp == "I7" and not kf
    print(f"{'ok  ' if ok else 'FAIL'} {f:30s} [{grp}] known {len(kf)} new {len(nf)} :: {last}")
    for x in nf:
        print(f"      NEW {x}")
known_in_list = {k for k in known if pathlib.Path(k.split("::")[0].split()[-1]).stem in set(files) - set(missing)}
fixed = sorted(known_in_list - seen_failed)
print(f"files {len(files)} (I7 {len(i7 & set(files))}, S3 {len(set(files) - i7)}), judged ok {n_ok}, I7 files with no failure {n_ok_i7}, "
      f"missing {len(missing)}; failed ids {len(seen_failed)}, known {len(seen_failed & known)} of {len(known_in_list)}, "
      f"NEW {len(new)}, known that passed {len(fixed)}")
for x in fixed:
    print(f"  passed (known) {x}")
sys.exit(1 if new or missing else 0)
