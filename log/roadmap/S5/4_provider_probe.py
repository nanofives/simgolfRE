"""S5.4 control for the selection: the patched SegmentRunner really attaches the provider SG_TEXT_METRICS names.
Runs a few segments that call Text_put under (provider, fttext negctl): freetype, freetype + advance +1 (RED expected),
gdi + the same negctl (must stay as gdi), none (no provider: the Text_put segments stop).

    S5_4_PATCH=1 PYTHONPATH=log/roadmap/S5/4_site SG_PORTSIM_DLL=port/build/S5_4/Release/portsim_c.dll ~/bin/py -3.12 log/roadmap/S5/4_provider_probe.py
"""
import os
import sys

from port.test import lib as L, segments as S

assert hasattr(S, "text_provider"), "run with S5_4_PATCH=1 and PYTHONPATH=log/roadmap/S5/4_site"
lib = L.load()
rows = {(r["scenario"], int(r["segment"])): r for r in S.manifest_rows()}
picks = [k for k in [("ui_tour", 0), ("ui_tour", 2), ("championship", 0), ("cov_a_play", 0)] if k in rows]
print("providers:", {m: S.text_provider(m, lib) for m in ("gdi", "freetype", "none", "x")})
for mode, neg in [("freetype", 0), ("freetype", 1), ("freetype", 2), ("freetype", 3), ("gdi", 2), ("none", 0)]:
    prev = lib.negctl("fttext.fontWidth", neg) if neg else None
    try:
        runner = S.SegmentRunner(lib, text_metrics=mode)
        out = []
        for k in picks:
            seg = S.load_segment(S.segment_dir(rows[k]))
            r = runner.run(seg)
            out.append(f"{k[0]}/{k[1]:02d} {r.status}")
        print(f"{mode:9s} negctl {neg}: " + "; ".join(out), flush=True)
    finally:
        if neg:
            lib.negctl("fttext.fontWidth", prev or 0)
