r"""Reader, writer and checks for SimGolf clock / input traces (phase 2.3, 2026-10-18). Format: re/model/trace_schema.md.

A trace is what one deterministic run of the original consumed from outside the simulation: every main-thread clock call
(value and call site, in call order), every input delivery the harness made (virtual cursor, virtual key state, posted
messages, memory writes of fixtures, virtual timer deliveries), what the game read back (window messages, root-window
handler calls, key-state reads, the cursor point), the polls and checkpoint names, and one FRAME record per mainLoop frame.
Written by re/frida/js/trace_rec.js (records) and re/tools/trace_tool.py (header); used by trace_tool.py and the tests.
"""
from __future__ import annotations

import io
import json
import struct
from dataclasses import dataclass, field

MAGIC = b"SGTRACE\x00"
VERSION = 1
HEADER = struct.Struct("<8sHHIIIII")          # magic, version, header size, meta length, step, t0, qstep, q0
assert HEADER.size == 32

CLOCKRUN, FRAME, DELIVER, POLL, MSG, KEYSTATE, KBSTATE, CURSOR, HANDLER, TAKE, SLEEP, MAPVK, TIMERSET, END = \
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0x7F
TYPE_NAMES = {CLOCKRUN: "CLOCKRUN", FRAME: "FRAME", DELIVER: "DELIVER", POLL: "POLL", MSG: "MSG", KEYSTATE: "KEYSTATE",
              KBSTATE: "KBSTATE", CURSOR: "CURSOR", HANDLER: "HANDLER", TAKE: "TAKE", SLEEP: "SLEEP", MAPVK: "MAPVK",
              TIMERSET: "TIMERSET", END: "END"}
DELIVER_KINDS = {1: "cursor", 2: "vkey", 3: "post", 4: "w32", 5: "timer", 6: "note"}
DELIVER_FMT = {1: "<Iii", 2: "<III", 3: "<IIIIII", 4: "<III", 5: "<IIII", 6: "<I"}
# FRAME snapshot: (va, length) in order; 0x60 bytes
SNAPSHOT = [(0x00822D58, 0x40), (0x00822B98, 4), (0x00822B9C, 4), (0x0083821C, 4), (0x00838208, 4), (0x0083AB9C, 4),
            (0x0059E7B8, 4), (0x00834170, 4), (0x00822D9C, 4)]
SNAP_LEN = sum(n for _, n in SNAPSHOT)
FIXED = {CLOCKRUN: "<IIIIQ", FRAME: "<III%ds" % SNAP_LEN, MSG: "<IIII", KEYSTATE: "<IIII", CURSOR: "<Iii", HANDLER: "<III",
         TAKE: "<III", SLEEP: "<III", MAPVK: "<IIIII", TIMERSET: "<IIIIII", END: "<IIII"}
HANDLERS = {1: "rootWin_onPress1", 2: "rootWin_onPoint", 3: "rootWin_onUnpoint", 4: "rootWin_onPress2", 5: "rootWin_onPoint2",
            6: "rootWin_onUnpoint2", 7: "rootWin_onKey", 8: "rootWin_onChar", 9: "rootWin_onChar0", 10: "rootWin_onPress1b",
            11: "rootWin_onPress2b"}
HANDLER_VA = {1: 0x00465580, 2: 0x00465610, 3: 0x00465650, 4: 0x00465680, 5: 0x00465720, 6: 0x00465740, 7: 0x00465750,
              8: 0x004657B0, 9: 0x004657D0, 10: 0x004657E0, 11: 0x00465840}
TAKES = {1: "takeKey 0x0045ae50 ([0x00822b9c])", 2: "takeCode 0x0045ae30 ([0x00822b98])"}
TIMER_APIS = {1: "SetTimer", 2: "KillTimer", 3: "timeSetEvent", 4: "timeKillEvent"}
KEY_APIS = {1: "GetKeyState", 2: "GetAsyncKeyState"}
FRAME_RA = 0x0040FC2F


class TraceError(ValueError):
    pass


@dataclass
class Record:
    type: int
    sub: int
    payload: bytes
    offset: int = 0
    f: dict = field(default_factory=dict)       # decoded fields

    @property
    def name(self) -> str:
        return TYPE_NAMES.get(self.type, f"T{self.type:#x}")


@dataclass
class Trace:
    version: int
    step: int
    t0: int
    qstep: int
    q0: int
    meta: dict
    records: list[Record]
    size: int = 0


# ---------------------------------------------------------------- decoding
def decode(r: Record) -> dict:
    t, p = r.type, r.payload
    if t in FIXED:
        fmt = FIXED[t]
        if len(p) != struct.calcsize(fmt):
            raise TraceError(f"{r.name} at {r.offset:#x}: payload {len(p)} bytes, expected {struct.calcsize(fmt)}")
        v = struct.unpack(fmt, p)
        if t == CLOCKRUN:
            return {"kind": r.sub, "first": v[0], "count": v[1], "ra": v[2], "slot": v[3], "v0": v[4]}
        if t == FRAME:
            return {"frame": v[0], "pos": v[1], "t": v[2], "snap": v[3]}
        if t == MSG:
            return {"pos": v[0], "msg": v[1], "wp": v[2], "lp": v[3], "wp_is_timer": r.sub == 1}
        if t == KEYSTATE:
            return {"pos": v[0], "api": r.sub, "ra": v[1], "vk": v[2], "result": v[3]}
        if t == CURSOR:
            return {"pos": v[0], "ok": r.sub, "x": v[1], "y": v[2]}
        if t == HANDLER:
            return {"pos": v[0], "handler": r.sub, "a0": v[1], "a1": v[2]}
        if t == TAKE:
            return {"pos": v[0], "which": r.sub, "ra": v[1], "value": v[2]}
        if t == SLEEP:
            return {"pos": v[0], "ra": v[1], "ms": v[2]}
        if t == MAPVK:
            return {"pos": v[0], "ra": v[1], "code": v[2], "maptype": v[3], "result": v[4]}
        if t == TIMERSET:
            return {"pos": v[0], "api": r.sub, "ra": v[1], "handle": v[2], "period": v[3], "fn": v[4], "extra": v[5]}
        if t == END:
            return {"n": v[0], "frames": v[1], "polls": v[2], "dumps": v[3]}
    if t == DELIVER:
        fmt = DELIVER_FMT.get(r.sub)
        if fmt is None:
            raise TraceError(f"DELIVER at {r.offset:#x}: unknown kind {r.sub}")
        if len(p) != struct.calcsize(fmt):
            raise TraceError(f"DELIVER/{DELIVER_KINDS[r.sub]} at {r.offset:#x}: payload {len(p)} bytes")
        v = struct.unpack(fmt, p)
        d = {"pos": v[0], "kind": DELIVER_KINDS[r.sub]}
        if r.sub == 1:
            d.update(x=v[1], y=v[2])
        elif r.sub == 2:
            d.update(vk=v[1], st=v[2])
        elif r.sub == 3:
            d.update(cls=v[1], hwnd=v[2], msg=v[3], wp=v[4], lp=v[5])
        elif r.sub == 4:
            d.update(addr=v[1], v=v[2])
        elif r.sub == 5:
            d.update(tkind=v[1], handle=v[2], t=v[3])
        return d
    if t == POLL:
        if len(p) < 9 or len(p) != 9 + p[8]:
            raise TraceError(f"POLL at {r.offset:#x}: bad length {len(p)}")
        pos, idx = struct.unpack_from("<II", p)
        name = p[9:].decode("latin-1") if r.sub & 1 else None
        return {"pos": pos, "index": idx, "checkpoint": name, "final": bool(r.sub & 2)}
    if t == KBSTATE:
        if len(p) < 10:
            raise TraceError(f"KBSTATE at {r.offset:#x}: short")
        pos, ra, n = struct.unpack_from("<IIH", p)
        if len(p) != 10 + 2 * n:
            raise TraceError(f"KBSTATE at {r.offset:#x}: {n} pairs but {len(p)} bytes")
        return {"pos": pos, "ra": ra, "ok": r.sub, "keys": [(p[10 + 2 * i], p[11 + 2 * i]) for i in range(n)]}
    raise TraceError(f"unknown record type {t:#x} at {r.offset:#x}")


def parse(data: bytes, decode_fields=True) -> Trace:
    if len(data) < HEADER.size:
        raise TraceError("file shorter than the header")
    magic, ver, hsize, mlen, step, t0, qstep, q0 = HEADER.unpack_from(data)
    if magic != MAGIC:
        raise TraceError(f"bad magic {magic!r}")
    if ver != VERSION:
        raise TraceError(f"version {ver}, this reader knows {VERSION}")
    if hsize != HEADER.size:
        raise TraceError(f"header size {hsize}")
    try:
        meta = json.loads(data[hsize:hsize + mlen].decode("utf-8"))
    except Exception as e:  # noqa: BLE001
        raise TraceError(f"metadata: {e}") from e
    if "q0_64" in meta:      # phase 3.8 (38f): a slice whose first QPC value needs more than 32 bits (header_bytes)
        if meta["q0_64"] & 0xFFFFFFFF != q0:
            raise TraceError(f"meta q0_64 0x{meta['q0_64']:x} does not end in the header's q0 0x{q0:08x}")
        q0 = meta["q0_64"]
    off = hsize + mlen
    recs = []
    while off < len(data):
        if off + 4 > len(data):
            raise TraceError(f"truncated record header at {off:#x}")
        t, sub, n = data[off], data[off + 1], struct.unpack_from("<H", data, off + 2)[0]
        if off + 4 + n > len(data):
            raise TraceError(f"record at {off:#x} runs past the end")
        r = Record(t, sub, bytes(data[off + 4:off + 4 + n]), off)
        if decode_fields:
            r.f = decode(r)
        recs.append(r)
        off += 4 + n
    return Trace(ver, step, t0, qstep, q0, meta, recs, len(data))


def read(path) -> Trace:
    with open(path, "rb") as f:
        return parse(f.read())


# ---------------------------------------------------------------- encoding
def header_bytes(meta: dict, step: int, t0: int, qstep: int, q0: int) -> bytes:
    """The header and metadata. A q0 of more than 32 bits (a slice that starts late in a long run: the QPC count grows
    10,000 per call, so course_season_6y passes 2^32 after about 40,000 frames) keeps its low 32 bits in the header field and
    the whole value in meta `q0_64` (phase 3.8, stream 38f); parse() reads it back. Traces with a 32-bit q0 are unchanged."""
    if q0 > 0xFFFFFFFF:
        meta = {**meta, "q0_64": q0}
    m = json.dumps(meta, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return HEADER.pack(MAGIC, VERSION, HEADER.size, len(m), step, t0, qstep, q0 & 0xFFFFFFFF) + m


def rec(t: int, sub: int, payload: bytes) -> bytes:
    if len(payload) > 0xFFFF:
        raise TraceError("payload too long")
    return struct.pack("<BBH", t, sub, len(payload)) + payload


class Writer:
    """Builds a trace in memory (used by the tests' synthetic traces; the recorder builds the same bytes in JS)."""

    def __init__(self, meta: dict, step=1, t0=0x00100000, qstep=10000, q0=0x10000000):
        self.head = header_bytes(meta, step, t0, qstep, q0)
        self.buf = io.BytesIO()
        self.step, self.t0, self.qstep, self.q0 = step, t0, qstep, q0
        self.n = 0
        self.tcount = 0
        self.qcount = 0
        self.frames = 0
        self.polls = 0
        self.dumps = 0

    def clock(self, ra: int, slot: int, kind: int, count: int):
        """`count` consecutive calls from one site on the virtual clock (kind 1 timeGetTime, 2 QPC)."""
        if kind == 1:
            v0 = self.t0 + self.tcount * self.step
            self.tcount += count
        else:
            v0 = self.q0 + self.qcount * self.qstep
            self.qcount += count
        self.buf.write(rec(CLOCKRUN, kind, struct.pack("<IIIIQ", self.n, count, ra, slot, v0)))
        self.n += count
        return v0

    def frame(self, snap: bytes = b"\0" * SNAP_LEN):
        t = self.clock(FRAME_RA, 0, 1, 1)
        self.buf.write(rec(FRAME, 0, struct.pack("<III", self.frames, self.n, t) + snap))
        self.frames += 1

    def deliver(self, kind: str, *vals):
        k = {v: k for k, v in DELIVER_KINDS.items()}[kind]
        self.buf.write(rec(DELIVER, k, struct.pack(DELIVER_FMT[k], self.n, *vals)))

    def poll(self, checkpoint: str | None = None, final=False):
        nb = (checkpoint or "").encode("latin-1")
        sub = (1 if checkpoint is not None else 0) | (2 if final else 0)
        self.buf.write(rec(POLL, sub, struct.pack("<IIB", self.n, self.polls, len(nb)) + nb))
        self.polls += 1
        if checkpoint is not None:
            self.dumps += 1

    def raw(self, t: int, sub: int, fmt: str, *vals):
        """A record whose payload is u32 pos (the current clock count) followed by `fmt` (without the pos)."""
        self.buf.write(rec(t, sub, struct.pack("<I" + fmt.lstrip("<"), self.n, *vals)))

    def end(self) -> bytes:
        self.buf.write(rec(END, 0, struct.pack("<IIII", self.n, self.frames, self.polls, self.dumps)))
        return self.head + self.buf.getvalue()


# ---------------------------------------------------------------- checks
def validate(tr: Trace, virtual_clock=True) -> list[str]:
    """Schema rules (re/model/trace_schema.md section 4). Returns a list of violations (empty = valid)."""
    errs: list[str] = []
    slots = tr.meta.get("slots") or []
    n = 0                        # clock calls so far
    tcount = qcount = 0
    last_ra = None
    frame_i = poll_i = dumps = 0
    last_pos = 0
    finals = 0
    ended = False
    for i, r in enumerate(tr.records):
        if ended:
            errs.append(f"record {i} ({r.name}) after END")
            break
        f = r.f or decode(r)
        if r.type == CLOCKRUN:
            if f["first"] != n:
                errs.append(f"record {i}: CLOCKRUN first {f['first']} != {n}")
            if f["count"] < 1:
                errs.append(f"record {i}: CLOCKRUN count 0")
            if f["kind"] not in (1, 2):
                errs.append(f"record {i}: CLOCKRUN kind {f['kind']}")
            if slots:
                if f["slot"] >= len(slots):
                    errs.append(f"record {i}: slot {f['slot']} outside the slot table")
                elif {"tgt": 1, "qpc": 2}.get(slots[f["slot"]][1]) != f["kind"]:
                    errs.append(f"record {i}: slot {f['slot']} is {slots[f['slot']][1]}, run kind {f['kind']}")
            if virtual_clock:
                want = tr.t0 + tcount * tr.step if f["kind"] == 1 else tr.q0 + qcount * tr.qstep
                if f["v0"] != want:
                    errs.append(f"record {i}: CLOCKRUN v0 {f['v0']:#x} != virtual clock {want:#x}")
            if f["kind"] == 1:
                tcount += f["count"]
            else:
                qcount += f["count"]
            n += f["count"]
            last_ra = f["ra"]
            continue
        pos = f.get("pos")
        if pos is not None:
            if pos > n:
                errs.append(f"record {i} ({r.name}): pos {pos} past the {n} clock calls before it")
            if pos < last_pos:
                errs.append(f"record {i} ({r.name}): pos {pos} < previous {last_pos}")
            last_pos = max(last_pos, pos)
        if r.type == FRAME:
            if f["frame"] != frame_i:
                errs.append(f"record {i}: FRAME {f['frame']} != {frame_i}")
            if f["pos"] != n or last_ra != tr.meta.get("frame_ra", FRAME_RA):
                errs.append(f"record {i}: FRAME not right after a frame-head clock call")
            frame_i += 1
        elif r.type == POLL:
            if f["index"] != poll_i:
                errs.append(f"record {i}: POLL index {f['index']} != {poll_i}")
            poll_i += 1
            dumps += f["checkpoint"] is not None
            finals += f["final"]
        elif r.type == END:
            ended = True
            if f["n"] != n:
                errs.append(f"END n {f['n']} != {n} clock calls")
            if f["frames"] != frame_i or f["polls"] != poll_i or f["dumps"] != dumps:
                errs.append(f"END counts {f['frames']}/{f['polls']}/{f['dumps']} != {frame_i}/{poll_i}/{dumps}")
    if not ended:
        errs.append("no END record")
    if finals != 1:
        errs.append(f"{finals} final polls (expected 1)")
    return errs


def clock_calls(tr: Trace):
    """Yield (ordinal, ra, slot, kind, value) for every clock call."""
    for r in tr.records:
        if r.type == CLOCKRUN:
            f = r.f
            st = tr.step if f["kind"] == 1 else tr.qstep
            for k in range(f["count"]):
                yield f["first"] + k, f["ra"], f["slot"], f["kind"], f["v0"] + k * st


def snap_fields(snap: bytes) -> dict:
    out, o = {}, 0
    for va, n in SNAPSHOT:
        out[va] = snap[o:o + n]
        o += n
    return {"root_input": out[0x00822D58], "code": struct.unpack("<I", out[0x00822B98])[0],
            "key": struct.unpack("<I", out[0x00822B9C])[0], "char": struct.unpack("<I", out[0x0083821C])[0],
            "key838208": struct.unpack("<I", out[0x00838208])[0], "ui83ab9c": out[0x0083AB9C][0],
            "flags": struct.unpack("<I", out[0x0059E7B8])[0], "date": struct.unpack("<I", out[0x00834170])[0],
            "rng": struct.unpack("<I", out[0x00822D9C])[0]}


def summary(tr: Trace) -> dict:
    from collections import Counter
    types = Counter(r.name for r in tr.records)
    bytes_by = Counter()
    for r in tr.records:
        bytes_by[r.name] += 4 + len(r.payload)
    sites = Counter()
    tgt = qpc = 0
    for r in tr.records:
        if r.type == CLOCKRUN:
            sites[(r.f["ra"], r.f["kind"])] += r.f["count"]
            if r.f["kind"] == 1:
                tgt += r.f["count"]
            else:
                qpc += r.f["count"]
    frames = [r.f for r in tr.records if r.type == FRAME]
    per_frame, dt, ddate = [], [], []
    for a, b in zip(frames, frames[1:]):
        per_frame.append(b["pos"] - a["pos"])
        dt.append(b["t"] - a["t"])
        ddate.append(snap_fields(b["snap"])["date"] - snap_fields(a["snap"])["date"])
    paused = sum(1 for fr in frames if snap_fields(fr["snap"])["flags"] & 4)

    def stats(xs):
        if not xs:
            return None
        s = sorted(xs)
        return {"min": s[0], "median": s[len(s) // 2], "max": s[-1], "mean": round(sum(s) / len(s), 2)}

    deliv = Counter(r.f["kind"] for r in tr.records if r.type == DELIVER)
    msgs = Counter(f"{r.f['msg']:#06x}" for r in tr.records if r.type == MSG)
    handlers = Counter(HANDLERS.get(r.f["handler"], str(r.f["handler"])) for r in tr.records if r.type == HANDLER)
    polls = [r.f for r in tr.records if r.type == POLL]
    end = next((r.f for r in tr.records if r.type == END), None)
    return {"size": tr.size, "records": dict(types), "bytes": dict(bytes_by), "clock_calls": tgt + qpc, "tgt": tgt, "qpc": qpc,
            "frames": len(frames), "polls": len(polls), "checkpoints": [p["checkpoint"] for p in polls if p["checkpoint"]],
            "clock_calls_per_frame": stats(per_frame), "virtual_ms_per_frame": stats(dt), "date_step_per_frame": stats(ddate),
            "paused_frames": paused,
            "top_sites": [[f"{ra:#010x}", {1: "tgt", 2: "qpc"}[k], c] for (ra, k), c in sites.most_common(12)],
            "deliveries": dict(deliv), "messages": dict(msgs), "handlers": dict(handlers), "end": end}


def fmt_record(r: Record) -> str:
    f = dict(r.f)
    if r.type == FRAME:
        s = snap_fields(f.pop("snap"))
        f.update({k: (f"{v:#x}" if isinstance(v, int) else v.hex()) for k, v in s.items()})
    for k in ("ra", "addr", "hwnd"):
        if k in f and isinstance(f[k], int):
            f[k] = f"{f[k]:#010x}"
    if r.type == HANDLER:
        f["handler"] = HANDLERS.get(f["handler"], f["handler"])
    if r.type == MSG:
        f["msg"] = f"{f['msg']:#06x}"
    return f"{r.offset:#010x} {r.name:9s} " + " ".join(f"{k}={v}" for k, v in f.items())


def diff(a: Trace, b: Trace, context=3) -> dict:
    """First difference between two traces (header, metadata, then records in order)."""
    if (a.version, a.step, a.t0, a.qstep, a.q0) != (b.version, b.step, b.t0, b.qstep, b.q0):
        return {"same": False, "where": "header", "a": [a.version, a.step, a.t0, a.qstep, a.q0],
                "b": [b.version, b.step, b.t0, b.qstep, b.q0]}
    if a.meta != b.meta:
        keys = sorted(k for k in set(a.meta) | set(b.meta) if a.meta.get(k) != b.meta.get(k))
        return {"same": False, "where": "meta", "keys": keys}
    frame = -1
    for i, (ra, rb) in enumerate(zip(a.records, b.records)):
        if ra.type == FRAME:
            frame = ra.f["frame"]
        if (ra.type, ra.sub, ra.payload) != (rb.type, rb.sub, rb.payload):
            lo = max(0, i - context)
            return {"same": False, "where": "record", "index": i, "frame": frame,
                    "a": [fmt_record(r) for r in a.records[lo:i + context + 1]],
                    "b": [fmt_record(r) for r in b.records[lo:i + context + 1]]}
    if len(a.records) != len(b.records):
        return {"same": False, "where": "length", "a": len(a.records), "b": len(b.records)}
    return {"same": True, "records": len(a.records)}


# ---------------------------------------------------------------- replay plan (consumed by trace_rec.js in replay mode)
def replay_plan(tr: Trace) -> tuple[bytes, list[dict], list[str]]:
    """(plan bytes: u32 N, RA[N], VLO[N], VHI[N]; deliveries in order for trace_rec.js; checkpoint list per poll).
    A delivery's `t` is the virtual timeGetTime value the agent's clock D+0 holds at that position (the last timeGetTime
    value before it plus one step), which scenario_det.js passes to a TIMERPROC and reports with a poll."""
    ras, vlo, vhi = [], [], []
    last_tgt = tr.t0 - tr.step
    deliveries, polls = [], []
    for r in tr.records:
        if r.type == CLOCKRUN:
            f = r.f
            st = tr.step if f["kind"] == 1 else tr.qstep
            for k in range(f["count"]):
                v = f["v0"] + k * st
                ras.append(f["ra"] if f["ra"] < 0x80000000 else 0)     # DLL sites (module-relative) are not checked natively
                vlo.append(v & 0xFFFFFFFF)
                vhi.append(v >> 32)
                if f["kind"] == 1:
                    last_tgt = v
            continue
        f = r.f
        if r.type == DELIVER:
            pos = f["pos"]
            if f["kind"] == "timer":
                deliveries.append({"pos": pos, "k": "timer", "tkind": f["tkind"], "id": f["handle"], "t": f["t"]})
            else:
                a = {"k": f["kind"]}
                if f["kind"] == "cursor":
                    a.update(x=f["x"], y=f["y"])
                elif f["kind"] == "vkey":
                    a.update(vk=f["vk"], st=f["st"])
                elif f["kind"] == "post":
                    a.update(msg=f["msg"], wp=f["wp"], lp=f["lp"])
                    if f["cls"] == 2:
                        a["hwnd"] = f["hwnd"]
                elif f["kind"] == "w32":
                    a.update(addr=f["addr"], v=f["v"])
                deliveries.append({"pos": pos, "k": "act", "a": a, "cls": f.get("cls", -1)})
        elif r.type == POLL:
            deliveries.append({"pos": f["pos"], "k": "poll", "t": (last_tgt + tr.step) & 0xFFFFFFFF})
            polls.append(f["checkpoint"] if not f["final"] else "<final>")
    n = len(ras)
    plan = struct.pack("<I", n) + struct.pack(f"<{n}I", *ras) + struct.pack(f"<{n}I", *vlo) + struct.pack(f"<{n}I", *vhi)
    return plan, deliveries, polls
