"""S3.8 stream check: the port's Terrain::drawLine calls in a boot of a pinned scenario (gfx and LiveTerrain attached, the overlay's
call log on) against the drawLine calls of the GL stream the original recorded for that scenario (re/tools/gl_record.py --scenario,
port/corpus/gl/<label>/stream.gl.txt).

The recorder keeps every GL call Terrain.dll makes except the vertex data of glBegin..glEnd blocks, so per drawLine the stream holds
glOrtho (which projection: the flip byte), glLineWidth (the width as a float) and glColor4f (the channels and alpha / 10 as floats);
the ends are not in it. A drawLine call in the stream = a glLineWidth line; the port's = a log line. Compared in order: the count, and
per call the float bits of the width, the colour and the alpha.

    ~/bin/py -3.12 log/roadmap/S3/8_streams.py <label>=<scenario> ... [--out tsv]      e.g. g2=cov_b_holes18 g3=cov_b_paths
"""
from __future__ import annotations

import argparse
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
SRC = ROOT / "port" / "build" / "S3_8_src"
sys.path.insert(0, str(SRC))
from port.test import boot as B  # noqa: E402
from port.test import framegate as FG  # noqa: E402
from port.test import lib as L  # noqa: E402
from port.test import terrainlive as TLm  # noqa: E402

GL = SRC / "port" / "corpus" / "gl"


def stream_calls(label: str) -> list[tuple[str, str, str]]:
    """(ortho, width bits, colour bits) per drawLine in the recorded stream"""
    out, ortho, width = [], "", None
    for ln in (GL / label / "stream.gl.txt").read_text().splitlines():
        if ln.startswith("glOrtho "):
            ortho = ln.split(" ", 1)[1]
        elif ln.startswith("glLineWidth "):
            width = ln.split(" ", 1)[1]
        elif ln.startswith("glColor4f ") and width is not None:
            out.append((ortho, width, ln.split(" ", 1)[1]))
            width = None
    return out


def port_calls(scenario: str, lib=None) -> tuple[list[dict], str]:
    lib = lib or L.load()
    va = FG.terrain_address(scenario)
    if getattr(lib, "dll", None) is not None:
        FG._bind_live(lib.dll)
    got = {}

    def before(st):
        st.call("gfx.attach", [])
        if TLm.live_attach(lib, st, va) != 0:
            raise RuntimeError("sg_ts_live_attach failed")
        st.call("overlay.logStart", [])

    def after(st, _res):
        st.call("overlay.logWrite", [])
        got["t"] = (st.files_read("$gfx/overlay_calls.txt") or b"").decode()
        st.call("gfx.detach", [])
        TLm.live_detach(lib, st)

    res = B.run(scenario, lib, before=before, after=after, to_end=True)
    rows = []
    for ln in got.get("t", "").splitlines():
        p = ln.split()
        rows.append(dict(who=p[0], ends=tuple(int(v) for v in p[1:5]), w="0x%x" % int(p[5][2:], 16),
                         c=",".join("0x%x" % int(v, 16) for v in p[6][2:].split(",")), ctx=p[7]))
    return rows, f"{res.status} {res.records}/{res.expected}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pairs", nargs="+")
    ap.add_argument("--out", type=pathlib.Path)
    a = ap.parse_args()
    lines = ["label\tscenario\tboot\tstream_calls\tport_calls\tcompared\tequal\tfirst_difference"]
    for pair in a.pairs:
        label, scen = pair.split("=")
        s = stream_calls(label)
        p, boot = port_calls(scen)
        n = min(len(s), len(p))
        eq, first = 0, ""
        for i in range(n):
            if s[i][1] == p[i]["w"] and s[i][2] == p[i]["c"]:
                eq += 1
            elif not first:
                first = f"#{i}: stream w={s[i][1]} c={s[i][2]}; port {p[i]['who']} {p[i]['ends']} w={p[i]['w']} c={p[i]['c']}"
        lines.append(f"{label}\t{scen}\t{boot}\t{len(s)}\t{len(p)}\t{n}\t{eq}\t{first}")
        print(lines[-1], flush=True)
    if a.out:
        a.out.write_text("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
