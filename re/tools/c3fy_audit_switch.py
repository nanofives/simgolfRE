r"""c3fy audit (d), batch c3fyc: the 27 switch tables of mainLoop 0x0040f5c0 against the switches of
shim/src/re/c3fy.cpp.

Original side, from the binary: for each `jmp [reg*4 + table]` (log/c3/alt/c3fy_cfg.json: site, table, entry count) the
bound (`cmp reg, N` + `ja default` before the jump, or the byte index table `mov r8, byte ptr [reg + idx]` with its own
`cmp` / `ja`), the default target, and the target of every index value (through the byte table when there is one).

Source side: the `switch (...)` whose line or the 3 lines below it cite the jump site or the table. Inside its braces each
group of `case k:` labels (and `default:`) is mapped to the address the group starts at: a `goto <label>` whose label
name carries the address (`La_4135b1`, `a41de11`, `L41a8ee`), else the first address cited from the label line on.

Check: the case value -> index offset (the source may switch on the value before the original's `sub` / `dec`) is the one
that pairs the most groups; then every index's original target is compared with the group of the matching case value
(or default). An index whose target is the default address needs no case. Reports per table: indices OK, MISMATCH
(index, original target, source group address and line), MISSING (index with a non-default target and no case),
EXTRA (a case value outside the table).

    py -3.12 re/tools/c3fy_audit_switch.py [--out log/c3/c3fyc_audit_switch.txt]
"""
from __future__ import annotations

import argparse
import re
import sys

import c3fy_audit_common as A


def original_tables(L: A.Listing):
    out = []
    for tva, t in sorted(A.tables().items()):
        site = t["site"]
        k = L.by_va[site].k
        idx_table, bound, default = None, None, None
        for j in range(k - 1, max(k - 14, 0), -1):
            i = L.ins[j]
            m = re.fullmatch(r"(?:[a-z]{2}) , ?byte ptr \[([a-z]{3}) \+ 0x([0-9a-f]+)\]", i.ops.replace(", ", " , ", 1)) \
                if i.mn in ("mov", "movzx") else None
            mm = re.search(r"byte ptr \[[a-z]{3} \+ 0x([0-9a-f]+)\]", i.ops) if i.mn in ("mov", "movzx", "xor") else None
            if mm and idx_table is None and i.mn in ("mov", "movzx") and 0x421600 <= int(mm.group(1), 16) < 0x421b30:
                idx_table = int(mm.group(1), 16)
            if i.mn == "ja" and default is None:
                default = int(i.ops, 16)
                c = L.ins[j - 1]
                if c.mn == "cmp":
                    bound = int(c.ops.split(",")[1], 0)
        nvals = (bound + 1) if bound is not None else t["n"]
        targets = {}
        for v in range(nvals):
            ix = A.read(idx_table + v, 1)[0] if idx_table else v
            targets[v] = A.u32(tva + 4 * ix)
        entries = {e: A.u32(tva + 4 * e) for e in range(t["n"])}
        out.append({"table": tva, "site": site, "idx": idx_table, "bound": bound, "default": default,
                    "targets": targets, "entries": entries})
    return out


LABEL_ADDR = re.compile(r"\bgoto\s+(?:La_|L|a)([0-9a-fA-F]{6})\b")


def source_switches(src: list[A.SrcLine], tables):
    """table va -> (switch line, {case value: (address, line)}, default (address, line) | None)"""
    out = {}
    for t in tables:
        hit = None
        for k, ln in enumerate(src):
            if "switch" not in ln.code:
                continue
            near = []
            for q in range(max(0, k - 3), min(len(src), k + 4)):
                near += src[q].own
                near += [int(x, 16) for x in re.findall(r"0x00([0-9a-fA-F]{6})", src[q].comment)]
            text = " ".join(src[q].text for q in range(max(0, k - 3), min(len(src), k + 4)))
            if t["site"] in near or f"{t['table']:x}" in text.lower():
                hit = k
                break
        if hit is None:
            continue
        # walk the braces of this switch
        k = hit
        depth, started = 0, False
        groups, cur_vals, cur_default, pending = {}, [], False, None
        default = None
        q = k
        while q < len(src):
            code = src[q].code
            for ch in code:
                if ch == "{":
                    depth += 1
                    started = True
                elif ch == "}":
                    depth -= 1
            # several `case k: goto La_xxxxxx;` on one line (the machine-transcribed P3 writes whole switches that way)
            inline = re.findall(r"\bcase\s+(-?(?:0x[0-9a-fA-F]+|\d+))\s*:\s*goto\s+(?:La_|L|a)([0-9a-fA-F]{6})\b", code)
            if len(inline) > 1:
                for v, lab in inline:
                    groups.setdefault(int(v, 0), (int(lab, 16), src[q].n))
                code = re.sub(r"\bcase\s+[^:]+:\s*goto\s+\w+\s*;", "", code)
            labels = re.findall(r"\bcase\s+(-?(?:0x[0-9a-fA-F]+|\d+))\s*:", code)
            is_def = re.search(r"\bdefault\s*:", code) is not None
            if labels or is_def:
                if pending is None:
                    pending = {"vals": [], "default": False, "line": src[q].n}
                pending["vals"] += [int(x, 0) for x in labels]
                pending["default"] |= is_def
            if pending is not None:
                # the group's address: a goto label on this line, else the first own citation from the label line on
                after = code
                if labels or is_def:
                    after = re.split(r"\bcase\s+[^:]+:|\bdefault\s*:", code)[-1]
                g = LABEL_ADDR.search(after)
                addr = None
                if g:
                    addr = int(g.group(1), 16)
                elif src[q].own:
                    addr = src[q].own[0]
                if addr is not None:
                    for v in pending["vals"]:
                        groups.setdefault(v, (addr, pending["line"]))
                    if pending["default"]:
                        default = (addr, pending["line"])
                    pending = None
            if started and depth <= 0:
                break
            q += 1
        out[t["table"]] = (src[hit].n, groups, default)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(A.ROOT / "log" / "c3" / "c3fyc_audit_switch.txt"))
    a = ap.parse_args()
    L = A.Listing()
    src = A.load_source()
    tabs = original_tables(L)
    sw = source_switches(src, tabs)
    lines = []
    tot = {"ok": 0, "bad": 0, "missing": 0, "tables": 0, "unpaired": 0}
    for t in tabs:
        head = (f"table {t['table']:#010x} site {t['site']:#010x} bound {t['bound']} default "
                f"{t['default'] and hex(t['default'])} index-bytes {t['idx'] and hex(t['idx'])}")
        if t["table"] not in sw:
            lines.append(head + "  -> NO SOURCE SWITCH FOUND (check by hand)")
            tot["unpaired"] += 1
            continue
        tot["tables"] += 1
        sl, groups, dflt = sw[t["table"]]
        best = None
        modes = [("value", t["targets"])] + ([("dword index", t["entries"])] if t["idx"] else [])
        for mname, tg_map in modes:
            for delta in range(-64, 65):
                score = sum(1 for v, (ad, _l) in groups.items() if tg_map.get(v - delta) is not None and
                            abs(tg_map[v - delta] - ad) <= 0x10)
                if best is None or score > best[0]:
                    best = (score, delta, mname, tg_map)
        delta, mname = best[1], best[2]
        t = dict(t, targets=best[3])
        rep = []
        ok = 0
        for ix, tg in t["targets"].items():
            v = ix + delta
            if v in groups:
                ad, gl = groups[v]
                if abs(tg - ad) <= 0x10:
                    ok += 1
                else:
                    rep.append(f"   MISMATCH index {ix} (case {v}): original {tg:#010x}, source {ad:#010x} (line {gl})")
                    tot["bad"] += 1
            elif tg == t["default"]:
                ok += 1
            elif dflt is not None and abs(dflt[0] - tg) <= 0x10:
                ok += 1
            else:
                rep.append(f"   MISSING index {ix} (case {v}): original {tg:#010x}, no case in the source switch")
                tot["missing"] += 1
        for v in groups:
            if v - delta not in t["targets"]:
                rep.append(f"   EXTRA case {v} (index {v - delta}) outside the table")
        tot["ok"] += ok
        lines.append(head + f"  -> source switch line {sl}, cases = {mname} {delta:+d}, {ok}/{len(t['targets'])} OK")
        lines += rep
    lines.insert(0, f"c3fy audit (d) switch tables: {tot}")
    A.pathlib.Path(a.out).write_text("\n".join(lines) + "\n")
    print("\n".join(lines))


if __name__ == "__main__":
    sys.exit(main())
