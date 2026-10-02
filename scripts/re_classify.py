"""Confidence ladder enforcement (C0..C4) for SimGolf RE. The rubric is re/CONFIDENCE.md; this is the gate.

    py -3.12 scripts/re_classify.py check   0x004490d0 --to C3
    py -3.12 scripts/re_classify.py promote 0x004490d0 --to C1 --name Terrain::tileAt --subsystem terrain \
            --evidence "export ?tileAt@Terrain@@QAEPAVTile@@HH@Z ordinal 4"
    py -3.12 scripts/re_classify.py promote 0x004490d0 --to C2 --note re/analysis/terrain/004490d0_tileAt.md \
            --callers none --callees none
    py -3.12 scripts/re_classify.py demote  0x004490d0 --to C1 --reason "anchor changed"
    py -3.12 scripts/re_classify.py status

Promotion is one level at a time. Every refusal names the exact failing gate. On success the row in
hooks.csv is written and one line is inserted into re/analysis/CHANGELOG.md below the ENTRIES marker
(never rewritten). Exit code: 0 = promoted / all gates pass, 2 = refused, 1 = usage error.
"""
from __future__ import annotations

import argparse
import csv
import datetime
import pathlib
import re
import sys

DEFAULT_ROOT = pathlib.Path(__file__).resolve().parents[1]
LEVELS = ["C0", "C1", "C2", "C3", "C4"]
SUBSYSTEMS = {"boot", "frontend", "render", "terrain", "course", "golfer", "economy", "ai", "audio", "video",
              "input", "save", "ui", "sim", "util", "crt", "unknown"}
COLUMNS = ["addr", "module", "name", "subsystem", "confidence", "status", "note", "file", "callers", "callees",
           "frida_diff", "scenario", "notes"]
NOTE_SECTIONS_C2 = ["## Signature", "## Reads", "## Writes", "## Callees", "## Constants"]
BANNED = re.compile(r"\b(probably|likely|seems to|appears to|I think|presumably|might be|maybe)\b", re.I)
DEFAULT_NAME = re.compile(r"^(FUN_|thunk_FUN_|LAB_|SUB_|sub_)", re.I)
MARKER = "<!-- ENTRIES -->"
MIN_LEAF_VECTORS = 10
REPLAY_MIN = 100  # recorded real calls for replay to count as C3 verification (scripts/scoreboard.py agrees)


class Project:
    def __init__(self, root: pathlib.Path):
        self.root = root
        self.hooks = root / "hooks.csv"
        self.changelog = root / "re" / "analysis" / "CHANGELOG.md"
        self.uncert = root / "UNCERTAINTIES.md"
        self.fn_tsv = root / "re" / "functions_ghidra.tsv"
        self.diff_dir = root / "log" / "diff"
        self.shim_dll = root / "shim" / "build" / "winmm.dll"

    # ---------- hooks.csv ----------
    def rows(self) -> dict[str, dict]:
        if not self.hooks.exists():
            return {}
        with self.hooks.open(newline="") as f:
            return {r["addr"]: r for r in csv.DictReader(f)}

    def save(self, rows: dict[str, dict]):
        with self.hooks.open("w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=COLUMNS)
            w.writeheader()
            for k in sorted(rows):
                w.writerow({c: rows[k].get(c, "") for c in COLUMNS})

    def log(self, line: str):
        text = self.changelog.read_text() if self.changelog.exists() else f"# CHANGELOG (newest first)\n\n{MARKER}\n"
        if MARKER not in text:
            raise SystemExit(f"{self.changelog} lost its {MARKER} marker; refusing to write")
        self.changelog.parent.mkdir(parents=True, exist_ok=True)
        self.changelog.write_text(text.replace(MARKER, f"{MARKER}\n{line}", 1))

    # ---------- lookups ----------
    def ghidra_name(self, addr: str) -> str | None:
        if not self.fn_tsv.exists():
            return None
        for line in self.fn_tsv.read_text().splitlines()[1:]:
            parts = line.split("\t")
            if parts and parts[0].lower() == addr:
                return parts[1]
        return None

    def open_blocking_uncertainties(self, addr: str) -> list[str]:
        if not self.uncert.exists():
            return []
        out = []
        for line in self.uncert.read_text().splitlines():
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            if len(cells) >= 6 and cells[0].startswith("U-") and norm_addr(cells[1]) == addr \
                    and cells[2].lower() in ("semantic", "structural") and cells[5].lower() == "open":
                out.append(cells[0])
        return out

    def filed_uncertainty_ids(self) -> set[str]:
        if not self.uncert.exists():
            return set()
        return set(re.findall(r"\|\s*(U-\d{4})\s*\|", self.uncert.read_text()))

    def diff_csv(self, addr: str, name: str, kind: str) -> pathlib.Path | None:
        hits = sorted(self.diff_dir.glob(f"{addr}_*.{kind}.csv")) if self.diff_dir.exists() else []
        return hits[0] if hits else None


def norm_addr(a: str) -> str:
    a = a.strip().lower()
    if not a:
        return ""
    return f"{int(a, 16):08x}"


def read_diff(path: pathlib.Path) -> dict:
    rows = list(csv.reader(path.open(newline="")))
    verdict = next((r for r in rows if r and r[0] == "VERDICT"), None)
    witness = next((r for r in rows if r and r[0] == "install_witness"), None)
    vectors = [r for r in rows[1:] if r and r[0] not in ("VERDICT", "install_witness", "meta")]
    return {"green": bool(verdict and verdict[1] == "GREEN"), "witness": bool(witness and witness[-1] == "True"),
            "vectors": len(vectors)}


# ---------------------------------------------------------------- gates
def gates(p: Project, row: dict, target: str, args) -> list[tuple[bool, str]]:
    """Return [(ok, description)] for the gate of `target` (the level being entered)."""
    addr = row["addr"]
    res: list[tuple[bool, str]] = []
    cur = row.get("confidence") or "C0"
    res.append((LEVELS.index(target) == LEVELS.index(cur) + 1,
                f"one level at a time ({cur} -> {target})"))

    if target == "C1":
        name = row.get("name", "")
        res.append((bool(name) and not DEFAULT_NAME.match(name), f"non-default name ({name or 'missing'})"))
        res.append((row.get("subsystem") in SUBSYSTEMS, f"subsystem in {sorted(SUBSYSTEMS)} ({row.get('subsystem') or 'missing'})"))
        res.append((bool(row.get("notes")), "evidence pointer recorded (--evidence)"))
        if row.get("module", "golf_clean.exe") == "golf_clean.exe" and p.fn_tsv.exists():
            res.append((p.ghidra_name(addr) is not None, "a function exists at this address in re/functions_ghidra.tsv"))

    if target in ("C2", "C3", "C4"):
        note = p.root / row.get("note", "") if row.get("note") else None
        ok = bool(note and note.is_file())
        res.append((ok, f"analysis note exists ({row.get('note') or 'missing: --note'})"))
        if ok:
            text = note.read_text()
            for sec in NOTE_SECTIONS_C2:
                res.append((sec in text, f"note has '{sec}'"))
            bad = [l for l in text.splitlines() if BANNED.search(l) and "[UNCERTAIN" not in l]
            res.append((not bad, "NO-GUESSING: no banned hedge words outside [UNCERTAIN] lines"
                        + (f" -> {bad[0].strip()[:80]!r}" if bad else "")))
            unfiled = re.findall(r"\[UNCERTAIN(?!\s+U-\d{4})[^\]]*\]", text)
            res.append((not unfiled, "every [UNCERTAIN] marker is filed as [UNCERTAIN U-NNNN]"))
            ids = set(re.findall(r"\[UNCERTAIN\s+(U-\d{4})", text))
            missing = ids - p.filed_uncertainty_ids()
            res.append((not missing, f"referenced uncertainties exist in UNCERTAINTIES.md {sorted(missing) or ''}".rstrip()))
        res.append((bool(row.get("callers")), "callers recorded (--callers 'none' or ';'-separated addrs/names)"))
        res.append((bool(row.get("callees")), "callees recorded (--callees 'none' for a leaf)"))

    if target in ("C3", "C4"):
        text = (p.root / row["note"]).read_text() if row.get("note") and (p.root / row["note"]).is_file() else ""
        purpose = text.split("## Purpose", 1)[1].split("\n## ", 1)[0] if "## Purpose" in text else ""
        res.append((bool(purpose.strip()) and bool(re.search(r"0x[0-9a-fA-F]{6,8}", purpose)),
                    "## Purpose is present and cites an address"))
        src = p.root / row.get("file", "") if row.get("file") else None
        src_ok = bool(src and src.is_file() and "shim/src/re/" in row["file"].replace("\\", "/"))
        res.append((src_ok, f"reimplementation under shim/src/re/ ({row.get('file') or 'missing: --file'})"))
        if src_ok:
            body = src.read_text()
            res.append((re.search(rf"//\s*0x{addr}\b", body, re.I) is not None, f"source has a '// 0x{addr}' comment"))
            res.append((re.search(rf"SG_HOOK\([^)]*0x{addr}\b", body, re.I) is not None, f"SG_HOOK registered at 0x{addr}"))
            res.append((p.shim_dll.exists() and p.shim_dll.stat().st_mtime >= src.stat().st_mtime,
                        "shim build is newer than the source (run shim\\build.bat)"))
        # Verified: a GREEN path-1 A/B (hand vectors) or a GREEN replay of >= REPLAY_MIN recorded real calls.
        d = p.diff_csv(addr, row.get("name", ""), "path1")
        info = read_diff(d) if d else None
        rp = p.diff_csv(addr, row.get("name", ""), "replay")
        rinfo = read_diff(rp) if rp else None
        replay_ok = bool(rinfo and rinfo["green"] and rinfo["vectors"] >= REPLAY_MIN)
        res.append((bool(info and info["green"]) or replay_ok,
                    f"verified: path-1 A/B GREEN ({d.name if d else 'none'}) or replay GREEN >= {REPLAY_MIN} real calls "
                    f"({rp.name + ': ' + str(rinfo['vectors']) if rp else 'none'})"))
        res.append((bool(info and info["witness"]) or bool(rinfo and rinfo["witness"]),
                    "install witness (0xE9 at the address, hook live in-process) recorded in an A/B or replay CSV"))
        res += anti_island(p, row, info if (info and info["green"]) else rinfo)
        blocking = p.open_blocking_uncertainties(addr)
        res.append((not blocking, f"no open semantic/structural uncertainties {blocking or ''}".rstrip()))

    if target == "C4":
        # Either: the game with the hook live behaves the same (scenario ON/OFF), or the decompiled source
        # recompiles with the original compiler to the original instructions (100% match, re/tools/match.py).
        d = p.diff_csv(addr, row.get("name", ""), "scenario")
        info = read_diff(d) if d else None
        m = p.diff_csv(addr, row.get("name", ""), "match")
        minfo = read_diff(m) if m else None
        res.append((bool(info and info["green"] and info["witness"]) or bool(minfo and minfo["green"]),
                    f"scenario ON/OFF GREEN with install witness ({d.name if d else 'none'}) or 100% match with "
                    f"the original compiler ({m.name if m else 'none'})"))
        src = p.root / row.get("file", "") if row.get("file") else None
        res.append((bool(src and src.is_file() and "STUB" not in src.read_text()), "no STUB marker in the reimplementation"))
    return res


def _identified(p: Project, item: str, rows: dict) -> bool:
    item = item.strip()
    if re.fullmatch(r"(0x)?[0-9a-fA-F]{6,8}", item):
        a = norm_addr(item)
        r = rows.get(a)
        if r and LEVELS.index(r.get("confidence") or "C0") >= 2:
            return True
        name = (r or {}).get("name") or p.ghidra_name(a) or ""
        return bool(name) and not DEFAULT_NAME.match(name)
    return bool(item) and not DEFAULT_NAME.match(item)


def anti_island(p: Project, row: dict, diff: dict | None) -> list[tuple[bool, str]]:
    rows = p.rows()
    callers = [c for c in (row.get("callers") or "").split(";") if c.strip()]
    callees = [c for c in (row.get("callees") or "").split(";") if c.strip()]
    out = []
    if callers == ["none"] or not callers:
        out.append((False, "anti-island (callers): no caller and no identified caller; dead code stays at C2"))
    else:
        out.append((any(_identified(p, c, rows) for c in callers),
                    f"anti-island (callers): one caller at C2+ or identified ({';'.join(callers)})"))
    if callees == ["none"]:
        n = diff["vectors"] if diff else 0
        out.append((n >= MIN_LEAF_VECTORS, f"leaf exemption: no callees and A/B covers >= {MIN_LEAF_VECTORS} vectors ({n})"))
    elif callees == ["indirect"]:
        out.append((bool(diff and diff["green"]), "indirect-dispatch exemption: A/B exercised the dispatch (recording stub)"))
    else:
        out.append((any(_identified(p, c, rows) for c in callees),
                    f"anti-island (callees): one callee at C2+ or identified ({';'.join(callees)})"))
    return out


# ---------------------------------------------------------------- commands
def apply_args(row: dict, a):
    for k in ("name", "subsystem", "note", "file", "callers", "callees", "module"):
        v = getattr(a, k, None)
        if v:
            row[k] = v.replace("\\", "/") if k in ("note", "file") else v
    if getattr(a, "evidence", None):
        row["notes"] = (row.get("notes", "") + "; " if row.get("notes") else "") + a.evidence


def cmd_check(p: Project, a, write: bool) -> int:
    addr = norm_addr(a.addr)
    rows = p.rows()
    row = dict(rows.get(addr) or {"addr": addr, "module": "golf_clean.exe", "confidence": "C0"})
    apply_args(row, a)
    results = gates(p, row, a.to, a)
    for ok, desc in results:
        print(f"  [{'PASS' if ok else 'FAIL'}] {desc}")
    passed = all(ok for ok, _ in results)
    if not write:
        print(f"{addr} {row.get('confidence') or 'C0'} -> {a.to}: {'all gates pass' if passed else 'REFUSED'}")
        return 0 if passed else 2
    if not passed:
        print(f"REFUSED: {addr} stays {row.get('confidence') or 'C0'}")
        return 2
    old = row.get("confidence") or "C0"
    row["confidence"] = a.to
    row["status"] = {"C1": "located", "C2": "transcribed", "C3": "impl", "C4": "verified"}[a.to]
    d = p.diff_csv(addr, row.get("name", ""), "path1")
    if d:
        row["frida_diff"] = d.relative_to(p.root).as_posix()
    s = p.diff_csv(addr, row.get("name", ""), "scenario")
    if s:
        row["scenario"] = s.relative_to(p.root).as_posix()
    rows[addr] = row
    p.save(rows)
    ev = row.get("frida_diff") if a.to == "C3" else (row.get("scenario") if a.to == "C4" else row.get("note") or row.get("notes"))
    p.log(f"{datetime.date.today()}  {addr}  {row.get('name')}  {old}->{a.to}  {ev}")
    print(f"PROMOTED {addr} {row.get('name')}: {old} -> {a.to}")
    return 0


def cmd_demote(p: Project, a) -> int:
    addr = norm_addr(a.addr)
    rows = p.rows()
    if addr not in rows:
        print(f"{addr} not in hooks.csv")
        return 1
    row = rows[addr]
    old = row.get("confidence") or "C0"
    if LEVELS.index(a.to) >= LEVELS.index(old):
        print(f"demote must lower the level ({old} -> {a.to})")
        return 1
    row["confidence"] = a.to
    row["notes"] = (row.get("notes", "") + "; " if row.get("notes") else "") + f"demoted {old}->{a.to}: {a.reason}"
    p.save(rows)
    p.log(f"{datetime.date.today()}  {addr}  {row.get('name')}  {old}<-{a.to}  DEMOTED: {a.reason}")
    print(f"DEMOTED {addr}: {old} -> {a.to}")
    return 0


def cmd_status(p: Project, a) -> int:
    rows = p.rows()
    from collections import Counter
    c = Counter(r.get("confidence") or "C0" for r in rows.values())
    total = sum(1 for _ in open(p.fn_tsv)) - 1 if p.fn_tsv.exists() else 0
    print(f"golf_clean.exe functions in Ghidra: {total}")
    print("tracked: " + ", ".join(f"{lvl}={c.get(lvl, 0)}" for lvl in LEVELS))
    for r in sorted(rows.values(), key=lambda r: r["addr"]):
        print(f"  {r['addr']}  {r.get('confidence'):3}  {r.get('subsystem', ''):9} {r.get('name', '')}")
    return 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", type=pathlib.Path, default=DEFAULT_ROOT)
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name in ("check", "promote"):
        s = sub.add_parser(name)
        s.add_argument("addr")
        s.add_argument("--to", required=True, choices=LEVELS[1:])
        for opt in ("name", "subsystem", "note", "file", "callers", "callees", "module", "evidence"):
            s.add_argument(f"--{opt}")
    d = sub.add_parser("demote")
    d.add_argument("addr")
    d.add_argument("--to", required=True, choices=LEVELS)
    d.add_argument("--reason", required=True)
    sub.add_parser("status")
    a = ap.parse_args(argv)
    p = Project(a.root)
    if a.cmd == "check":
        return cmd_check(p, a, write=False)
    if a.cmd == "promote":
        return cmd_check(p, a, write=True)
    if a.cmd == "demote":
        return cmd_demote(p, a)
    return cmd_status(p, a)


if __name__ == "__main__":
    sys.exit(main())
