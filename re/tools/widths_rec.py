r"""Text widths of the original (phase 3.8 stream 38a, 2026-10-20; notes log/p38/38a_notes.md).

The port's widgets take Text_put widths from a TextMetrics provider (port/include/portsim/sim/commands.h, U-0944); the
host GDI provider (port/src/sim/gditext.cpp) must equal the original's widths before a segment run uses it. This tool
records them: a trace_tool.py record of a pinned scenario with re/frida/js/widths_rec.js appended to the agent (after
trace_rec.js), which logs every main-thread jgld Font::textWidth, golf_clean.exe Text_put and drawMarkupRun call.
trace_tool.py and trace_rec.js are not changed: without this tool nothing records widths. The same run also writes its
trace, and the tool compares it byte for byte with the corpus trace (port/corpus/traces/<scenario>.trace) and the run's
checkpoints with the pinned reference, so the recorder is shown not to change the run.

    ~/bin/py -3.12 re/tools/widths_rec.py record --scenario cov_a_build2 [--instance p38a] [--label w38a]
    ~/bin/py -3.12 re/tools/widths_rec.py parse --scenario cov_a_build2       # rebuild the TSVs from the raw rings
    ~/bin/py -3.12 re/tools/widths_rec.py summary                             # one line per recorded scenario

Output (local corpus, gitignored): port/corpus/widths/<scenario>.widths.tsv (one row per distinct Font::textWidth call:
font, string bytes, result, count), <scenario>.textput.tsv (one row per distinct Text_put call with the draw context's
markup and font words before and after), <scenario>.markup.tsv (the same for drawMarkupRun, with its run length),
<scenario>.widths.json (counters, fonts with their LOGFONTA, trace and reference comparison), raw rings <scenario>.tw.bin
/ .tp.bin / .dm.bin. Record layouts: re/frida/js/widths_rec.js. The provider check is port/test/widths_check.py.
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import json
import pathlib
import struct
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import trace_tool as TT  # noqa: E402

WIDTHS_JS = ROOT / "re" / "frida" / "js" / "widths_rec.js"
OUT = ROOT / "port" / "corpus" / "widths"
DATA_LO, DATA_HI = 0x004C1000, 0x00842174   # golf_clean.exe .data (the GameState)
RINGS = ("tw", "tp", "dm")


# ---------------------------------------------------------------- run hook
def install_widths(S, state: dict):
    """Subclass the (already trace-installed) Run so the agent also carries widths_rec.js and its messages are kept."""
    Base = S.Run

    class WidthsRun(Base):
        def __init__(self, *a, **kw):
            super().__init__(*a, **kw)
            self.wr_chunks = {k: {} for k in RINGS}
            self.wr_end = None
            self.wr_ready = False
            self.wr_errors = []
            state["wrun"] = self

        def agent_source(self):
            p = super().agent_source()
            cfg = {"drainMs": 20}
            p.write_text(p.read_text() + "\nconst WIDTHS_CFG = " + json.dumps(cfg) + ";\n" + WIDTHS_JS.read_text())
            return p

        def _on_message(self, m, data):
            if m.get("type") == "send":
                pl = m["payload"]
                if isinstance(pl, dict) and pl.get("type") in ("wrec", "wrec_end", "wrec_ready"):
                    if pl["type"] == "wrec":
                        self.wr_chunks[pl["ring"]][pl["seq"]] = bytes(data or b"")
                    elif pl["type"] == "wrec_end":
                        self.wr_end = pl
                    else:
                        self.wr_ready = True
                    return
                if isinstance(pl, dict) and pl.get("type") == "error" and str(pl.get("where", "")).startswith("widths"):
                    self.wr_errors.append(pl)
            return super()._on_message(m, data)

    S.Run = WidthsRun


def run_widths(scenario: str, label: str, instance: str) -> tuple[int, pathlib.Path, dict]:
    state: dict = {}
    orig_install = TT.install

    def install(S, scen, mode, st):
        orig_install(S, scen, mode, st)
        install_widths(S, state)

    TT.install = install
    try:
        rc, run_dir = TT.run_traced(scenario, label, instance, "record", state)
    finally:
        TT.install = orig_install
    return rc, run_dir, state


# ---------------------------------------------------------------- parsing
def ring_bytes(chunks: dict) -> bytes:
    return b"".join(chunks[i] for i in range(len(chunks)))


def parse_ring(data: bytes, want: int) -> list[tuple]:
    """(words, payload bytes) per record of type `want` (1 textWidth, 2 Text_put, 3 drawMarkupRun); padding records are
    skipped."""
    out, o = [], 0
    nwords = {1: 14, 2: 26, 3: 26}[want]
    while o < len(data):
        hdr = struct.unpack_from("<I", data, o)[0]
        typ, ln = hdr >> 24, hdr & 0xFFFFFF
        if ln < 4 or o + ln > len(data):
            raise ValueError(f"bad record at {o}: header {hdr:#x}")
        if typ == want:
            w = struct.unpack_from(f"<{nwords}I", data, o)
            k = w[11] if want == 1 else w[24]
            out.append((w, data[o + 4 * nwords:o + 4 * nwords + k]))
        elif typ != 0:
            raise ValueError(f"record type {typ} in ring {want} at {o}")
        o += ln
    return out


def s32(v: int) -> int:
    return v - (1 << 32) if v & 0x80000000 else v


def fontref_of(owner: int) -> str:
    """The exe FontRef a jgld Font belongs to: its owner word is the FontRef's handle field (FontRef + 4)."""
    if DATA_LO <= owner - 4 < DATA_HI:
        return f"{owner - 4:#010x}"
    return f"owner:{owner:#010x}"


def logfont_fields(b: list[int]) -> dict:
    raw = bytes(b)
    h, w, esc, ori, wt = struct.unpack_from("<5i", raw, 0)
    it, ul, so, cs, op, cp, q, pf = raw[20:28]
    face = raw[28:60].split(b"\0")[0].decode("latin-1")
    return {"height": h, "width": w, "escapement": esc, "orientation": ori, "weight": wt, "italic": it, "underline": ul,
            "strikeout": so, "charset": cs, "outprecision": op, "clipprecision": cp, "quality": q, "pitchfamily": pf,
            "face": face}


def width_rows(tw: list[tuple], fonts: dict) -> list[dict]:
    """Distinct Font::textWidth calls. Sites are raw return addresses: exe callers as VAs (measureTextWidth's
    `call [edi + 0x10]` returns to 0x0048396c), jgld callers at the run's jgld base."""
    rows = collections.OrderedDict()
    for w, b in tw:
        fr = fontref_of(w[3])
        lf = fonts.get(w[5], {})
        key = (fr, w[5], w[7], b, w[9])
        r = rows.get(key)
        if r is None:
            r = rows[key] = {"fontref": fr, "hfont": f"{w[5]:#x}", "face": lf.get("face", "?"), "height": lf.get("height", "?"),
                             "weight": lf.get("weight", "?"), "italic": lf.get("italic", "?"), "flags": f"{w[4]:#x}",
                             "metric8": w[12], "metricC": w[13], "n": s32(w[7]), "copied": w[11], "bytes": b.hex(),
                             "result": s32(w[9]), "count": 0, "first_seq": w[1], "sites": collections.Counter()}
        r["count"] += 1
        r["sites"][f"{w[8]:#010x}"] += 1
    return list(rows.values())


def run_rows(recs: list[tuple]) -> list[dict]:
    """Distinct Text_put / drawMarkupRun calls: context words before and after, text, run length, result."""
    out = collections.OrderedDict()
    for w, b in recs:
        key = (w[2], tuple(w[8:19]), b, w[25], w[7], tuple(w[19:23]))
        r = out.get(key)
        if r is None:
            r = out[key] = {"ctx": f"{w[2]:#010x}", "style38": w[8], "link3c": w[9], "markup40": w[10], "drop4c": w[11],
                            "arrow54": w[12], "font5c": f"{w[13]:#x}", "font60": f"{w[14]:#x}", "font64": f"{w[15]:#x}",
                            "font68": f"{w[16]:#x}", "primary": f"{w[17]:#x}", "handle5c": f"{w[18]:#x}",
                            "strlen": w[23], "copied": w[24], "len": s32(w[25]), "text": b.hex(), "result": s32(w[7]),
                            "after38": w[19], "after3c": w[20], "after4c": w[21], "after5c": f"{w[22]:#x}",
                            "tw_calls": (w[6] - w[5]) & 0xFFFFFFFF, "count": 0, "first_seq": w[1],
                            "sites": collections.Counter()}
        r["count"] += 1
        r["sites"][f"{w[4]:#010x}"] += 1
    return list(out.values())


def build_tables(raw: dict, end: dict) -> dict:
    tw = parse_ring(raw.get("tw", b""), 1)
    tp = parse_ring(raw.get("tp", b""), 2)
    dm = parse_ring(raw.get("dm", b""), 3)
    fonts = {f["hfont"]: dict(logfont_fields(f["logfont"]), got=f["got"]) for f in (end or {}).get("fonts", [])}
    seqs = [w[1] for w, _ in tw]
    gaps = sum(1 for a, b in zip(seqs, seqs[1:]) if b != a + 1)   # every call published in sequence order, no gap
    return {"tw": tw, "tp": tp, "dm": dm, "rows": width_rows(tw, fonts), "tprows": run_rows(tp), "dmrows": run_rows(dm),
            "fonts": fonts, "seq_gaps": gaps}


def write_tsv(path: pathlib.Path, rows: list[dict], cols: list[str]):
    with path.open("w", encoding="utf-8", newline="\n") as fh:
        fh.write("\t".join(cols) + "\n")
        for r in rows:
            vals = []
            for c in cols:
                v = r.get(c, "")
                if isinstance(v, collections.Counter):
                    v = ";".join(f"{k}x{n}" for k, n in v.most_common(4)) + (";..." if len(v) > 4 else "")
                vals.append(str(v))
            fh.write("\t".join(vals) + "\n")


TW_COLS = ["fontref", "hfont", "face", "height", "weight", "italic", "flags", "metric8", "metricC", "n", "copied", "bytes",
           "result", "count", "first_seq", "sites"]
TP_COLS = ["ctx", "style38", "link3c", "markup40", "drop4c", "arrow54", "font5c", "font60", "font64", "font68", "primary",
           "handle5c", "strlen", "copied", "len", "text", "result", "after38", "after3c", "after4c", "after5c", "tw_calls",
           "count", "first_seq", "sites"]


def write_outputs(scenario: str, raw: dict, end: dict, info: dict) -> dict:
    OUT.mkdir(parents=True, exist_ok=True)
    t = build_tables(raw, end)
    write_tsv(OUT / f"{scenario}.widths.tsv", t["rows"], TW_COLS)
    write_tsv(OUT / f"{scenario}.textput.tsv", t["tprows"], TP_COLS)
    write_tsv(OUT / f"{scenario}.markup.tsv", t["dmrows"], TP_COLS)
    info = dict(info)
    info.update(textwidth_calls=len(t["tw"]), textwidth_distinct=len(t["rows"]), textput_calls=len(t["tp"]),
                textput_distinct=len(t["tprows"]), markuprun_calls=len(t["dm"]), markuprun_distinct=len(t["dmrows"]),
                seq_gaps=t["seq_gaps"], fonts={f"{h:#x}": v for h, v in t["fonts"].items()}, end=end)
    (OUT / f"{scenario}.widths.json").write_text(json.dumps(info, indent=1, default=str))
    return info


# ---------------------------------------------------------------- commands
def cmd_record(a) -> int:
    t0 = time.time()
    rc, run_dir, state = run_widths(a.scenario, a.label, a.instance)
    wrun = state.get("wrun")
    OUT.mkdir(parents=True, exist_ok=True)
    trace_out = OUT / f"{a.scenario}.trace"
    data = TT.write_trace(state, trace_out)
    corpus = TT.CORPUS / f"{a.scenario}.trace"
    info = {"scenario": a.scenario, "label": a.label, "instance": a.instance, "rc": rc,
            "run_dir": str(run_dir.relative_to(ROOT)), "seconds": round(time.time() - t0, 1)}
    if data is not None:
        info["trace_sha256"] = hashlib.sha256(data).hexdigest()
        info["trace_vs_corpus"] = ("BYTE-IDENTICAL" if corpus.exists() and corpus.read_bytes() == data
                                   else ("DIFFERS" if corpus.exists() else "no corpus trace"))
        if corpus.exists() and info["trace_vs_corpus"] == "DIFFERS":
            import trace_format as TF
            info["trace_diff"] = TF.diff(TF.read(corpus), TF.parse(data))
    else:
        info["trace_vs_corpus"] = "no complete trace"
    info["reference"] = TT.ref_compare(a.scenario, run_dir)
    if wrun is None or wrun.wr_end is None:
        info["widths"] = "no wrec_end (recording incomplete)"
        (OUT / f"{a.scenario}.widths.json").write_text(json.dumps(info, indent=1, default=str))
        print(f"widths {a.scenario}: INCOMPLETE rc {rc}; trace {info['trace_vs_corpus']}; reference {info['reference']}")
        return 2
    info["errors"] = wrun.wr_errors
    raw = {k: ring_bytes(wrun.wr_chunks[k]) for k in RINGS}
    for k in RINGS:
        (OUT / f"{a.scenario}.{k}.bin").write_bytes(raw[k])
    (OUT / f"{a.scenario}.end.json").write_text(json.dumps(wrun.wr_end, indent=1))
    info = write_outputs(a.scenario, raw, wrun.wr_end, info)
    end = wrun.wr_end
    drops = [end[k]["drops"] for k in RINGS]
    other = [end[k]["other"] for k in RINGS]
    ok = (rc == 0 and info["trace_vs_corpus"] == "BYTE-IDENTICAL" and info["reference"].startswith("SAME")
          and not any(drops) and info["seq_gaps"] == 0 and not wrun.wr_errors)
    print(f"widths {a.scenario}: {'OK' if ok else 'CHECK'} rc {rc}; textWidth {info['textwidth_calls']} calls "
          f"({info['textwidth_distinct']} distinct), Text_put {info['textput_calls']} ({info['textput_distinct']} distinct), "
          f"drawMarkupRun {info['markuprun_calls']} ({info['markuprun_distinct']} distinct); drops {drops}, other-thread "
          f"{other}; trace {info['trace_vs_corpus']}; reference {info['reference']}; {info['seconds']} s")
    return 0 if ok else 1


def cmd_parse(a) -> int:
    end = json.loads((OUT / f"{a.scenario}.end.json").read_text())
    old = json.loads((OUT / f"{a.scenario}.widths.json").read_text())
    keep = {k: old[k] for k in ("scenario", "label", "instance", "rc", "run_dir", "seconds", "trace_sha256", "trace_vs_corpus",
                                "reference", "errors") if k in old}
    raw = {k: (OUT / f"{a.scenario}.{k}.bin").read_bytes() if (OUT / f"{a.scenario}.{k}.bin").exists() else b"" for k in RINGS}
    info = write_outputs(a.scenario, raw, end, keep)
    print(f"{a.scenario}: textWidth {info['textwidth_calls']} ({info['textwidth_distinct']} distinct), Text_put "
          f"{info['textput_calls']} ({info['textput_distinct']} distinct), drawMarkupRun {info['markuprun_calls']} "
          f"({info['markuprun_distinct']} distinct), gaps {info['seq_gaps']}")
    return 0


def cmd_summary(a) -> int:
    for p in sorted(OUT.glob("*.widths.json")):
        i = json.loads(p.read_text())
        e = i.get("end") or {}
        print(f"{i.get('scenario'):22s} rc {i.get('rc')} tw {i.get('textwidth_calls')} ({i.get('textwidth_distinct')}) "
              f"tp {i.get('textput_calls')} ({i.get('textput_distinct')}) dm {i.get('markuprun_calls')} "
              f"({i.get('markuprun_distinct')}) drops {[e.get(k, {}).get('drops') for k in RINGS]} "
              f"trace {i.get('trace_vs_corpus')} ref {i.get('reference')}")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("record")
    p.add_argument("--scenario", required=True)
    p.add_argument("--label", default="w38a")
    p.add_argument("--instance", default="p38a")
    p.set_defaults(fn=cmd_record)
    p = sp.add_parser("parse")
    p.add_argument("--scenario", required=True)
    p.set_defaults(fn=cmd_parse)
    p = sp.add_parser("summary")
    p.set_defaults(fn=cmd_summary)
    a = ap.parse_args()
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
