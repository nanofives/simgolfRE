"""Phase 3.8 stream 38a: the host GDI TextMetrics provider (module gditext) against the widths the ORIGINAL measured.

Input: the recordings of re/tools/widths_rec.py in port/corpus/widths/ (<scenario>.widths.tsv: every distinct jgld
Font::textWidth call with its font, bytes and result; <scenario>.textput.tsv: every distinct Text_put call with the
draw context's markup and font words before and after, its text and its result). Per scenario the provider runs in one
context holding a start image of that scenario's segments (the fonts' string pools, handle words and the metrics
Font::create stored, as the original left them), the GameState at the original data base and the user's install as the
game files:
  widths   gditext.fontWidth(fontref, bytes, n) must equal the recorded result of every row;
  textput  the draw context's words set as recorded, Text_put(ctx, text) (every font word read from the context) must
           give the recorded result and leave +0x38 +0x3c +0x4c +0x5c as recorded;
  setdraw  for the rows whose style fonts are none, the TextMetrics interface call (gditext.textPut, setDrawTarget(font,
           0, 0, 0) semantics, the context's +0x5c cleared first) must give the same result.

Roadmap S5.1: `--provider freetype` runs the same checks with module fttext's FtTextMetrics (FreeType) in place of the
GDI provider (the chain is the same code, only the font primitives differ). Every row whose result differs is listed with
its CAUSE: the bytes whose single-byte advance differs between the FreeType and the GDI provider in the same process
(`advance 0xNN ft F gdi G`), else `no single-byte advance differs` (a difference the per-byte sums do not explain).
`--diff-tsv` writes those rows; the summary gives the textWidth row counts per font (`per_font`).

    ~/bin/py -3.12 -m port.test.widths_check [--provider gdi|freetype] [--scenario s ...] [--negctl N] [--diff-tsv f]
                                             (SG_PORTSIM_DLL selects the DLL)
"""
from __future__ import annotations

import argparse
import csv
import json
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from port.test import lib as L  # noqa: E402

WIDTHS = ROOT / "port" / "corpus" / "widths"
SEG_DIR = ROOT / "port" / "corpus" / "segments"
INSTALL = ROOT / "original"
DATA_VA = 0x004C1000
DATA_END = DATA_VA + 0x381174
TEXT_VA = 0x30000000            # a block outside every image range for the measured bytes
CTX_BLOCK = 0x31000000          # draw contexts that are not in .data are mapped from here, one page each
PRIMARY_OFF = None              # g_primarySurface's GameState offset (looked up from the library's object table)


def scenarios() -> list[str]:
    return sorted(p.name[:-len(".widths.tsv")] for p in WIDTHS.glob("*.widths.tsv"))


def start_image(scenario: str) -> bytes | None:
    d = SEG_DIR / scenario
    for k in sorted(d.glob("[0-9][0-9]")) if d.is_dir() else []:
        p = k / "start.data.bin"
        if p.exists():
            return p.read_bytes()
    return None


def rows(path: pathlib.Path) -> list[dict]:
    with path.open(newline="", encoding="utf-8") as fh:
        return list(csv.DictReader(fh, delimiter="\t"))


def control_only(hexbytes: str) -> bool:
    """A string of C0 control characters only (a lone 0x0a in the recordings): GDI's extent for it is not a function of
    (font, bytes) in a 32-bit process (U-0960, log/p38/38a/newline_probe.log), so these rows are counted apart."""
    b = bytes.fromhex(hexbytes)
    return len(b) > 0 and all(c < 0x20 for c in b)


def has_token(hexbytes: str) -> bool:
    """A text holding one of drawMarkupRun's token characters ({ } [ ] $ = ^)."""
    return any(c in b"{}[]$=^" for c in bytes.fromhex(hexbytes))


def i32(v: int) -> int:
    v &= 0xFFFFFFFF
    return v - (1 << 32) if v & 0x80000000 else v


PROVIDERS = {"gdi": "gditext", "freetype": "fttext"}


class Checker:
    def __init__(self, lib: L.Lib, scenario: str, provider: str = "gdi"):
        self.lib, self.scenario, self.provider = lib, scenario, provider
        self.mod = PROVIDERS[provider]
        self.offs = {n: o for n, o, _ in lib.objects()}
        self.st = lib.state()
        self.st.set_data_base(DATA_VA)
        img = start_image(scenario)
        if img is None:
            self.st.fill_from_exe(whole_image=True)
            self.image = "exe image"
        else:
            self.st.load_image(img)
            self.image = "segment start image"
        self.st.files_root(INSTALL)
        self.st.call(f"{self.mod}.attach", [])
        self.text = (L.C.c_uint8 * 0x2000)()
        self.st.map(TEXT_VA, self.text)
        self.ctx_blocks: dict[int, object] = {}
        self.ref = None          # the GDI provider for cause lookups (freetype only), made on the first differing row
        self.diffs: list[dict] = []

    def close(self):
        try:
            self.st.call(f"{self.mod}.detach", [])
        finally:
            self.st.close()
            if self.ref is not None:
                self.ref.close()

    def advance(self, fontref: int, b: int):
        self.put_text(bytes([b]))
        return self.call(f"{self.mod}.fontWidth", [fontref, TEXT_VA, 1])[0]

    def cause(self, fontref: int, data: bytes) -> str:
        """Why a FreeType result differs: the bytes whose single-byte advance differs from the GDI provider's."""
        if self.provider == "gdi":
            return ""
        if self.ref is None:
            self.ref = Checker(self.lib, self.scenario, "gdi")
        out = []
        for b in sorted(set(data)):
            f, g = self.advance(fontref, b), self.ref.advance(fontref, b)
            if f != g:
                out.append(f"advance 0x{b:02x} ft {f} gdi {g}")
        if not out and control_only(data.hex()):
            return "control-only text: both providers give this result, the recording another (U-0960)"
        return "; ".join(out) if out else "no single-byte advance differs"

    def note(self, table: str, fontref: int, data: bytes, want, got, err, extra=""):
        self.diffs.append({"scenario": self.scenario, "table": table, "fontref": f"0x{fontref:08x}", "bytes": data.hex(),
                           "want": want, "got": got if err is None else f"error {err}",
                           "control_only": control_only(data.hex()),
                           "cause": self.cause(fontref, data) if err is None else "error", "extra": extra})

    def put_text(self, b: bytes):
        L.C.memset(self.text, 0, len(self.text))
        L.C.memmove(self.text, b, len(b))

    def call(self, name, args):
        try:
            return i32(self.st.call(name, args).eax), None
        except L.SgError as e:
            return None, f"{name}: {e}"

    # ---- draw context words
    def ctx_write(self, ctx: int, off: int, v: int):
        va = ctx + off
        if DATA_VA <= va < DATA_END:
            self.st.write(va - DATA_VA, struct.pack("<I", v & 0xFFFFFFFF))
            return
        page = va & ~0xFFF
        if page not in self.ctx_blocks:
            self.ctx_blocks[page] = self.st.map(page, (L.C.c_uint8 * 0x1000)())
        buf = self.ctx_blocks[page]
        struct.pack_into("<I", buf, va - page, v & 0xFFFFFFFF)

    def ctx_read(self, ctx: int, off: int) -> int:
        va = ctx + off
        if DATA_VA <= va < DATA_END:
            return struct.unpack("<I", self.st.read(va - DATA_VA, 4))[0]
        page = va & ~0xFFF
        return struct.unpack_from("<I", self.ctx_blocks[page], va - page)[0]

    def set_primary(self, v: int):
        self.st.write(self.offs["g_primarySurface"], struct.pack("<I", v & 0xFFFFFFFF))

    # ---- the three checks
    def widths(self) -> dict:
        res = {"rows": 0, "equal": 0, "differ": 0, "error": 0, "skipped": 0, "calls": 0, "calls_equal": 0, "examples": [],
               "control_rows": 0, "control_equal": 0, "control_differ": [], "per_font": {}}
        for r in rows(WIDTHS / f"{self.scenario}.widths.tsv"):
            n, copied, cnt = int(r["n"]), int(r["copied"]), int(r["count"])
            if n > copied and n > 0:
                res["skipped"] += 1
                continue
            res["rows"] += 1
            res["calls"] += cnt
            fontref = int(r["fontref"], 16)
            self.put_text(bytes.fromhex(r["bytes"]))
            got, err = self.call(f"{self.mod}.fontWidth", [fontref, TEXT_VA, n])
            pf = res["per_font"].setdefault(f"{r['fontref']} {r['face']} {r['height']}", [0, 0])
            pf[0] += 1
            pf[1] += 1 if err is None and got == int(r["result"]) else 0
            if err is not None or got != int(r["result"]):
                self.note("widths", fontref, bytes.fromhex(r["bytes"])[:max(n, 0)], int(r["result"]), got, err)
            if err:
                res["error"] += 1
                if len(res["examples"]) < 6:
                    res["examples"].append({"fontref": r["fontref"], "bytes": r["bytes"][:60], "error": err})
            elif got == int(r["result"]):
                res["equal"] += 1
                res["calls_equal"] += cnt
            else:
                res["differ"] += 1
                if len(res["examples"]) < 6:
                    res["examples"].append({"fontref": r["fontref"], "face": r["face"], "bytes": r["bytes"][:60],
                                            "want": int(r["result"]), "got": got})
            if control_only(r["bytes"]):
                res["control_rows"] += 1
                if err is None and got == int(r["result"]):
                    res["control_equal"] += 1
                else:
                    res["control_differ"].append({"fontref": r["fontref"], "bytes": r["bytes"], "want": int(r["result"]),
                                                  "got": got, "count": cnt})
        return res

    def runs(self, kind: str) -> dict:
        """kind "textput": Text_put(ctx, text); kind "markup": drawMarkupRun(ctx, text, len) (the run length recorded)."""
        res = {"rows": 0, "equal": 0, "differ": 0, "error": 0, "skipped": 0, "calls": 0, "calls_equal": 0,
               "setdraw_rows": 0, "setdraw_equal": 0, "examples": [], "control_rows": 0, "control_equal": 0,
               "token_rows": 0, "token_equal": 0}
        path = WIDTHS / f"{self.scenario}.{kind}.tsv"
        for r in rows(path) if path.exists() else []:
            if int(r["strlen"]) > int(r["copied"]):
                res["skipped"] += 1
                continue
            res["rows"] += 1
            cnt = int(r["count"])
            res["calls"] += cnt
            ctx = int(r["ctx"], 16)
            before = [int(r["style38"]), int(r["link3c"]), int(r["markup40"]), int(r["drop4c"]), int(r["arrow54"]),
                      int(r["font5c"], 16), int(r["font60"], 16), int(r["font64"], 16), int(r["font68"], 16)]
            offs = [0x38, 0x3c, 0x40, 0x4c, 0x54, 0x5c, 0x60, 0x64, 0x68]

            def setup():
                for o, v in zip(offs, before):
                    self.ctx_write(ctx, o, v)
                self.set_primary(int(r["primary"], 16))
                self.put_text(bytes.fromhex(r["text"]))
            setup()
            if kind == "textput":
                got, err = self.call("Text_put", [ctx, TEXT_VA])
            else:
                got, err = self.call("drawMarkupRun", [ctx, TEXT_VA, int(r["len"])])
            after = None if err else [self.ctx_read(ctx, o) for o in (0x38, 0x3c, 0x4c, 0x5c)]
            want_after = [int(r["after38"]), int(r["after3c"]), int(r["after4c"]), int(r["after5c"], 16)]
            ok = err is None and got == int(r["result"]) and after == want_after
            if err:
                res["error"] += 1
            elif ok:
                res["equal"] += 1
                res["calls_equal"] += cnt
            else:
                res["differ"] += 1
            if control_only(r["text"]):
                res["control_rows"] += 1
                res["control_equal"] += 1 if ok else 0
            if has_token(r["text"]):
                res["token_rows"] += 1
                res["token_equal"] += 1 if ok else 0
            if not ok:
                self.note(kind, before[5], bytes.fromhex(r["text"]), int(r["result"]), got, err,
                          f"after {after} want {want_after}")
            if not ok and len(res["examples"]) < 6:
                res["examples"].append({"kind": kind, "text": r["text"][:80], "before": before, "want": int(r["result"]),
                                        "got": got, "after": after, "want_after": want_after, "error": err})
            # the TextMetrics interface call: setDrawTarget(font, 0, 0, 0) semantics over a context whose +0x5c is cleared
            if kind == "textput" and before[6] == before[7] == before[8] == 0 and before[5] != 0:
                setup()
                self.ctx_write(ctx, 0x5c, 0)
                g2, e2 = self.call(f"{self.mod}.textPut", [ctx, before[5], TEXT_VA])
                res["setdraw_rows"] += 1
                if e2 is None and g2 == int(r["result"]):
                    res["setdraw_equal"] += 1
        return res


def check(lib: L.Lib, scenario: str, provider: str = "gdi") -> dict:
    c = Checker(lib, scenario, provider)
    try:
        out = {"scenario": scenario, "provider": provider, "image": c.image, "widths": c.widths(),
               "textput": c.runs("textput"), "markup": c.runs("markup")}
        out["diffs"] = c.diffs
    finally:
        c.close()
    w, t, m = out["widths"], out["textput"], out["markup"]
    out["ok_all"] = (w["rows"] > 0 and w["skipped"] == 0 and all(x["equal"] == x["rows"] and x["skipped"] == 0
                                                                 for x in (w, t, m))
                     and t["setdraw_equal"] == t["setdraw_rows"])
    # every row with printable text equal (the control-character rows are reported, not required: U-0960)
    out["ok"] = (w["rows"] > 0 and all(x["skipped"] == 0 and x["error"] == 0
                                       and x["equal"] - x["control_equal"] == x["rows"] - x["control_rows"]
                                       for x in (w, t, m))
                 and t["setdraw_equal"] - t["control_equal"] == t["setdraw_rows"] - t["control_rows"])
    return out


def load_win32(build_dir: pathlib.Path):
    """A lib32.Lib32 over the runner and DLL of a private Win32 build (lib32.load32 always uses port/build/Win32)."""
    from port.test import lib32 as L32
    lib = L32.Lib32.__new__(L32.Lib32)
    lib.path = build_dir / "Release" / "portsim_runner32.exe"
    lib.runner = L32.Runner(lib.path, build_dir / "Release" / "portsim_c.dll")
    _, r = lib.runner.tx(L32.OP_HELLO)
    lib.state_size = r.u32()
    lib._info = r.s()
    lib.pointer_size = r.u32()
    lib._objects, lib._funcs, lib.fpcw, lib.dll = {}, None, L32.DEFAULT_CW, None
    return lib


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--scenario", action="append")
    ap.add_argument("--negctl", type=int, default=0)
    ap.add_argument("--json")
    ap.add_argument("--provider", default="gdi", choices=sorted(PROVIDERS))
    ap.add_argument("--diff-tsv", help="write every differing row with its cause")
    ap.add_argument("--win32", help="a Win32 build directory: measure inside its 32-bit runner process")
    a = ap.parse_args()
    lib = load_win32(pathlib.Path(a.win32)) if a.win32 else L.load()
    if a.negctl:
        lib.negctl(f"{PROVIDERS[a.provider]}.fontWidth", a.negctl)
    outs = []
    tot = {}
    names = a.scenario or scenarios()
    for i, s in enumerate(names, 1):
        o = check(lib, s, a.provider)
        outs.append(o)
        w, t, m = o["widths"], o["textput"], o["markup"]
        for tag, x in (("widths", w), ("textput", t), ("markup", m)):
            for k in ("rows", "equal", "calls", "calls_equal", "control_rows", "control_equal", "error", "skipped",
                      "token_rows", "token_equal", "setdraw_rows", "setdraw_equal"):
                if k in x:
                    tot[f"{tag}.{k}"] = tot.get(f"{tag}.{k}", 0) + x[k]
        print(f"[{i}/{len(names)}] {s} {'PASS' if o['ok'] else 'FAIL'} textWidth rows {w['equal']}/{w['rows']} "
              f"(calls {w['calls_equal']}/{w['calls']}, control-only {w['control_equal']}/{w['control_rows']}, errors "
              f"{w['error']}, skipped {w['skipped']}); Text_put rows {t['equal']}/{t['rows']} (calls {t['calls_equal']}/"
              f"{t['calls']}; setDrawTarget interface {t['setdraw_equal']}/{t['setdraw_rows']}); drawMarkupRun rows "
              f"{m['equal']}/{m['rows']} (calls {m['calls_equal']}/{m['calls']}, with a token {m['token_equal']}/"
              f"{m['token_rows']})", flush=True)
        for ex in [e for e in (w["examples"] + t["examples"] + m["examples"])
                   if not control_only(e.get("bytes", e.get("text", "")))][:4]:
            print("    ", json.dumps(ex)[:300])
    per_font = {}
    for o in outs:
        for k, (n, e) in o["widths"]["per_font"].items():
            pf = per_font.setdefault(k, [0, 0])
            pf[0] += n
            pf[1] += e
    for k in sorted(per_font):
        print(f"  per font {k}: textWidth rows {per_font[k][1]}/{per_font[k][0]}")
    tot["per_font"] = per_font
    print(json.dumps(tot))
    if a.diff_tsv:
        cols = ["scenario", "table", "fontref", "bytes", "want", "got", "control_only", "cause", "extra"]
        with open(a.diff_tsv, "w", encoding="utf-8", newline="") as fh:
            fh.write("\t".join(cols) + "\n")
            for o in outs:
                for d in o["diffs"]:
                    fh.write("\t".join(str(d[c]) for c in cols) + "\n")
    if a.json:
        pathlib.Path(a.json).write_text(json.dumps({"total": tot, "scenarios": outs}, indent=1))
    return 0 if all(o["ok"] for o in outs) else 1


if __name__ == "__main__":
    sys.exit(main())
