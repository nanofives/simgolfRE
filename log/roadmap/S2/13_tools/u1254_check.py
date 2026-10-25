"""S2.13: U-1254 check on cov_r_s2_screens: the boot with the fix, with negctl showEndOfYearScreen mode 6 (the loop's
View_toLocal read left out), and gfx.check()'s duplicate count.

usage (from a mirror root, SG_PORTSIM_DLL set): ~/bin/py -3.12 <this> [--backend win32]
"""
import json
import os
import sys

sys.path.insert(0, os.getcwd())
from port.test import boot, lib as L  # noqa: E402


def main():
    lib = L.load()
    out = {}
    for label, mode in (("fix", 0), ("negctl6", 6)):
        prev = lib.negctl("showEndOfYearScreen", mode)
        try:
            r = boot.run("cov_r_s2_screens", lib)
        finally:
            lib.negctl("showEndOfYearScreen", prev or 0)
        out[label] = {"status": r.status, "records": r.records, "expected": r.expected, "detail": r.detail,
                      "first_divergence": r.first_divergence, "run_detail": r.run_detail}
        print(f"[{label}] {r.status} {r.records}/{r.expected} {r.detail}", flush=True)
        print(f"    run_detail: {r.run_detail}", flush=True)
    with lib.state() as st:
        st.set_data_base(boot.SG.DATA_VA)
        st.call("gfx.attach", [])
        dup = st.call("gfx.check", []).eax
        txt = st.files_read("$gfx/bodies.txt").decode()
    out["duplicates"] = dup
    print(f"gfx.check duplicates: {dup}", flush=True)
    for line in txt.splitlines():
        if line.startswith("duplicate"):
            print("   ", line)
    for n in ("Surface_drawSprite", "Widget_fillRect", "Widget_draw\t", "drawStringAt\t", "drawStringAt2"):
        print("   ", [l for l in txt.splitlines() if l.startswith(n)])
    print(json.dumps(out)[:2000])


if __name__ == "__main__":
    main()
