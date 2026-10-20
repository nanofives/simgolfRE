"""SegmentRunner and first-divergence localisation (phase 2.6).

A segment (re/analysis/data/segment_corpus.md sections 2, 5, 7) is port/corpus/segments/<scenario>/<kk>/:
start.data.bin (the GameState image at a frame head), noise.bin (dword bitmap: bytes that differ between continuous runs
of the original; never values), frames.bin (u32 T, u32 frame-head index, u64 sgh64 x n per record; record 0 = the start
frame), frames.json (the n symbols: the sim class in gamestate_meta.inc order), checkpoints.json (object hashes of every
class at the end checkpoint), start.json (queued_messages, input, class), trace.trace (schema v1, re/model/trace_schema.md).

The runner (1) loads start.data.bin into a portsim GameState, (2) checks that the frames.json symbols are the library's
sim objects in order and that no noise dword touches a sim object, (3) LOAD CHECK: the C++ sgh64 of the 203 sim objects
of the loaded state equals frames.bin record 0, (4) calls sg_run_frames; while it returns SG_E_NOT_IMPLEMENTED the
segment is LOADED_ONLY, otherwise every record k >= 1 is compared (count, T, 203 hashes) and the end checkpoint's sim
hashes against the state left after the last frame (PASS / FAIL with the first divergent record and objects).
"""
from __future__ import annotations

import csv
import dataclasses
import json
import pathlib
import struct
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from port.test import lib as L  # noqa: E402

SEG_DIR = ROOT / "port" / "corpus" / "segments"
MANIFEST_TSV = ROOT / "re" / "port" / "segments_manifest.tsv"
TRACE_MAGIC = b"SGTRACE\x00"
INSTALL = ROOT / "original"  # the user's own install (game files for the frame step; tests skip what needs it)
DATA_VA = 0x004C1000  # golf_clean.exe .data base (tooling knowledge of the harness, = re/tools/vector_lib.py DATA_VA)

# NON-CAUSAL sim objects (phase 3.7a): hashed with the sim class, but no value they hold at a frame head can reach any later
# state, so a difference there alone does not fail a segment; it is reported in its own column (SegmentResult.noncausal).
# Each entry cites the evidence; nothing goes here to make a hash match.
NON_CAUSAL = {
    # char[16] at 0x0058a528: all 81 references in golf_clean.exe are address-takes (re/model/inventory_addresses.tsv: 0 direct
    # reads or writes), and every one is the buffer argument of the static CRT's __itoa 0x004ad425 (log/p3/p37a_noncausal.py:
    # 81 of 81), which writes the digits and a NUL from the start and returns the buffer; its readers (appendNumber,
    # appendCents, appendDate and 16 more functions) read the returned text up to that NUL. So what the buffer held before an
    # __itoa call, and every byte past the NUL, is never read. Drawing paths (drawMoneyLabels, panels, report screens) format
    # numbers into it every frame they show one, which a port that drops drawing does not reproduce (U-0946).
    "g_textBuf": "every reference is an __itoa destination (log/p3/p37a_noncausal.py), read only up to its NUL",
}


@dataclasses.dataclass
class Segment:
    scenario: str
    index: int
    dir: pathlib.Path
    start: bytes
    noise: bytes
    symbols: list
    records: list           # [(T, frame_index, [hash] * n)]
    checkpoints: dict
    start_json: dict
    trace: bytes

    @property
    def name(self) -> str:
        return f"{self.scenario}/{self.index:02d}"

    @property
    def queued(self) -> list:
        return self.start_json.get("queued_messages") or []


def read_frames(path: pathlib.Path, n: int) -> list:
    data = path.read_bytes()
    rec = 8 + 8 * n
    if len(data) % rec:
        raise ValueError(f"{path}: {len(data)} bytes is not a multiple of the record size {rec}")
    out = []
    for k in range(len(data) // rec):
        t, idx = struct.unpack_from("<II", data, k * rec)
        out.append((t, idx, list(struct.unpack_from(f"<{n}Q", data, k * rec + 8))))
    return out


def load_segment(d: pathlib.Path) -> Segment:
    d = pathlib.Path(d)
    fj = json.loads((d / "frames.json").read_text())
    syms = fj["symbols"]
    return Segment(scenario=d.parent.name, index=int(d.name), dir=d, start=(d / "start.data.bin").read_bytes(),
                   noise=(d / "noise.bin").read_bytes(), symbols=syms,
                   records=read_frames(d / "frames.bin", len(syms)),
                   checkpoints=json.loads((d / "checkpoints.json").read_text()),
                   start_json=json.loads((d / "start.json").read_text()), trace=(d / "trace.trace").read_bytes())


@dataclasses.dataclass
class SegmentResult:
    name: str
    status: str             # LOADED_ONLY / PASS / FAIL / LOAD_FAILED / MISSING
    detail: str = ""
    frames_expected: int = 0
    frames_run: int = 0
    first_divergence: dict | None = None
    noncausal: str = ""     # differences in NON_CAUSAL objects only: "<object>@<first record>,..." (not a failure)
    run_detail: str = ""    # sg_run_frames_detail (why the run stopped; the input summary)


# ------------------------------------------------------------------------------------------- divergence helpers
def first_divergent_record(expected: list, got: list, symbols: list, ignore=()) -> dict | None:
    """Expected / got records [(T, index, [hash])] aligned in order (record 0 = the start). None when equal. Objects named in
    `ignore` are not compared."""
    for k in range(max(len(expected), len(got))):
        if k >= len(expected) or k >= len(got):
            return {"record": k, "why": f"record count {len(got)} != expected {len(expected)}"}
        te, _, he = expected[k]
        tg, _, hg = got[k]
        objs = [symbols[i] for i in range(len(symbols)) if he[i] != hg[i] and symbols[i] not in ignore]
        if te != tg or objs:
            return {"record": k, "T_expected": te, "T_got": tg, "objects": objs}
    return None


def noncausal_differences(expected: list, got: list, symbols: list) -> str:
    """The NON_CAUSAL objects whose hash differs in some record, each with its first such record: "g_textBuf@12"."""
    out = []
    for i, name in enumerate(symbols):
        if name not in NON_CAUSAL:
            continue
        for k in range(1, min(len(expected), len(got))):
            if expected[k][2][i] != got[k][2][i]:
                out.append(f"{name}@{k}")
                break
    return ",".join(out)


def first_byte_divergence(lib: L.Lib, a: bytes, b: bytes, classes=("sim",), pointer_words: dict | None = None) -> dict | None:
    """First differing byte of two GameState images, in offset order, restricted to objects of `classes` (None = all),
    SG_POINTER_WORD dwords ignored. Returns {symbol, class, offset, object_offset, a, b, objects_differing}."""
    if len(a) != len(b):
        raise ValueError("images differ in size")
    va, vb = np.frombuffer(a, np.uint8), np.frombuffer(b, np.uint8)
    words = pointer_words if pointer_words is not None else _pointer_words()
    first, differing = None, []
    for cls in (classes or [None]):
        for name, off, size in lib.objects(cls):
            x, y = va[off:off + size].copy(), vb[off:off + size].copy()
            for w in words.get(name, ()):
                x[w:w + 4] = 0
                y[w:w + 4] = 0
            nz = np.nonzero(x != y)[0]
            if len(nz):
                differing.append(name)
                k = int(nz[0])
                if first is None or off + k < first["offset"]:
                    first = {"symbol": name, "class": cls or "-", "offset": off + k, "object_offset": k,
                             "a": int(x[k]), "b": int(y[k])}
    if first:
        first["objects_differing"] = differing
    return first


_PW = None


def _pointer_words() -> dict:
    global _PW
    if _PW is None:
        import re
        _PW = {}
        for line in (ROOT / "port" / "include" / "gamestate_meta.inc").read_text().splitlines():
            m = re.match(r"SG_POINTER_WORD\((\w+), (0x[0-9a-f]+)\)$", line)
            if m:
                _PW.setdefault(m.group(1), []).append(int(m.group(2), 16))
    return _PW


# ------------------------------------------------------------------------------------------- runner
class SegmentRunner:
    def __init__(self, lib: L.Lib | None = None):
        self.lib = lib or L.load()
        self.sim = self.lib.objects("sim")

    def noise_sim_bytes(self, seg: Segment) -> int:
        bits = np.unpackbits(np.frombuffer(seg.noise, np.uint8), bitorder="little")
        count = 0
        for _, off, size in self.sim:
            lo, hi = off // 4, (off + size + 3) // 4
            count += int(bits[lo:hi].sum())
        return count

    def load_check(self, seg: Segment, st: L.State) -> str | None:
        """None when the loaded state's sim hashes equal record 0, else why."""
        names = [n for n, _, _ in self.sim]
        if seg.symbols != names:
            return f"frames.json symbols ({len(seg.symbols)}) are not the library's sim objects ({len(names)})"
        if seg.trace[:8] != TRACE_MAGIC:
            return "trace.trace has no SGTRACE header"
        st.load_image(seg.start)
        n = self.noise_sim_bytes(seg)
        if n:
            return f"{n} noise dwords inside sim objects"
        got = st.hash_class("sim")
        exp = seg.records[0][2]
        bad = [names[i] for i in range(len(names)) if got[i] != exp[i]]
        if bad:
            return f"record 0 hash differs for {len(bad)} sim objects: {', '.join(bad[:6])}"
        return None

    def run(self, seg: Segment) -> SegmentResult:
        res = SegmentResult(seg.name, "LOAD_FAILED", frames_expected=len(seg.records) - 1)
        with self.lib.state() as st:
            # phase 3.7a: the GameState at the original data section's base, as in the vector runner: some sim-class
            # objects hold addresses of state (unk_005a9cc8 = cellAt's cell pointer), which only match at that base
            st.set_data_base(DATA_VA)
            # the user's own install as the game files (story files, rosters), as the vector runner attaches it
            if INSTALL.is_dir():
                st.files_root(INSTALL)
            why = self.load_check(seg, st)
            if why:
                res.detail = why
                return res
            code, n, ts, hs = st.run_frames(seg.trace, seg.queued, res.frames_expected)
            res.run_detail = self._detail(st)
            if code == L.SG_E_NOT_IMPLEMENTED:
                res.status, res.detail = "LOADED_ONLY", "load check passed; sg_run_frames not implemented"
                return res
            res.frames_run = n
            if code != L.SG_OK:
                res.status, res.detail = "FAIL", f"sg_run_frames returned {code} ({L.ERRORS.get(code, '?')})"
                return res
            got = [seg.records[0]] + [(ts[k], 0, hs[k]) for k in range(n)]
            div = first_divergent_record(seg.records, got, seg.symbols, ignore=NON_CAUSAL)
            res.noncausal = noncausal_differences(seg.records, got, seg.symbols)
            if div is None:
                end = self.end_check(seg, st)
                nc_end = [s for s in end if s in NON_CAUSAL]
                end = [s for s in end if s not in NON_CAUSAL]
                if nc_end:
                    res.noncausal = ",".join(filter(None, [res.noncausal] + [f"{s}@end" for s in nc_end]))
                if end:
                    div = {"record": "end", "objects": end}
            res.first_divergence = div
            res.status = "PASS" if div is None else "FAIL"
            res.detail = "" if div is None else json.dumps(div)[:300]
            return res

    def _detail(self, st: L.State) -> str:
        f = getattr(getattr(self.lib, "dll", None), "sg_run_frames_detail", None)
        if f is None:
            return ""
        import ctypes as C
        f.restype = C.c_char_p
        f.argtypes = [C.c_void_p]
        try:
            return (f(st.ctx) or b"").decode("utf-8", "replace")
        except Exception:  # noqa: BLE001 (the Win32 runner's State has no in-process ctx)
            return ""

    def end_check(self, seg: Segment, st: L.State) -> list:
        """Sim objects whose hash differs from the end checkpoint (checkpoints.json, the last checkpoint)."""
        if not seg.checkpoints:
            return []
        cp = list(seg.checkpoints.values())[-1]["hashes"]
        got = dict(zip([n for n, _, _ in self.sim], st.hash_class("sim")))
        return [s for s, (cls, h) in cp.items() if cls == "sim" and int(h, 16) != got.get(s)]


def manifest_rows() -> list[dict]:
    with MANIFEST_TSV.open(newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def segment_dir(row: dict) -> pathlib.Path:
    return SEG_DIR / row["scenario"] / f"{int(row['segment']):02d}"
