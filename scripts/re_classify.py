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
              "input", "save", "ui", "sim", "net", "util", "crt", "unknown"}
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
        self.fn_tsvs = {"golf_clean.exe": self.fn_tsv}
        for m in ("Terrain.dll", "jgld.dll", "sound.dll"):
            self.fn_tsvs[m] = root / "re" / f"functions_ghidra_{m}.tsv"
        self._names = {}
        self.pending_log = []
        self.diff_dir = root / "log" / "diff"
        self.shim_dll = root / "shim" / "build" / "winmm.dll"

    # ---------- hooks.csv ----------
    def rows(self) -> dict[tuple[str, str], dict]:
        """Keyed by (module, addr): the three DLLs share the image base 0x10000000, so an RVA alone is ambiguous."""
        if not self.hooks.exists():
            return {}
        with self.hooks.open(newline="") as f:
            return {(r.get("module") or "golf_clean.exe", r["addr"]): r for r in csv.DictReader(f)}

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
    def ghidra_name(self, addr: str, module: str = "golf_clean.exe") -> str | None:
        """Name in the module's Ghidra function list (exe: VA; DLLs: RVA, as in hooks.csv)."""
        if module not in self._names:
            f = self.fn_tsvs.get(module)
            self._names[module] = {} if not (f and f.exists()) else {
                l.split("\t")[0].lower(): l.split("\t")[1]
                for l in f.read_text().splitlines()[1:] if "\t" in l}
        return self._names[module].get(addr)

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

    def diff_csvs(self, addr: str, kind: str, module: str = "golf_clean.exe") -> list[pathlib.Path]:
        """Every CSV of one function. DLL RVAs collide across jgld.dll / sound.dll / Terrain.dll, so a DLL's CSVs carry
        its stem after the address (<rva>_jgld_<key>, as diff_hook.py writes them); exe VAs (>= 0x401000) cannot
        collide with DLL RVAs."""
        if not self.diff_dir.exists():
            return []
        if (module or "golf_clean.exe").lower() == "golf_clean.exe":
            return sorted(self.diff_dir.glob(f"{addr}_*.{kind}.csv"))
        return sorted(self.diff_dir.glob(f"{addr}_{module.rsplit('.', 1)[0]}_*.{kind}.csv"))


def norm_addr(a: str) -> str:
    a = a.strip().lower()
    if not a:
        return ""
    return f"{int(a, 16):08x}"


def read_diff(path: pathlib.Path) -> dict:
    rows = list(csv.reader(path.open(newline="")))
    verdict = next((r for r in rows if r and r[0] == "VERDICT"), None)
    witness = next((r for r in rows if r and r[0] == "install_witness"), None)
    # summary rows written after the vectors (diff_hook: state_regions, distinct_results) are not vectors
    vectors = [r for r in rows[1:] if r and r[0] not in ("VERDICT", "install_witness", "meta", "state_regions",
                                                        "distinct_results")]
    return {"green": bool(verdict and verdict[1] == "GREEN"), "witness": bool(witness and witness[-1] == "True"),
            "vectors": len(vectors)}


def read_path1(paths: list[pathlib.Path]) -> dict | None:
    """All path-1 CSVs of one address (one per registry key: fixture variants, guard-side keys) as one result:
    GREEN and witnessed only if every key is, vectors summed."""
    infos = [read_diff(x) for x in paths]
    if not infos:
        return None
    return {"green": all(i["green"] for i in infos), "witness": all(i["witness"] for i in infos),
            "vectors": sum(i["vectors"] for i in infos)}


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
        mod = row.get("module") or "golf_clean.exe"
        if p.fn_tsvs.get(mod) and p.fn_tsvs[mod].exists():
            res.append((p.ghidra_name(addr, mod) is not None,
                        f"a function exists at this address in {p.fn_tsvs[mod].relative_to(p.root).as_posix()}"))

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
        ds = p.diff_csvs(addr, "path1", row.get("module") or "golf_clean.exe")
        d = ds[0] if ds else None
        info = read_path1(ds)
        rp = p.diff_csv(addr, row.get("name", ""), "replay")
        rinfo = read_diff(rp) if rp else None
        replay_ok = bool(rinfo and rinfo["green"] and rinfo["vectors"] >= REPLAY_MIN)
        res.append((bool(info and info["green"]) or replay_ok,
                    f"verified: path-1 A/B GREEN ({(d.name + (f' +{len(ds) - 1} keys' if len(ds) > 1 else '')) if d else 'none'}) or replay GREEN >= {REPLAY_MIN} real calls "
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


def _identified(p: Project, item: str, rows: dict, module: str = "golf_clean.exe") -> bool:
    item = item.strip()
    if re.fullmatch(r"(0x)?[0-9a-fA-F]{6,8}", item):
        a = norm_addr(item)
        if module != "golf_clean.exe" and int(a, 16) >= 0x10000000:
            a = f"{int(a, 16) - 0x10000000:08x}"   # DLL callers are listed as VAs, DLL rows are keyed by RVA
        r = rows.get((module, a))
        if r and LEVELS.index(r.get("confidence") or "C0") >= 2:
            return True
        name = (r or {}).get("name") or p.ghidra_name(a, module) or ""
        return bool(name) and not DEFAULT_NAME.match(name)
    return bool(item) and not DEFAULT_NAME.match(item)


def anti_island(p: Project, row: dict, diff: dict | None) -> list[tuple[bool, str]]:
    rows = p.rows()
    mod = row.get("module") or "golf_clean.exe"
    callers = [c for c in (row.get("callers") or "").split(";") if c.strip()]
    callees = [c for c in (row.get("callees") or "").split(";") if c.strip()]
    out = []
    if callers == ["none"] or not callers:
        out.append((False, "anti-island (callers): no caller and no identified caller; dead code stays at C2"))
    else:
        out.append((any(_identified(p, c, rows, mod) for c in callers),
                    f"anti-island (callers): one caller at C2+ or identified ({';'.join(callers)})"))
    if callees == ["none"]:
        n = diff["vectors"] if diff else 0
        out.append((n >= MIN_LEAF_VECTORS, f"leaf exemption: no callees and A/B covers >= {MIN_LEAF_VECTORS} vectors ({n})"))
    elif callees == ["indirect"]:
        out.append((bool(diff and diff["green"]), "indirect-dispatch exemption: A/B exercised the dispatch (recording stub)"))
    else:
        out.append((any(_identified(p, c, rows, mod) for c in callees),
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


def cmd_check(p: Project, a, write: bool, rows: dict | None = None, quiet: bool = False) -> int:
    addr = norm_addr(a.addr)
    mod = getattr(a, "module", None) or "golf_clean.exe"
    batch = rows is not None
    rows = p.rows() if rows is None else rows
    row = dict(rows.get((mod, addr)) or {"addr": addr, "module": mod, "confidence": "C0"})
    apply_args(row, a)
    results = gates(p, row, a.to, a)
    for ok, desc in results:
        if not quiet or not ok:
            print(f"  [{'PASS' if ok else 'FAIL'}] {desc}")
    passed = all(ok for ok, _ in results)
    if not write:
        print(f"{addr} {row.get('confidence') or 'C0'} -> {a.to}: {'all gates pass' if passed else 'REFUSED'}")
        return 0 if passed else 2
    if not passed:
        print(f"REFUSED: {mod} {addr} stays {row.get('confidence') or 'C0'}")
        return 2
    old = row.get("confidence") or "C0"
    row["confidence"] = a.to
    row["status"] = {"C1": "located", "C2": "transcribed", "C3": "impl", "C4": "verified"}[a.to]
    ds = p.diff_csvs(addr, "path1", mod)
    if ds:
        row["frida_diff"] = ds[0].relative_to(p.root).as_posix()
    s = p.diff_csv(addr, row.get("name", ""), "scenario")
    if s:
        row["scenario"] = s.relative_to(p.root).as_posix()
    rows[(mod, addr)] = row
    ev = row.get("frida_diff") if a.to == "C3" else (row.get("scenario") if a.to == "C4" else row.get("note") or row.get("notes"))
    where = addr if mod == "golf_clean.exe" else f"{mod}:{addr}"
    if batch:
        p.pending_log.append(f"{datetime.date.today()}  {where}  {row.get('name')}  {old}->{a.to}  {ev}")
    else:
        p.save(rows)
        p.log(f"{datetime.date.today()}  {where}  {row.get('name')}  {old}->{a.to}  {ev}")
    if not quiet:
        print(f"PROMOTED {where} {row.get('name')}: {old} -> {a.to}")
    return 0


def cmd_demote(p: Project, a) -> int:
    addr = norm_addr(a.addr)
    mod = getattr(a, "module", None) or "golf_clean.exe"
    rows = p.rows()
    if (mod, addr) not in rows:
        print(f"{mod} {addr} not in hooks.csv")
        return 1
    row = rows[(mod, addr)]
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
    by_mod = Counter((r.get("module") or "golf_clean.exe", r.get("confidence") or "C0") for r in rows.values())
    for m in sorted({k[0] for k in by_mod}):
        print(f"  {m}: " + ", ".join(f"{lvl}={by_mod.get((m, lvl), 0)}" for lvl in LEVELS))
    if not getattr(a, "summary", False):
        for r in sorted(rows.values(), key=lambda r: (r.get("module") or "", r["addr"])):
            print(f"  {r.get('module', ''):14} {r['addr']}  {r.get('confidence'):3}  {r.get('subsystem', ''):9} {r.get('name', '')}")
    return 0


def cmd_batch(p: Project, a) -> int:
    """Promote every row of a TSV (header: module addr name subsystem evidence ...) by one level through the same
    gates as `promote`: hooks.csv is written once, one CHANGELOG line per promotion. DLL addresses may be VAs."""
    rows = p.rows()
    p.pending_log = []
    ok = refused = 0
    for path in a.tsv:
        lines = pathlib.Path(path).read_text().splitlines()
        head = lines[0].split("\t")
        for line in lines[1:]:
            c = dict(zip(head, line.split("\t")))
            if not c.get("addr"):
                continue
            mod = c.get("module") or "golf_clean.exe"
            va = int(c["addr"], 16)
            base = 0 if mod == "golf_clean.exe" else 0x10000000
            ns = argparse.Namespace(addr=f"{va - base if va >= base else va:08x}", to=a.to, module=mod,
                                    name=c.get("name"), subsystem=c.get("subsystem"), evidence=c.get("evidence"),
                                    note=c.get("note"), file=None, callers=c.get("callers"), callees=c.get("callees"))
            r = cmd_check(p, ns, write=True, rows=rows, quiet=True)
            ok += r == 0
            refused += r != 0
    if ok:
        p.save(rows)
        _flush_log(p)
    print(f"batch {a.to}: {ok} promoted, {refused} refused")
    return 0 if not refused else 2


def _flush_log(p: Project) -> None:
    text = p.changelog.read_text() if p.changelog.exists() else "# CHANGELOG (newest first)\n\n" + MARKER + "\n"
    if MARKER not in text:
        raise SystemExit(f"{p.changelog} lost its {MARKER} marker; refusing to write")
    p.changelog.write_text(text.replace(MARKER, MARKER + "\n" + "\n".join(reversed(p.pending_log)), 1))


def cmd_retag(p: Project, a) -> int:
    """Copy the subsystem, name, callers and callees columns of a TSV onto rows already in hooks.csv (the level does
    not change; callers/callees only when the TSV has those columns, as c2_note.py --refresh writes).
    One CHANGELOG line per changed row; rows not in hooks.csv are skipped (promote them with `batch`)."""
    rows = p.rows()
    p.pending_log = []
    changed = skipped = 0
    for path in a.tsv:
        lines = pathlib.Path(path).read_text().splitlines()
        head = lines[0].split("\t")
        for line in lines[1:]:
            c = dict(zip(head, line.split("\t")))
            if not c.get("addr") or not c.get("subsystem"):
                continue
            if c["subsystem"] not in SUBSYSTEMS:
                raise SystemExit(f"{path}: unknown subsystem {c['subsystem']!r} at {c['addr']}")
            mod = c.get("module") or "golf_clean.exe"
            va = int(c["addr"], 16)
            base = 0 if mod == "golf_clean.exe" else 0x10000000
            addr = f"{va - base if va >= base else va:08x}"
            row = rows.get((mod, addr))
            if row is None:
                skipped += 1
                continue
            where = addr if mod == "golf_clean.exe" else f"{mod}:{addr}"
            for col in ("subsystem", "name", "callers", "callees"):
                old, new = row.get(col) or "", c.get(col) or ""
                if not new or old == new:
                    continue
                row[col] = new
                p.pending_log.append(f"{datetime.date.today()}  {where}  {c.get('name')}  {col} {old}->{new}"
                                     f"  {pathlib.Path(path).as_posix()}")
                changed += 1
    if changed:
        p.save(rows)
        _flush_log(p)
    print(f"retag: {changed} changed, {skipped} not in hooks.csv")
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
    d.add_argument("--module")
    d.add_argument("--to", required=True, choices=LEVELS)
    d.add_argument("--reason", required=True)
    st = sub.add_parser("status")
    st.add_argument("--summary", action="store_true", help="counts per module only")
    b = sub.add_parser("batch", help="promote every row of one or more TSVs (module addr name subsystem evidence)")
    b.add_argument("tsv", nargs="+")
    b.add_argument("--to", required=True, choices=LEVELS[1:])
    rt = sub.add_parser("retag", help="update the subsystem and name of tracked rows from names TSVs")
    rt.add_argument("tsv", nargs="+")
    a = ap.parse_args(argv)
    p = Project(a.root)
    if a.cmd == "check":
        return cmd_check(p, a, write=False)
    if a.cmd == "promote":
        return cmd_check(p, a, write=True)
    if a.cmd == "demote":
        return cmd_demote(p, a)
    if a.cmd == "batch":
        return cmd_batch(p, a)
    if a.cmd == "retag":
        return cmd_retag(p, a)
    return cmd_status(p, a)


if __name__ == "__main__":
    sys.exit(main())
