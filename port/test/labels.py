"""Roadmap step S5.2: the label pixel corpus and the reference raster of a text provider (notes log/roadmap/S5/2_notes.md).

THE CORPUS (port/corpus/labels/corpus.json, local). Every distinct (FontRef, text) of the widths recordings
(port/corpus/widths/<scenario>.widths.tsv: jgld Font::textWidth's bytes; <scenario>.textput.tsv: Text_put's text, in the
draw context's base font +0x5c), drawn as the run drawer hands it to TextOutA ('_' as ' ', the run drawer's loop at
allocTextBuffer, c3et.cpp; texts with a tab or a C0 control byte are counted and left out). Each entry gets a surface of
(recorded width + 2 mx) x (FontRef +0xc + 2 my) words with the baseline at my + FontRef +0x18 and the pen at mx: sizes come
from the recording and the FontRef words the ORIGINAL stored, never from the provider, so every provider draws on the same
surface. Configurations (the colour words the exe's text calls push, log/roadmap/S5/2_colours.tsv, converted as the run
drawer converts a direct colour on the 5-5-5 surface: r = (c >> 7) & 0xf8, g = (c >> 2) & 0xf8, b = (c << 3) & 0xff):
  base     every entry, 0x80007fff on 0x0000          (white: 48 immediate call sites)
  inverse  every entry, 0x80000000 on 0x7fff          (black: 138 immediate call sites, the most common word)
  sweep    per font its 6 most called texts x the 21 direct colour words of the scan x backgrounds 0x0000 0x7fff 0x4210
  shadow   the sweep texts with drawCenteredText2's shadow (setQuad(colour, 1, 0, 1): the run drawn first at (+0, +1) in
           shadow colour 1, a palette index) in 0x0000 as a stand-in for palette index 1 (U-1512), then 0x80007fff, on 0x4210
plus the label calls of the gfx checkpoints that PASS at HEAD (sandbox_basic menu and the five cov_b_location checkpoints),
recorded by textlab.recordStart over a real boot with the GDI provider: font, colour, origin, text, ink box and the
surface words of the box before and after (port/corpus/labels/checkpoints/<scenario>.calls.bin).

    ~/bin/py -3.12 -m port.test.labels corpus                      # writes corpus.json
    ~/bin/py -3.12 -m port.test.labels render gdi [--backend x64|win32]   # rasters/<provider>_<backend>.npz
    ~/bin/py -3.12 -m port.test.labels reference                   # GDI twice on x64 and Win32 -> reference.json
    ~/bin/py -3.12 -m port.test.labels checkpoints                 # records the checkpoint label calls (boots, light)
    ~/bin/py -3.12 -m port.test.labels facts                       # the measured rendering facts -> facts.json
    ~/bin/py -3.12 -m port.test.labels compare <provider> [--backend ..] [--negctl N]

SG_PORTSIM_DLL selects the x64 DLL; --w32-dir a private Win32 build (else port/build/Win32). Provider names: gdi = module
gditext; any other name <p> attaches module <p> through `<p>.attach` (freetype = fttext).
"""
from __future__ import annotations

import argparse
import collections
import csv
import hashlib
import json
import os
import pathlib
import struct
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from port.test import lib as L  # noqa: E402

WIDTHS = ROOT / "port" / "corpus" / "widths"
SEG_DIR = ROOT / "port" / "corpus" / "segments"
OUT = ROOT / "port" / "corpus" / "labels"
INSTALL = ROOT / "original"
COLOURS_TSV = ROOT / "log" / "roadmap" / "S5" / "2_colours.tsv"
DATA_VA = 0x004C1000
JOBS_VA, TEXT_VA, OUT_VA = 0x30000000, 0x30400000, 0x32000000
JOB_SIZE, TEXT_CAP, OUT_CAP = 0x2c, 0x00400000, 0x01000000
IMAGE_SCENARIO = "cov_a_play"         # the start image whose FontRefs every entry uses (all eight fonts created there)
SWEEP_TEXTS, SWEEP_BGS = 6, (0x0000, 0x7fff, 0x4210)
WHITE, BLACK = 0x80007fff, 0x80000000
PROVIDERS = {"gdi": "gditext", "freetype": "fttext"}
CHECKPOINTS = {"sandbox_basic": ("menu",), "cov_b_location": ("refused", "site_hover", "buttons", "tip", "back")}


def module_of(provider: str) -> str:
    return PROVIDERS.get(provider, provider)


def colorref(c: int) -> int:
    """the run drawer's direct colour on the 5-5-5 surface (SetRunColour in c3et.cpp; gfx.cpp draw_text_string)"""
    r, g, b = (c >> 7) & 0xf8, (c >> 2) & 0xf8, (c << 3) & 0xff
    return r | (g << 8) | (b << 16)


def word555(cref: int) -> int:
    """the RGB555 word GDI stores for an opaque pixel of colorref `cref` on a BI_RGB 16-bit DIB (top 5 bits of each)"""
    r, g, b = cref & 0xff, (cref >> 8) & 0xff, (cref >> 16) & 0xff
    return ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)


def scan_colours() -> list[int]:
    """the distinct immediate direct colour words of the text call sites (2_colours.py), in first-seen order"""
    out = []
    with COLOURS_TSV.open() as fh:
        for r in csv.DictReader((l for l in fh if not l.startswith("#")), delimiter="\t"):
            if r["colour"] != "?":
                v = int(r["colour"], 16)
                if v & 0x80000000 and v not in out:
                    out.append(v)
    return out


# ---------------------------------------------------------------------------------------------------- the corpus
def start_image(scenario: str) -> bytes:
    for k in sorted((SEG_DIR / scenario).glob("[0-9][0-9]")):
        p = k / "start.data.bin"
        if p.exists():
            return p.read_bytes()
    raise FileNotFoundError(f"no segment start image for {scenario}")


def fontref_words(img: bytes, fontref: int) -> list[int]:
    o = fontref - DATA_VA
    return list(struct.unpack_from("<8i", img, o))   # +0 vtable, +4 handle, +8, +0xc height, +0x10, +0x14, +0x18, +0x1c


def drawn(b: bytes) -> bytes | None:
    if not b or b"\t" in b or any(c < 0x20 for c in b):
        return None
    return b.replace(b"_", b" ")


def build_corpus() -> dict:
    img = start_image(IMAGE_SCENARIO)
    entries: dict[tuple[int, bytes], dict] = {}
    left_out = collections.Counter()
    for scen in sorted(p.name[:-len(".widths.tsv")] for p in WIDTHS.glob("*.widths.tsv")):
        for kind, col_font, col_text in (("widths", "fontref", "bytes"), ("textput", "font5c", "text")):
            path = WIDTHS / f"{scen}.{kind}.tsv"
            if not path.exists():
                continue
            with path.open(newline="", encoding="utf-8") as fh:
                for r in csv.DictReader(fh, delimiter="\t"):
                    if kind == "widths" and int(r["n"]) > int(r["copied"]):
                        left_out["truncated"] += 1
                        continue
                    if kind == "textput" and int(r["strlen"]) > int(r["copied"]):
                        left_out["truncated"] += 1
                        continue
                    raw = bytes.fromhex(r[col_text])
                    if kind == "widths":
                        raw = raw[:int(r["n"])]
                    t = drawn(raw)
                    if t is None:
                        left_out["tab or control byte" if raw else "empty"] += 1
                        continue
                    f = int(r[col_font], 16)
                    key = (f, t)
                    e = entries.get(key)
                    if e is None:
                        e = entries[key] = {"fontref": f, "text": t.hex(), "width": int(r["result"]), "calls": 0,
                                            "sources": []}
                    e["width"] = max(e["width"], int(r["result"]))
                    e["calls"] += int(r["count"])
                    if f"{scen}.{kind}" not in e["sources"]:
                        e["sources"].append(f"{scen}.{kind}")
    fonts = {}
    for f in sorted({k[0] for k in entries}):
        w = fontref_words(img, f)
        if w[1] == 0:
            raise RuntimeError(f"FontRef 0x{f:08x} has no handle in the {IMAGE_SCENARIO} image")
        fonts[f"0x{f:08x}"] = {"height": w[3], "w10": w[4], "internal": w[5], "ascent": w[6], "descent": w[7]}
    ents = sorted(entries.values(), key=lambda e: (e["fontref"], e["text"]))
    for i, e in enumerate(ents):
        fw = fonts[f"0x{e['fontref']:08x}"]
        t = bytes.fromhex(e["text"])
        mx, my = 4 + fw["height"] // 2, 4
        tokens = sum(1 for c in t if c in b"{}[]$=^")
        e["id"] = i
        e["w"] = max(1, e["width"]) + 2 * mx + tokens * fw["height"]
        e["h"] = fw["height"] + 2 * my
        e["x"], e["y"] = mx, my + fw["ascent"]
    colours = scan_colours()
    by_font = collections.defaultdict(list)
    for e in ents:
        by_font[e["fontref"]].append(e)
    sweep = []
    for f, es in sorted(by_font.items()):
        sweep += [e["id"] for e in sorted(es, key=lambda e: (-e["calls"], e["text"]))[:SWEEP_TEXTS]]
    jobs = []   # (config, entry id, colour word, bg, shadow colour word or None)
    for e in ents:
        jobs.append(("base", e["id"], WHITE, 0x0000, None))
    for e in ents:
        jobs.append(("inverse", e["id"], BLACK, 0x7fff, None))
    for i in sweep:
        for c in colours:
            for bg in SWEEP_BGS:
                jobs.append(("sweep", i, c, bg, None))
    for i in sweep:
        jobs.append(("shadow", i, WHITE, 0x4210, 0x80000000))
    return {"image": IMAGE_SCENARIO, "fonts": fonts, "entries": ents, "colours": [f"0x{c:08x}" for c in colours],
            "jobs": [list(j) for j in jobs], "left_out": dict(left_out),
            "per_font": {f"0x{f:08x}": len(es) for f, es in sorted(by_font.items())}}


def load_corpus() -> dict:
    return json.loads((OUT / "corpus.json").read_text())


# ---------------------------------------------------------------------------------------------------- rendering
def load_lib(backend: str, w32_dir: str | None = None):
    if backend == "x64":
        return L.load()
    if w32_dir:
        from port.test.widths_check import load_win32
        return load_win32(pathlib.Path(w32_dir))
    from port.test import lib32
    return lib32.load32()


class Lab:
    """One context: the corpus image, the install as game files, the provider attached, the job / text / out blocks."""

    def __init__(self, lib, provider: str, image: bytes):
        self.lib, self.provider = lib, provider
        self.st = lib.state()
        self.st.set_data_base(DATA_VA)
        self.st.load_image(image)
        self.st.files_root(INSTALL)
        self.st.call(f"{module_of(provider)}.attach", [])
        self.jobs = self.st.map(JOBS_VA, (L.C.c_uint8 * (JOB_SIZE * 0x10000))())
        self.text = self.st.map(TEXT_VA, (L.C.c_uint8 * TEXT_CAP)())
        self.out = self.st.map(OUT_VA, (L.C.c_uint8 * OUT_CAP)())

    def close(self):
        try:
            self.st.call(f"{module_of(self.provider)}.detach", [])
        finally:
            self.st.close()

    def render(self, jobs: list[tuple]) -> list[np.ndarray]:
        """jobs: (fontref, colour word, bg, x, y, w, h, text bytes, shadow word or None, sdx, sdy) -> h x w uint16 each"""
        res = []
        i = 0
        while i < len(jobs):
            tpos, opos, k = 0, 0, i
            while k < len(jobs) and k - i < 0x10000:
                f, c, bg, x, y, w, h, t, sh, sdx, sdy = jobs[k]
                nbytes = (2 * w * h + 3) & ~3
                if tpos + len(t) + 1 > TEXT_CAP or opos + nbytes > OUT_CAP:
                    break
                L.C.memmove(L.C.addressof(self.text) + tpos, t, len(t))
                struct.pack_into("<IIIiiiiIiIi", self.jobs, (k - i) * JOB_SIZE, f, colorref(c), bg, x, y, w, h,
                                 TEXT_VA + tpos, len(t), 0xFFFFFFFF if sh is None else colorref(sh),
                                 ((sdx & 0xFFFF) << 16) | (sdy & 0xFFFF))
                tpos += (len(t) + 4) & ~3
                opos += nbytes
                k += 1
            if k == i:
                raise RuntimeError("a job does not fit the blocks")
            n = self.st.call("textlab.batch", [JOBS_VA, k - i, OUT_VA, OUT_CAP]).eax
            if n != k - i:
                raise RuntimeError(f"textlab.batch drew {n} of {k - i} jobs")
            buf = bytes(self.out)
            opos = 0
            for j in jobs[i:k]:
                w, h = j[5], j[6]
                res.append(np.frombuffer(buf, "<u2", w * h, opos).reshape(h, w).copy())
                opos += (2 * w * h + 3) & ~3
            i = k
        return res


def corpus_jobs(corpus: dict) -> list[tuple]:
    ents = corpus["entries"]
    out = []
    for cfg, eid, c, bg, sh in corpus["jobs"]:
        e = ents[eid]
        out.append((e["fontref"], c, bg, e["x"], e["y"], e["w"], e["h"], bytes.fromhex(e["text"]), sh, 0, 1))
    return out


def render_corpus(lib, provider: str, negctl: int = 0) -> list[np.ndarray]:
    corpus = load_corpus()
    prev = lib.negctl("textlab.draw", negctl) if negctl else None
    lab = Lab(lib, provider, start_image(corpus["image"]))
    try:
        return lab.render(corpus_jobs(corpus))
    finally:
        lab.close()
        if negctl:
            lib.negctl("textlab.draw", prev or 0)


def digest(a: np.ndarray) -> str:
    return hashlib.sha256(a.astype("<u2").tobytes()).hexdigest()[:16]


def save_rasters(path: pathlib.Path, rasters: list[np.ndarray]):
    path.parent.mkdir(parents=True, exist_ok=True)
    flat = np.concatenate([r.ravel() for r in rasters]) if rasters else np.zeros(0, "<u2")
    shapes = np.array([r.shape for r in rasters], dtype=np.int32)
    np.savez_compressed(path, flat=flat, shapes=shapes)


def load_rasters(path: pathlib.Path) -> list[np.ndarray]:
    z = np.load(path)
    flat, shapes = z["flat"], z["shapes"]
    out, pos = [], 0
    for h, w in shapes:
        out.append(flat[pos:pos + h * w].reshape(h, w))
        pos += h * w
    return out


# ---------------------------------------------------------------------------------------------------- checkpoints
def parse_calls(raw: bytes) -> list[dict]:
    magic, n = struct.unpack_from("<II", raw, 0)
    if magic != 0x31434c54:
        raise ValueError("not a textlab calls file")
    pos, out = 8, []
    for _ in range(n):
        tag, f, cref, x, y, w, h, tn = struct.unpack_from("<IIIiiiiI", raw, pos)
        pos += 32
        text = raw[pos:pos + tn]
        pos += (tn + 3) & ~3
        bx0, by0, bx1, by1, ok = struct.unpack_from("<iiiiI", raw, pos)
        pos += 20
        npx = max(0, bx1 - bx0) * max(0, by1 - by0)
        pre = np.frombuffer(raw, "<u2", npx, pos).reshape(max(0, by1 - by0), max(0, bx1 - bx0)).copy()
        pos += 2 * npx
        post = np.frombuffer(raw, "<u2", npx, pos).reshape(pre.shape).copy()
        pos += 2 * npx
        pos = (pos + 3) & ~3
        out.append({"tag": tag, "fontref": f, "colorref": cref, "x": x, "y": y, "w": w, "h": h, "text": text,
                    "box": (bx0, by0, bx1, by1), "drawn": ok, "pre": pre, "post": post})
    return out


CALL_MARGIN = 4   # textlab.cpp kMargin


def visible_calls(calls: list[dict], k: int) -> list[dict]:
    """the calls before snapshot k, one per (font, colour, origin, text, box contents before): the last of each"""
    seen = {}
    for c in calls:
        if c["tag"] > k or c["box"][2] <= c["box"][0]:
            continue
        key = (c["fontref"], c["colorref"], c["x"], c["y"], c["text"], c["pre"].tobytes())
        seen[key] = c
    return list(seen.values())


def record_checkpoints(backend: str = "x64", w32_dir: str | None = None) -> dict:
    from port.test import gfx as GX
    from port.test import boot
    lib = load_lib(backend, w32_dir)
    summary = {}
    for scen, chks in CHECKPOINTS.items():
        holder = {}

        def before(st, _chks=chks, _scen=scen):
            st.call("gfx.attach", [])
            for o, _c in sorted((GX.checkpoint_ordinal(_scen, c), c) for c in _chks):
                st.call("gfx.snapshotAt", [o])
            st.call("textlab.recordStart", [])

        def after(st, _res, _holder=holder, _chks=chks):
            if st.call("textlab.recordStop", []).eax != 0:
                raise RuntimeError("textlab.recordStop failed")
            _holder["calls"] = st.files_read("$textlab/calls.bin")
            frames = {}
            ords = sorted((GX.checkpoint_ordinal(scen, c), c) for c in _chks)
            for k, (_o, c) in enumerate(ords):
                raw = st.files_read(f"$gfx/snapshot_{k}.555")
                frames[c] = raw
            _holder["frames"] = frames
            _holder["order"] = [c for _o, c in ords]
            st.call("gfx.detach", [])

        no_frames = not (boot.expected_records(scen)[1] or [])
        tr_last = max(GX.checkpoint_ordinal(scen, c) for c in chks)
        tr = GX.TF.read(boot.TRACES / f"{scen}.trace")
        max_records = sum(1 for r in tr.records if r.type == GX.TF.FRAME and r.f["pos"] <= tr_last) + 1
        res = boot.run(scen, lib, max_records=max_records, before=before, after=after, to_end=no_frames)
        d = OUT / "checkpoints"
        d.mkdir(parents=True, exist_ok=True)
        (d / f"{scen}.{backend}.calls.bin").write_bytes(holder["calls"])
        calls = parse_calls(holder["calls"])
        per = {}
        for k, c in enumerate(holder["order"]):
            ref = GX.reference(scen, c)
            snap = holder["frames"][c]
            img = None if snap is None else np.frombuffer(snap, "<u2").reshape(GX.H, GX.W)
            vis = visible_calls(calls, k)
            on = on_frame_calls(calls, k, rgb_to_555(ref))
            per[c] = {"snapshot": img is not None, "calls": len(vis),
                      "boxes": [list(v["box"]) for v in vis],
                      "on_frame": [{"box": list(v["box"]), "fontref": f"0x{v['fontref']:08x}",
                                    "colorref": f"0x{v['colorref']:06x}", "x": v["x"], "y": v["y"],
                                    "text": v["text"].decode("latin-1")} for v in on],
                      "frame_equal": None if img is None else int((rgb(img) != ref).any(axis=2).sum())}
        summary[scen] = {"boot": res.status, "calls_total": len(calls), "checkpoints": per}
    return summary


def rgb(words: np.ndarray) -> np.ndarray:
    sys.path.insert(0, str(ROOT / "re" / "tools"))
    import imgcmp
    return imgcmp.rgb555_to_rgb(words)


def replay_call(st, call: dict, surf_va: int, text_va: int, surf_buf, text_buf) -> np.ndarray:
    bx0, by0, bx1, by1 = call["box"]
    w, h = bx1 - bx0, by1 - by0
    pre = call["pre"].astype("<u2").tobytes()
    L.C.memmove(surf_buf, pre, len(pre))
    L.C.memmove(text_buf, call["text"], len(call["text"]))
    st.call("textlab.draw", [call["fontref"], text_va, len(call["text"]), call["x"] - bx0, call["y"] - by0,
                             call["colorref"], surf_va, w, h])
    return np.frombuffer(bytes(surf_buf)[:2 * w * h], "<u2").reshape(h, w).copy()


def compare_checkpoints(lib, provider: str, image: bytes, backend: str = "x64") -> dict:
    """per checkpoint: the provider's replay of every visible label call on its pre-call box against the GDI run's
    post-call box (the boot's own provider) and, where the box is not drawn over later, against the original's frame"""
    from port.test import gfx as GX
    st = lib.state()
    st.set_data_base(DATA_VA)
    st.load_image(image)
    st.files_root(INSTALL)
    st.call(f"{module_of(provider)}.attach", [])
    surf = st.map(OUT_VA, (L.C.c_uint8 * (2 * GX.W * GX.H))())
    text = st.map(TEXT_VA, (L.C.c_uint8 * 0x1000)())
    out = {}
    try:
        for scen, chks in CHECKPOINTS.items():
            calls = parse_calls((OUT / "checkpoints" / f"{scen}.x64.calls.bin").read_bytes())
            for k, c in enumerate(chks):
                vis = visible_calls(calls, k)
                words = rgb_to_555(GX.reference(scen, c))
                on = {id(v) for v in on_frame_calls(calls, k, words)}
                diff_gdi = diff_frame = 0
                for v in vis:
                    got = replay_call(st, v, OUT_VA, TEXT_VA, surf, text)
                    diff_gdi += int((got != v["post"]).sum())
                    if id(v) in on:
                        bx0, by0, bx1, by1 = v["box"]
                        diff_frame += int((got != words[by0:by1, bx0:bx1]).sum())
                out[f"{scen}/{c}"] = {"calls": len(vis), "on_frame": len(on), "px_vs_gdi": diff_gdi,
                                      "px_vs_original": diff_frame}
    finally:
        st.call(f"{module_of(provider)}.detach", [])
        st.close()
    return out


def last_at_box(vis: list[dict], v: dict) -> dict | None:
    """the last visible call whose box overlaps v's: only that one's post words can be the frame's (later draws over a
    box replace its pixels; the visible list keeps call order)"""
    b = v["box"]
    last = None
    for u in vis:
        a = u["box"]
        if a[0] < b[2] and b[0] < a[2] and a[1] < b[3] and b[1] < a[3]:
            last = u
    return last


# ---------------------------------------------------------------------------------------------------- rendering facts
SPI = {"SPI_GETFONTSMOOTHING": 0x004A, "SPI_GETFONTSMOOTHINGTYPE": 0x200A, "SPI_GETFONTSMOOTHINGCONTRAST": 0x200C,
       "SPI_GETFONTSMOOTHINGORIENTATION": 0x2012, "SPI_GETCLEARTYPE": 0x1048}
QUALITIES = {"DEFAULT_QUALITY": 0, "DRAFT_QUALITY": 1, "PROOF_QUALITY": 2, "NONANTIALIASED_QUALITY": 3,
             "ANTIALIASED_QUALITY": 4, "CLEARTYPE_QUALITY": 5, "CLEARTYPE_NATURAL_QUALITY": 6}
FONT_FILES = ("manu3_.TTF", "KLEPTO__.TTF")


def host_smoothing() -> dict:
    """the host's font smoothing parameters, read with SystemParametersInfoW (GET actions only: nothing is changed)"""
    import ctypes
    out = {}
    for name, action in SPI.items():
        v = ctypes.c_uint(0)
        ok = ctypes.windll.user32.SystemParametersInfoW(action, 0, ctypes.byref(v), 0)
        out[name] = v.value if ok else None
    return out


class GdiProbe:
    """A private GDI raster in this process, for the per-font option the provider does not expose (lfQuality): the font
    of a FontRef as the recordings give its CreateFontIndirectA arguments (<scenario>.widths.tsv face / height / weight /
    italic; OUT_TT_ONLY_PRECIS and every other field 0, as Font::create builds it), the game's font files added with
    FR_PRIVATE (this process only), TextOutA with BkMode TRANSPARENT and TA_BASELINE on a 16-bit BI_RGB top-down DIB
    section, as GdiTextMetrics::draw_glyphs draws. Nothing system-wide is changed."""

    def __init__(self):
        import ctypes
        from ctypes import wintypes as W
        self.c = ctypes
        g = self.gdi = ctypes.windll.gdi32
        g.AddFontResourceExW.argtypes = [W.LPCWSTR, W.DWORD, ctypes.c_void_p]
        g.RemoveFontResourceExW.argtypes = [W.LPCWSTR, W.DWORD, ctypes.c_void_p]
        for fn in FONT_FILES:
            if g.AddFontResourceExW(str(INSTALL / fn), 0x10, None) <= 0:   # FR_PRIVATE
                raise RuntimeError(f"AddFontResourceExW {fn} failed")
        g.CreateCompatibleDC.restype = W.HANDLE
        g.CreateCompatibleDC.argtypes = [W.HANDLE]
        g.DeleteDC.argtypes = [W.HANDLE]
        g.CreateFontIndirectA.restype = W.HANDLE
        g.CreateDIBSection.restype = W.HANDLE
        g.CreateDIBSection.argtypes = [W.HANDLE, ctypes.c_void_p, ctypes.c_uint, ctypes.POINTER(ctypes.c_void_p),
                                       W.HANDLE, W.DWORD]
        g.SelectObject.restype = W.HANDLE
        g.SelectObject.argtypes = [W.HANDLE, W.HANDLE]
        g.DeleteObject.argtypes = [W.HANDLE]
        g.TextOutA.argtypes = [W.HANDLE, ctypes.c_int, ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
        g.SetTextColor.argtypes = [W.HANDLE, W.DWORD]
        g.SetBkMode.argtypes = [W.HANDLE, ctypes.c_int]
        g.SetTextAlign.argtypes = [W.HANDLE, ctypes.c_uint]
        self.dc = g.CreateCompatibleDC(None)
        self.fonts = {}

    def close(self):
        for h in self.fonts.values():
            self.gdi.DeleteObject(h)
        self.gdi.DeleteDC(self.dc)
        for fn in FONT_FILES:
            self.gdi.RemoveFontResourceExW(str(INSTALL / fn), 0x10, None)

    def font(self, lf: dict, quality: int):
        key = (lf["face"], lf["height"], lf["weight"], lf["italic"], quality)
        if key not in self.fonts:
            c = self.c

            class LOGFONTA(c.Structure):
                _fields_ = [("h", c.c_long), ("w", c.c_long), ("esc", c.c_long), ("ori", c.c_long),
                            ("weight", c.c_long), ("italic", c.c_ubyte), ("under", c.c_ubyte), ("strike", c.c_ubyte),
                            ("charset", c.c_ubyte), ("outp", c.c_ubyte), ("clip", c.c_ubyte), ("quality", c.c_ubyte),
                            ("pitch", c.c_ubyte), ("face", c.c_char * 32)]
            lfa = LOGFONTA(lf["height"], 0, 0, 0, lf["weight"], lf["italic"], 0, 0, 0, 7, 0, quality, 0,
                           lf["face"].encode("latin-1"))
            self.fonts[key] = self.gdi.CreateFontIndirectA(c.byref(lfa))
        return self.fonts[key]

    def draw(self, lf: dict, quality: int, text: bytes, x: int, y: int, cref: int, bg: int, w: int, h: int) -> np.ndarray:
        c, g = self.c, self.gdi

        class BMI(c.Structure):
            _fields_ = [("size", c.c_uint32), ("w", c.c_int32), ("h", c.c_int32), ("planes", c.c_uint16),
                        ("bpp", c.c_uint16), ("comp", c.c_uint32), ("isize", c.c_uint32), ("xp", c.c_int32),
                        ("yp", c.c_int32), ("used", c.c_uint32), ("imp", c.c_uint32), ("pal", c.c_uint32 * 3)]
        bmi = BMI(40, w, -h, 1, 16, 0)
        bits = c.c_void_p()
        bmp = g.CreateDIBSection(self.dc, c.byref(bmi), 0, c.byref(bits), None, 0)
        old = g.SelectObject(self.dc, bmp)
        pitch = (w + 1) & ~1
        arr = np.ctypeslib.as_array((c.c_uint16 * (pitch * h)).from_address(bits.value))
        arr[:] = bg
        g.SetBkMode(self.dc, 1)        # TRANSPARENT
        g.SetTextAlign(self.dc, 0x18)  # TA_BASELINE
        oldf = g.SelectObject(self.dc, self.font(lf, quality))
        g.SetTextColor(self.dc, cref)
        g.TextOutA(self.dc, x, y, text, len(text))
        g.GdiFlush()
        out = arr.reshape(h, pitch)[:, :w].copy()
        g.SelectObject(self.dc, oldf)
        g.SelectObject(self.dc, old)
        g.DeleteObject(bmp)
        return out


def recorded_logfonts() -> dict:
    """fontref -> the CreateFontIndirectA arguments the recordings give (widths.tsv face, height, weight, italic)"""
    out = {}
    for p in sorted(WIDTHS.glob("*.widths.tsv")):
        with p.open(newline="", encoding="utf-8") as fh:
            for r in csv.DictReader(fh, delimiter="\t"):
                out.setdefault(int(r["fontref"], 16), {"face": r["face"], "height": int(r["height"]),
                                                       "weight": int(r["weight"]), "italic": int(r["italic"])})
    return out


def classify(a: np.ndarray, bg: int, fg: int) -> str:
    """mono: only bg and fg words; grey: other words, each with r = g = b; subpixel: some other word with unequal
    channels (colour fringes)"""
    u = set(np.unique(a).tolist()) - {bg, fg}
    if not u:
        return "mono"
    for v in u:
        if not (((v >> 10) & 31) == ((v >> 5) & 31) == (v & 31)):
            return "subpixel"
    return "grey"


def rgb_to_555(img: np.ndarray) -> np.ndarray:
    """the RGB555 words of a reference frame (imgcmp.rgb555_to_rgb expands v to (v << 3) | (v >> 2): the top 5 bits)"""
    r, g, b = (img[..., k].astype(np.uint16) >> 3 for k in range(3))
    return (r << 10) | (g << 5) | b


def on_frame_calls(calls: list[dict], k: int, words: np.ndarray) -> list[dict]:
    """the visible calls of checkpoint k whose GDI result is what the original's frame shows in their box (the last call
    at the box, its post-call words equal to the frame's)"""
    vis = visible_calls(calls, k)
    out = []
    for v in vis:
        if last_at_box(vis, v) is not v:
            continue
        bx0, by0, bx1, by1 = v["box"]
        if np.array_equal(words[by0:by1, bx0:bx1], v["post"]):
            out.append(v)
    return out


def measure_facts() -> dict:
    from port.test import gfx as GX
    corpus = load_corpus()
    ents = corpus["entries"]
    ref = load_rasters(OUT / "rasters" / "gdi_x64.npz")
    facts = {"host": host_smoothing()}
    # 1. the GDI provider's raster per configuration
    cls = collections.defaultdict(collections.Counter)
    for (cfg, eid, c, bg, sh), r in zip(corpus["jobs"], ref):
        if sh is None:
            cls[cfg][classify(r, bg, word555(colorref(c)))] += 1
    facts["gdi_raster_classes"] = {k: dict(v) for k, v in cls.items()}
    # 2. the original's labels in the PASS checkpoints: the frame's words where the call changed the surface
    orig = {}
    for scen, chks in CHECKPOINTS.items():
        calls = parse_calls((OUT / "checkpoints" / f"{scen}.x64.calls.bin").read_bytes())
        for k, chk in enumerate(chks):
            words = rgb_to_555(GX.reference(scen, chk))
            tally, classes, crefs = collections.Counter(), collections.Counter(), collections.Counter()
            on = on_frame_calls(calls, k, words)
            for v in on:
                bx0, by0, bx1, by1 = v["box"]
                changed = v["post"] != v["pre"]
                fg = word555(v["colorref"])
                crop = words[by0:by1, bx0:bx1]
                for w_ in crop[changed].tolist():
                    tally["text colour" if w_ == fg else "other"] += 1
                # the changed words alone: mono / grey / subpixel relative to the text colour
                sub = crop[changed]
                classes[classify(sub, fg, fg) if sub.size else "no change"] += 1
                crefs[f"0x{v['colorref']:06x}"] += 1
            orig[f"{scen}/{chk}"] = {"on_frame_calls": len(on), "changed_pixels": dict(tally),
                                     "call_classes": dict(classes), "colorrefs": dict(crefs)}
    facts["original_checkpoint_labels"] = orig
    # 3. lfQuality through the probe; the probe's DEFAULT_QUALITY raster checked against the provider's
    lfs = recorded_logfonts()
    probe = GdiProbe()
    try:
        sweep = sorted({j[1] for j in corpus["jobs"] if j[0] == "sweep"})
        prov = {j[1]: r for j, r in zip(corpus["jobs"], ref) if j[0] == "base"}
        q = {name: {"jobs": 0, "equal_default": 0, "classes": collections.Counter()} for name in QUALITIES}
        pv = {"jobs": 0, "equal": 0}
        fg = word555(colorref(WHITE))
        for eid in sweep:
            e = ents[eid]
            lf, t = lfs[e["fontref"]], bytes.fromhex(e["text"])
            d = probe.draw(lf, 0, t, e["x"], e["y"], colorref(WHITE), 0, e["w"], e["h"])
            pv["jobs"] += 1
            pv["equal"] += int(np.array_equal(d, prov[eid]))
            for name, qv in QUALITIES.items():
                r = probe.draw(lf, qv, t, e["x"], e["y"], colorref(WHITE), 0, e["w"], e["h"])
                q[name]["jobs"] += 1
                q[name]["equal_default"] += int(np.array_equal(r, d))
                q[name]["classes"][classify(r, 0, fg)] += 1
        facts["probe_default_equals_provider"] = pv
        facts["quality_options"] = {k: {**v, "classes": dict(v["classes"])} for k, v in q.items()}
    finally:
        probe.close()
    return facts


# ---------------------------------------------------------------------------------------------------- comparison
def compare_corpus(rasters: list[np.ndarray], ref: list[np.ndarray], corpus: dict) -> dict:
    ents = corpus["entries"]
    per_font = collections.defaultdict(lambda: {"jobs": 0, "jobs_differ": 0, "px_differ": 0})
    per_cfg = collections.defaultdict(lambda: {"jobs": 0, "jobs_differ": 0, "px_differ": 0})
    for (cfg, eid, *_r), a, b in zip(corpus["jobs"], rasters, ref):
        f = f"0x{ents[eid]['fontref']:08x}"
        d = int((a != b).sum()) if a.shape == b.shape else a.size + b.size
        for s in (per_font[f], per_cfg[cfg]):
            s["jobs"] += 1
            s["px_differ"] += d
            s["jobs_differ"] += 1 if d else 0
    return {"per_font": dict(per_font), "per_config": dict(per_cfg),
            "px_differ": sum(v["px_differ"] for v in per_font.values()),
            "jobs_differ": sum(v["jobs_differ"] for v in per_font.values())}


def reference_summary(corpus: dict, rasters: list[np.ndarray]) -> dict:
    ents = corpus["entries"]
    per_font = collections.Counter()
    jobs = []
    for (cfg, eid, c, bg, sh), r in zip(corpus["jobs"], rasters):
        per_font[f"0x{ents[eid]['fontref']:08x}"] += 1
        jobs.append(digest(r))
    allh = hashlib.sha256("".join(jobs).encode()).hexdigest()
    return {"jobs": len(jobs), "per_font_jobs": dict(per_font), "digest_all": allh, "digests": jobs}


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=["corpus", "render", "reference", "checkpoints", "facts", "compare"])
    ap.add_argument("provider", nargs="?", default="gdi")
    ap.add_argument("--backend", default="x64", choices=["x64", "win32"])
    ap.add_argument("--w32-dir")
    ap.add_argument("--negctl", type=int, default=0)
    a = ap.parse_args(argv)
    OUT.mkdir(parents=True, exist_ok=True)
    if a.cmd == "corpus":
        c = build_corpus()
        (OUT / "corpus.json").write_text(json.dumps(c, indent=0))
        print(f"{len(c['entries'])} entries, {len(c['jobs'])} jobs, per font {c['per_font']}, left out {c['left_out']}")
        return 0
    if a.cmd == "render":
        r = render_corpus(load_lib(a.backend, a.w32_dir), a.provider, a.negctl)
        save_rasters(OUT / "rasters" / f"{a.provider}_{a.backend}.npz", r)
        s = reference_summary(load_corpus(), r)
        print(f"{s['jobs']} jobs, digest {s['digest_all']}")
        return 0
    if a.cmd == "reference":
        corpus = load_corpus()
        runs = {}
        for be in ("x64", "win32"):
            lib = load_lib(be, a.w32_dir)
            for k in (1, 2):
                runs[f"{be}_{k}"] = render_corpus(lib, "gdi")
        sums = {k: reference_summary(corpus, v) for k, v in runs.items()}
        alls = {k: s["digest_all"] for k, s in sums.items()}
        ref = sums["x64_1"]
        ref["deterministic"] = {"x64": alls["x64_1"] == alls["x64_2"], "win32": alls["win32_1"] == alls["win32_2"]}
        ref["x64_equals_win32"] = alls["x64_1"] == alls["win32_1"]
        ref["runs"] = alls
        save_rasters(OUT / "rasters" / "gdi_x64.npz", runs["x64_1"])
        save_rasters(OUT / "rasters" / "gdi_win32.npz", runs["win32_1"])
        (OUT / "reference.json").write_text(json.dumps(ref, indent=0))
        print(json.dumps({k: v for k, v in ref.items() if k != "digests"}, indent=1))
        return 0 if all(ref["deterministic"].values()) and ref["x64_equals_win32"] else 1
    if a.cmd == "checkpoints":
        s = record_checkpoints(a.backend, a.w32_dir)
        (OUT / "checkpoints" / f"summary.{a.backend}.json").write_text(json.dumps(s, indent=1))
        print(json.dumps({k: {"boot": v["boot"], "calls": v["calls_total"],
                              "chk": {c: (x["calls"], x["frame_equal"]) for c, x in v["checkpoints"].items()}}
                          for k, v in s.items()}, indent=1))
        return 0
    if a.cmd == "facts":
        f = measure_facts()
        (OUT / "facts.json").write_text(json.dumps(f, indent=1))
        print(json.dumps(f, indent=1))
        return 0
    if a.cmd == "compare":
        corpus = load_corpus()
        lib = load_lib(a.backend, a.w32_dir)
        r = render_corpus(lib, a.provider, a.negctl)
        ref = load_rasters(OUT / "rasters" / "gdi_x64.npz")
        res = compare_corpus(r, ref, corpus)
        prev = lib.negctl("textlab.draw", a.negctl) if a.negctl else None
        try:
            res["checkpoints"] = compare_checkpoints(lib, a.provider, start_image(corpus["image"]), a.backend)
        finally:
            if a.negctl:
                lib.negctl("textlab.draw", prev or 0)
        print(f"corpus: {res['jobs_differ']} jobs / {res['px_differ']} px differ")
        for f, v in sorted(res["per_font"].items()):
            print(f"  font {f}: {v['jobs_differ']} / {v['jobs']} jobs, {v['px_differ']} px")
        for k, v in res["checkpoints"].items():
            print(f"  checkpoint {k}: {v['calls']} calls ({v['on_frame']} on the frame), {v['px_vs_gdi']} px vs the GDI run, "
                  f"{v['px_vs_original']} px vs the original's frame")
        out = OUT / f"compare_{a.provider}_{a.backend}{'_negctl' + str(a.negctl) if a.negctl else ''}.json"
        out.write_text(json.dumps(res, indent=1))
        return 0 if res["px_differ"] == 0 and all(v["px_vs_gdi"] == 0 for v in res["checkpoints"].values()) else 1
    return 2


if __name__ == "__main__":
    sys.exit(main())
