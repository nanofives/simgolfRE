"""S7.9: which present the app tests' `present 0` is, and where the replay's snapshot of it comes from. One app session to the
title menu (test_app_live's Session / _to_title, the test's own standalone build port/build/S7_6_app), then: the first presents'
rows, and the replay's snapshot hash at pos - 2 .. pos + 2 for present 0. Run from the mirror root:
    ~/bin/py -3.12 <this> [--keep <dir>]"""
import os
import shutil
import sys

sys.path.insert(0, os.getcwd())
from port.test import test_app_live as T  # noqa: E402

keep = sys.argv[sys.argv.index("--keep") + 1] if "--keep" in sys.argv else None
exe = T.exe.__wrapped__() if hasattr(T.exe, "__wrapped__") else None
if exe is None:
    exe = T.EXE
    os.environ["SG_PORTSIM_DLL"] = str(T.DLL)
s = T.Session(exe, lambda app, s: T._to_title(app, s))
try:
    print("presents", len(s.presents), "records", len(s.records()))
    for p in s.presents[:6]:
        print("present", p)
    p0 = s.presents[0]
    cand = [p0["pos"] + d for d in (-2, -1, 0, 1, 2) if p0["pos"] + d >= 0]
    r = T.replay(s, cand)
    for c, h in zip(cand, r["snapshots"]):
        print(f"replay snapshot at {c}: {h if h is None else hex(h)}  {'== present 0' if h == p0['hash'] else ''}")
    for p in s.presents[1:4]:
        r1 = T.replay(s, [p["pos"]])
        print(f"present {p['index']} pos {p['pos']} kind {p['kind']}: replay {'equal' if r1['snapshots'][0] == p['hash'] else 'DIFFERS'}")
    print("session", {k: s.session["session"].get(k) for k in ("presents_by_kind", "present_points", "r_superseded")})
    if keep:
        shutil.copytree(s.rec, keep, dirs_exist_ok=True)
finally:
    s.cleanup()
