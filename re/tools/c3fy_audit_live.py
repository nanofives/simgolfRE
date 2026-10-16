r"""c3fy audit, batch c3fyc: follow-up of log/c3/c3fy2_live_cmp.py (fix 6's class) to the place where the stale value would
be consumed.

c3fy2_live_cmp.py lists the conditional jumps whose compared register was loaded from memory in the same block and is
still live at a successor, and FLAGs those whose load-citing source lines assign no C variable of the register's name
(207 of them). For every FLAG (register r, load L, jump J, successor S where r is live) this script walks forward from S
(fallthrough and unconditional `jmp` only, at most 60 instructions) to the first instruction that READS r before writing
it (the use U), takes the source lines citing U (or, when none, the block of U), and classifies:
  OK-REREAD  a use line reads the load's memory operand again (its displacement / global appears in the line)
  OK-VAR     a use line names a variable that a load-citing line assigns (`int flags = G32(0x0059E7B8)` ... `flags`)
  OK-REG     a use line names the register itself and a load-citing line assigns that register variable
  OK-DEAD    the forward walk meets a write of r before any read (liveness is path-merged; this path never reads it)
  REVIEW     none of the above: printed with the load line, the use instruction and the use's source lines
The REVIEW rows are the manual pass (log/c3/c3fyc_notes.txt section 2). Uses log/c3/alt/c3fy_live.py (liveness).

    py -3.12 re/tools/c3fy_audit_live.py [--out log/c3/c3fyc_audit_live.txt]
"""
from __future__ import annotations

import argparse
import collections
import re
import sys

import c3fy_audit_common as A

sys.path.insert(0, str(A.ROOT / "log" / "c3" / "alt"))
import c3fy_live as LV  # noqa: E402
from capstone import x86  # noqa: E402

SUBREG = {"eax": r"\b(eax|ax|al|ah)\b", "ebx": r"\b(ebx|bx|bl|bh)\b", "ecx": r"\b(ecx|cx|cl|ch)\b",
          "edx": r"\b(edx|dx|dl|dh)\b", "esi": r"\b(esi|si)\b", "edi": r"\b(edi|di)\b", "ebp": r"\b(ebp|bp)\b"}


def cites(src, va):
    return [ln for ln in src if va in ln.cites and ln.code.strip()]


def mem_key(d):
    """the load's memory operand as (displacement) for the reread test."""
    for op in d.operands:
        if op.type == x86.X86_OP_MEM:
            return op.mem.disp & 0xFFFFFFFF
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(A.ROOT / "log" / "c3" / "c3fyc_audit_live.txt"))
    a = ap.parse_args()
    insns, blocks, succ = LV.insns, LV.blocks, LV.succ
    live = LV.liveness()
    src = A.load_source()
    order = sorted(insns)
    nxt = {order[q]: order[q + 1] for q in range(len(order) - 1)}
    rows = []
    st = collections.Counter()
    for b, ins in blocks.items():
        j = ins[-1]
        ji = insns[j]
        if not ji.mnemonic.startswith("j") or ji.mnemonic == "jmp":
            continue
        k = len(ins) - 2
        while k >= 0 and insns[ins[k]].mnemonic not in ("cmp", "test"):
            if insns[ins[k]].eflags and not insns[ins[k]].mnemonic.startswith("j"):
                k = -1
                break
            k -= 1
        if k < 0:
            continue
        s = insns[ins[k]]
        regs = [LV.rn(op.reg) for op in s.operands if op.type == x86.X86_OP_REG]
        for r in [x for x in regs if x]:
            d = None
            for q in range(k - 1, -1, -1):
                u, defs = LV.use_def(insns[ins[q]])
                if r in defs:
                    d = insns[ins[q]]
                    break
            if d is None or not any(op.type == x86.X86_OP_MEM for op in d.operands) or \
                    d.mnemonic not in ("mov", "movsx", "movzx"):
                continue
            if any(r in LV.use_def(insns[ins[q]])[1] for q in range(k + 1, len(ins))):
                continue
            live_at = [t for t in succ[b] if r in live.get(t, set())]
            if not live_at:
                continue
            lcl = cites(src, d.address)
            if any(re.search(r"\b" + r + r"\s*=[^=]", ln.code) for ln in lcl):
                continue                                   # c3fy2_live_cmp's `ok` rows
            st["flag"] += 1
            assigned_vars = set()
            for ln in lcl:
                for m in re.finditer(r"\b([A-Za-z_]\w*)\s*=[^=]", ln.code):
                    assigned_vars.add(m.group(1))
            disp = mem_key(d)
            for t in live_at:
                # forward to the first read of r
                va, use, steps = t, None, 0
                dead = False
                over = set()                 # sub-registers of r overwritten on the way (their bytes no longer the load's)
                fam = SUBREG[r]
                while va is not None and steps < 60:
                    i = insns.get(va)
                    if i is None:
                        break
                    u, defs = LV.use_def(i)
                    ops = [x.strip() for x in i.op_str.split(",")]
                    dst = ops[0] if ops else ""
                    srcs = ",".join(ops[1:]) if len(ops) > 1 else ("" if i.mnemonic in ("pop", "set") else dst)
                    full_set = (i.mnemonic in ("or",) and len(ops) == 2 and dst == r and ops[1] in ("0xffffffff", "-1")) or \
                        (i.mnemonic in ("xor", "sub") and len(ops) == 2 and dst == r and ops[1] == r)
                    if full_set:
                        dead = True
                        break
                    if r in u:
                        rd = set(re.findall(fam, srcs)) if i.mnemonic not in ("mov", "movsx", "movzx", "lea") \
                            else set(re.findall(fam, srcs))
                        if i.mnemonic in ("cmp", "test", "add", "sub", "and", "or", "xor", "inc", "dec", "shl", "shr",
                                          "sar", "neg", "not", "imul", "adc", "sbb"):
                            rd |= set(re.findall(fam, dst))
                        # a `mov r8/r16, src` that only overwrites part of r
                        if i.mnemonic == "mov" and re.fullmatch(fam, dst) and dst != r and not rd:
                            over.add(dst)
                            va = nxt.get(va)
                            steps += 1
                            continue
                        if rd and rd <= over:
                            if r in defs and dst == r:          # e.g. `movsx eax, al` after `mov al, [..]`: a full write
                                dead = True
                                break
                            va = nxt.get(va)
                            steps += 1
                            continue
                        use = i
                        break
                    if r in defs:
                        dead = True
                        break
                    if i.mnemonic == "jmp" and i.op_str.startswith("0x"):
                        va = int(i.op_str, 16)
                    elif i.mnemonic in ("ret",) or (i.mnemonic == "jmp"):
                        break
                    else:
                        va = nxt.get(va)
                    steps += 1
                if dead:
                    st["ok_dead"] += 1
                    rows.append(("OK-DEAD", r, d, ji, t, None, []))
                    continue
                if use is None:
                    st["no_use"] += 1
                    rows.append(("REVIEW", r, d, ji, t, None, []))
                    continue
                ucl = cites(src, use.address)
                if not ucl:
                    # lines citing anything in the use's block window
                    ucl = [ln for ln in src if ln.code.strip() and any(use.address - 0x10 <= c <= use.address + 0x4
                                                                        for c in ln.cites)]
                text = " ".join(ln.code for ln in ucl)
                tag = "REVIEW"
                if disp and disp >= 0x400000 and re.search(f"0x0*{disp:x}\\b", text, re.I):
                    tag = "OK-REREAD"
                elif assigned_vars and any(re.search(r"\b" + re.escape(v) + r"\b", text) for v in assigned_vars):
                    tag = "OK-VAR"
                st[tag] += 1
                rows.append((tag, r, d, ji, t, use, ucl))
    out = [f"c3fy live-compare follow-up: {dict(st)}"]
    for tag, r, d, ji, t, use, ucl in rows:
        if tag.startswith("OK"):
            continue
        out.append(f"{tag} {r}: load {d.address:08x} {d.mnemonic} {d.op_str} | jcc {ji.address:08x} {ji.mnemonic} | "
                   f"successor {t:08x} | use {use.address:08x} {use.mnemonic} {use.op_str}" if use else
                   f"{tag} {r}: load {d.address:08x} {d.mnemonic} {d.op_str} | jcc {ji.address:08x} | successor {t:08x} | no use found")
        for ln in ucl[:3]:
            out.append(f"      c3fy.cpp:{ln.n}: {ln.code.strip()[:150]}")
    A.pathlib.Path(a.out).write_text("\n".join(out) + "\n")
    print(out[0])


if __name__ == "__main__":
    sys.exit(main())
