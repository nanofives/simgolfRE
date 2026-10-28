"""S3.6b: what the panels' emulator oracle (port/test/test_gfx_hud.py test_panel_calls_equal_the_original) covers, and its
negative controls. Run from port/build/S3_6b_src with SG_PORTSIM_DLL set:

    python ../../../log/roadmap/S3/6b_emu_stats.py [n] [--negctl]

Per panel: cases, original faults, compared, equal, the count of cases holding each call kind, and per hud mode 11..14 the
cases that differ (a control is RED when that count is above 0).
"""
import collections
import os
import sys

sys.path.insert(0, os.getcwd())
from port.test import cmdemu as CE  # noqa: E402
from port.test import lib as L  # noqa: E402
from port.test import test_gfx_hud as T  # noqa: E402


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 120
    lib = L.load()
    if not lib.exe_loaded():
        lib.exe_load()
    emu = CE.Emu()
    for which in ("view", "detail"):
        cases = T._panel_cases(lib, which, n, 0x5E6B + (which == "detail"))
        kinds, faults, eq, comp, errs, lens = collections.Counter(), 0, 0, 0, collections.Counter(), []
        wants, cover = [], {}
        for img, x, y in cases:
            want, r = T._emu_panel_calls(emu, img, which, x, y, cover)
            if r.err:
                faults += 1
                errs[r.err[:60]] += 1
                wants.append(None)
                continue
            got = T._port_panel_calls(lib, img, which, x, y)
            comp += 1
            eq += got == want
            lens.append(len(want))
            for k in {ln.split(" ")[0].split("|")[0] for ln in want}:
                kinds[k] += 1
            texts = [ln for ln in want if ln.startswith("drawCenteredText") or ln.startswith("drawText")]
            if texts:
                kinds["(cases with text)"] += 1
            wants.append(want)
        print(f"{which}: cases {len(cases)} faults {faults} compared {comp} equal {eq} calls/case min {min(lens)} max {max(lens)}")
        print("  kinds:", dict(sorted(kinds.items())))
        nsite, both, missing = CE.coverage_summary(cover)
        print(f"  jcc sites {nsite}, both sides {both}, missing {missing}")
        if errs:
            print("  faults:", dict(errs))
        if "--negctl" in sys.argv:
            for mode in (11, 12, 13, 14):
                prev = lib.negctl("gfx.family.hud", mode)
                diff = sum(1 for (img, x, y), w in zip(cases, wants) if w is not None and T._port_panel_calls(lib, img, which, x, y) != w)
                lib.negctl("gfx.family.hud", prev or 0)
                print(f"  negctl hud={mode}: {diff} cases differ")


if __name__ == "__main__":
    main()
