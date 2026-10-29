"""S10.9 body-shape classifier over 9_facts_<key>.json (our capstone listing only).
kind(body) -> (kind, detail): EMPTY, CONST, GETTER, SETTER, FORWARD, CTOR, SDD, OTHER."""
import re

MEMW = re.compile(r"^(mov|add|sub|or|and|xor|inc|dec|neg|not|shl|shr|sar|fstp|fst|fistp|stosd|movsd|rep)\b")
THIS = re.compile(r"\[(ecx|esi|eax|edx)( \+ (0x[0-9a-f]+|\d+))?\]")


def split(line):
    a, rest = line.split(" ", 1)
    mn, _, ops = rest.partition(" ")
    return int(a, 16), mn, ops


def kind(body):
    ins = [split(b) for b in body]
    mns = [m for _, m, _ in ins]
    if not ins:
        return "OTHER", ""
    if mns == ["ret"] or (len(mns) == 1 and mns[0] == "ret"):
        return "EMPTY", f"`ret` at 0x{ins[0][0]:08x}"
    if all(m in ("xor", "mov", "or", "ret") for m in mns) and len(ins) <= 3 and mns[-1] == "ret" \
            and all(o.startswith("eax") for _, m, o in ins[:-1]) and not any("[" in o for _, _, o in ins):
        return "CONST", "; ".join(f"`{m} {o}`".replace(" `", "`") for _, m, o in ins) + f" at 0x{ins[0][0]:08x}"
    calls = [o for _, m, o in ins if m == "call" or (m == "jmp" and "ptr [" in o)]
    stores = [(a, m, o) for a, m, o in ins if MEMW.match(m) and "ptr [" in o.split(",")[0]]
    vtst = [(a, o) for a, m, o in stores if m == "mov" and re.match(r"dword ptr \[(ecx|esi|eax|edi)\], 0x1", o)]
    if vtst:
        return "CTOR", f"stores {vtst[0][1].split(', ')[1]} into [{vtst[0][1].split('[')[1].split(']')[0]}] at 0x{vtst[0][0]:08x}"
    if any("byte ptr [esp + 8], 1" in o or "byte ptr [esp + 0xc], 1" in o for _, m, o in ins if m == "test") and calls:
        return "SDD", "tests bit 0 of its argument (" + ", ".join(f"0x{a:08x}" for a, m, o in ins if m == "test")[:11] + ") around a call to the destructor"
    if not calls and not stores and mns[-1] == "ret":
        reads = sorted({o.split(", ", 1)[1] for _, m, o in ins if m in ("mov", "movsx", "movzx", "fld", "lea") and "," in o and "[" in o})
        if reads:
            return "GETTER", f"reads {', '.join(reads)} (from 0x{ins[0][0]:08x}), no store, no call"
    if stores and not calls and all(re.search(r"\[(ecx|esi)", o) for _, _, o in stores):
        args = any("[esp + " in o for _, _, o in ins)
        if args:
            return "SETTER", f"stores {', '.join(o.split(', ')[0] for _, _, o in stores)} from its arguments (0x{stores[0][0]:08x})"
    if calls and re.match(r"dword ptr \[e[a-d]x \+ 0x[0-9a-f]+\]", calls[-1]) and any(m == "mov" and o.startswith("ecx, dword ptr [ecx") for _, m, o in ins):
        src = next(o.split(", ")[1] for _, m, o in ins if m == "mov" and o.startswith("ecx, dword ptr [ecx"))
        return "FORWARD", f"calls slot {calls[-1].split('+ ')[1].rstrip(']')} of the object at {src}"
    return "OTHER", ""
