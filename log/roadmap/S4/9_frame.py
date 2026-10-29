"""S4.9 helper: disassemble a sound.dll function (the user's file) with the esp delta tracked linearly through push / pop /
sub / add / call (callee-cleaned `ret N` is not known, so stdcall calls are marked with their pushed bytes only when the
instruction after the call is not an `add esp`), printing each esp-relative operand as an entry-relative offset:
`E-0x48` is the local at (entry esp - 0x48), `E+4` the first stack argument. Branches are not followed: past a join the delta
may be off, read the raw listing then (8_dis.py).

    ~/bin/py -3.12 log/roadmap/S4/9_frame.py 0x6420 [n]          # n instructions (default 80)
"""
import importlib.util
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location("dis8", ROOT / "log" / "roadmap" / "S4" / "8_dis.py")
D = importlib.util.module_from_spec(spec)
spec.loader.exec_module(D)
from capstone import CS_ARCH_X86, CS_MODE_32, Cs  # noqa: E402

md = Cs(CS_ARCH_X86, CS_MODE_32)
md.detail = False
rva = int(sys.argv[1], 16)
n = int(sys.argv[2]) if len(sys.argv) > 2 else 80
code = D.DATA[rva:rva + n * 15]
delta = 0     # esp = E + delta (E = the address of the return address)
at_branch = 0
pending = 0   # bytes pushed since the last call (a callee-cleaned call gives them back)


def follow(r):
    for _ in range(8):
        if D.DATA[r] == 0xE9:
            import struct
            r = (r + 5 + struct.unpack_from("<i", D.DATA, r + 1)[0]) & 0xFFFFFFFF
        else:
            break
    return r


def ret_n(r):
    """the N of the first `ret N` (or 0 for `ret`) after r (a heuristic: the callee's first return)"""
    r = follow(r)
    for i, ins in enumerate(md.disasm(D.DATA[r:r + 600], D.IBASE + r)):
        if ins.mnemonic == "ret":
            return int(ins.op_str, 0) if ins.op_str else 0
    return 0
# stdcall argument counts of the imports the module calls (the callee pops them)
STD = {"mmioOpenA": 3, "mmioClose": 2, "mmioGetInfo": 3, "mmioSetInfo": 3, "mmioRead": 3, "mmioWrite": 3, "mmioSeek": 3,
       "mmioAdvance": 3, "mmioSetBuffer": 4, "mmioDescend": 4, "mmioAscend": 3, "mmioStringToFOURCCA": 2, "timeGetTime": 0,
       "waveInOpen": 5, "waveInClose": 1, "waveInStart": 1, "waveInStop": 1, "waveInReset": 1, "waveInPrepareHeader": 3,
       "waveInUnprepareHeader": 3, "waveInAddBuffer": 3, "waveInGetDevCapsA": 3, "waveInGetNumDevs": 0,
       "EnterCriticalSection": 1, "LeaveCriticalSection": 1, "Sleep": 1, "PostMessageA": 4}
SP = re.compile(r"\[esp(?: ([+-]) (0x[0-9a-f]+|\d+))?\]")
for k, i in enumerate(md.disasm(code, D.IBASE + rva)):
    if k >= n:
        break
    op, mn = i.op_str, i.mnemonic
    note = ""
    m = SP.search(op)
    if m:
        disp = 0 if not m.group(1) else (1 if m.group(1) == "+" else -1) * int(m.group(2), 0)
        e = delta + disp
        note = f"   ; E{'+' if e >= 0 else '-'}{abs(e):#x}"
    extra = ""
    if mn in ("call", "jmp") and op.startswith("0x"):
        t = int(op, 16) - D.IBASE
        extra = f"   ; 0x{t:x} {D.who(t)}"
    elif "dword ptr [0x" in op:
        a = int(op.split("[0x")[1].split("]")[0], 16) - D.IBASE
        extra = f"   ; [0x{a:x}] {D.IMPS.get(a, '')}"
    print(f"{i.address - D.IBASE:06x}  [{delta:+#x}]  {mn} {op}{note}{extra}")
    if mn == "call":
        n_pop = 0
        if op.startswith("0x"):
            n_pop = ret_n(int(op, 16) - D.IBASE)
        elif "dword ptr [0x" in op:
            nm = D.IMPS.get(int(op.split("[0x")[1].split("]")[0], 16) - D.IBASE, "")
            short = nm.split("!")[-1]
            n_pop = 4 * STD.get(short, 0)
        delta += n_pop
        pending = 0
    elif mn == "push":
        pending += 4
        delta -= 4
    elif mn == "pop":
        pending = max(0, pending - 4)
        delta += 4
    elif mn == "sub" and op.startswith("esp, "):
        delta -= int(op.split(", ")[1], 0)
    elif mn == "add" and op.startswith("esp, "):
        delta += int(op.split(", ")[1], 0)
        pending = 0
    elif mn == "ret":
        print("   ---- ret")
        delta = at_branch      # the code after a return is a jump target of an earlier conditional branch
        pending = 0
    elif mn.startswith("j") and mn != "jmp":
        at_branch = delta
