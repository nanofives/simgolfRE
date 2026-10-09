"""Entry points of code Ghidra never made into functions, found from the calls they make.

A C2 row can have `callers none` and no stored pointer even though it is called: the call sits in code that is not
inside any function of the Ghidra listing, so xref.py never sees it (2026-10-09 survey: exe 247 of 253 such rows,
sound 109 of 113, jgld 54 of 127). Example: 0x403f10 `call 0x403f20; jmp 0x403f40` is a C++ dynamic-initializer stub
whose only reference is its slot 0x4c1004 in the CRT initializer table; Ghidra never created it.

For every `call rel32` / `jmp rel32` in .text whose target is a function of the listing (or one of its incremental-link
thunks) and whose site lies outside every function and outside the DLL thunk table, the entry of the missing function
is taken as the nearest address at or before the site, within --window bytes and without crossing a known function,
that something else references: a dword in the image equal to it, or a `call rel32` from anywhere landing on it.
An entry found this way is reported with its evidence; nothing is created here. Feed the entries to
`ghidra/scripts/CreateFunctions.java`, re-export re/functions_ghidra*.tsv, delete log/xref_<module>.json and run again:
calls inside the new functions become visible and further entries can appear.

    py -3.12 re/tools/undiscovered.py <module> [--window 0x400] [--all-targets] [--out <tsv>]
        default targets: the C2 rows of hooks.csv with `callers none`; --all-targets: every function of the listing
"""
import argparse, collections, csv, pathlib, struct, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import pointer_callers as pc  # noqa: E402


def rel32_sites(m):
    """{target_va: [(site_va, opcode)]} for every E8/E9 rel32 in .text, plus the set of call targets."""
    out = collections.defaultdict(list)
    for lo, hi in m.text:
        for o in range(lo - m.base, hi - m.base - 5):
            op = m.img[o]
            if op in (0xE8, 0xE9):
                t = m.base + o + 5 + struct.unpack("<i", m.img[o + 1:o + 5])[0]
                if any(a <= t < b for a, b in m.text):
                    out[t].append((m.base + o, op))
    return out


def thunk_table_end(m):
    if m.name == "golf_clean.exe":
        return m.text[0][0]
    o = m.text[0][0] - m.base
    while o < len(m.img) and m.img[o] in (0xE9, 0xCC):
        o += 5 if m.img[o] == 0xE9 else 1
    return m.base + o


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("module")
    ap.add_argument("--window", default="0x400")
    ap.add_argument("--all-targets", action="store_true")
    ap.add_argument("--out")
    a = ap.parse_args()
    m = pc.mod(a.module)
    window = int(a.window, 16)
    sites = rel32_sites(m)
    called = {t for t, ss in sites.items() if any(op == 0xE8 for _, op in ss)}
    tt_end = thunk_table_end(m)
    if a.all_targets:
        bodies = list(m.entries)
    else:
        bodies = [pc.norm(m, r["addr"]) for r in csv.DictReader(open(ROOT / "hooks.csv"))
                  if (r["module"] or "golf_clean.exe") == a.module and r["confidence"] == "C2"
                  and r["callers"] in ("", "none")]
    def switch_case(e):
        """True when every reference to e is a slot of a table that an indirect `jmp [reg*4 + table]` uses: a case
        body of a switch, not a function (N1 found 25 such false entries in the first pass, 2026-10-09)."""
        refs = [x for v in [e] + m.thunks.get(e, []) for x in m.find_all(v)]
        if not refs or e in called:
            return False
        for x in refs:
            start = x
            while m.dword(start - 4) is not None and m.in_code(m.dword(start - 4)):
                start -= 4
            users = []
            for t in range(start, x + 4, 4):
                for u in m.find_all(t):
                    if m.img[u - m.base - 3:u - m.base - 1] == bytes((0xFF, 0x24)):   # jmp dword ptr [reg*4 + disp32]
                        users.append(u)
            if not users:
                return False
        return True
    entries = {}
    unresolved = []
    for body in bodies:
        for t in [body] + m.thunks.get(body, []):
            for site, op in sites.get(t, []):
                if m.function_at(site) is not None or site < tt_end:
                    continue
                found = None
                for e in range(site, max(site - window, tt_end) - 1, -1):
                    if e != site and m.function_at(e) is not None:
                        break
                    # debug DLLs reach a function through its incremental-link thunk: a referenced or called
                    # thunk that jumps to e makes e an entry too
                    if e in called or m.find_all(e) or any(th in called or m.find_all(th) for th in m.thunks.get(e, [])):
                        found = e
                        break
                if found is None:
                    unresolved.append((body, site))
                    continue
                if switch_case(found):
                    unresolved.append((body, site))
                    continue
                ev = entries.setdefault(found, {"targets": set(), "refs": [], "calls": []})
                ev["targets"].add(body)
                via = [found] + m.thunks.get(found, [])
                ev["refs"] = [hex(x) for v in via for x in m.find_all(v)][:4]
                ev["calls"] = [hex(s) for v in via for s, op in sites.get(v, []) if op == 0xE8][:4]
    rows = sorted(entries.items())
    out = open(a.out, "w", newline="") if a.out else sys.stdout
    out.write("module\tentry\tcalls_targets\tdword_refs\tcall_sites\n")
    for e, ev in rows:
        out.write(f"{a.module}\t0x{e:08x}\t" + ";".join(f"0x{t:08x}" for t in sorted(ev['targets']))
                  + f"\t{';'.join(ev['refs'])}\t{';'.join(ev['calls'])}\n")
    if a.out:
        out.close()
    print(f"{a.module}: {len(rows)} entries for {len({t for _, ev in rows for t in ev['targets']})} targets; "
          f"{len(unresolved)} call sites with no referenced entry within the window", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
