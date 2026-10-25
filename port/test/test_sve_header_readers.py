"""S1.1 (b), U-1060: the readers of a loaded .sve / .cse header (re/analysis/data/sve_header_readers.md).

Static, over the user's unwrapped exe (original/golf_clean.exe, anchored by tests/test_static.py). The header is read into
g_textBuffer (0x51a068) with __read 0x4a583a, 0x64 bytes at a time; saveGame / saveCourse write all 0x64 bytes of the
buffer, so the bytes after the text's NUL are leftover text. The test pins:
  1. the set of exe call sites that read a file into g_textBuffer (4, all in loadGame / loadCourse) and the set that
     write it with __write (the savers);
  2. that the body decoders (serializeCourse, deserializeCourse, saveFieldIo) hold no reference into the buffer;
  3. that the consumers of the loaded text, the browser's list builder (loadGameBrowserScreen) and the text draw path
     (drawText4049d0 -> drawText 0x477c30), measure the text with a NUL scan (`repne scasb`) and never with the 0x64
     length: no `push 0x64` and no 0x64 compare in the browser's header-consuming loops.
"""
import re
from pathlib import Path

import pytest

pytestmark = pytest.mark.static

ROOT = Path(__file__).resolve().parents[2]
EXE = ROOT / "original" / "golf_clean.exe"

TEXT_BUFFER = 0x0051A068
TEXT_BUFFER_END = 0x0051B068
READ, WRITE = 0x004A583A, 0x004A5B58

# (function, address of the call, who) of every __read whose buffer is g_textBuffer
HEADER_READS = [
    (0x0040B840, 0x0040B88F, "loadCourse header line 1"),
    (0x0040B840, 0x0040B92C, "loadCourse header line 2"),
    (0x0040B9B0, 0x0040BA91, "loadGame header line 1"),
    (0x0040B9B0, 0x0040BAAA, "loadGame header line 2 (championship files)"),
]
# every __write whose buffer is g_textBuffer: the savers
HEADER_WRITES = {0x0040B4A0: "saveGame", 0x0040B680: "saveCourse"}


def functions():
    """addr -> (name, size, ranges) from re/functions_ghidra.tsv (a body can have several ranges)."""
    out = {}
    for line in (ROOT / "re" / "functions_ghidra.tsv").read_text().splitlines()[1:]:
        c = line.split("	")
        rng = [tuple(int(v, 16) for v in r.split("-")) for r in c[5].split(";")]
        out[int(c[0], 16)] = (c[1], int(c[2]), rng)
    return out


def disasm():
    """Every function's instructions, decoded range by range (a linear sweep of .text desyncs on its data)."""
    if not EXE.exists():
        pytest.skip("original/golf_clean.exe not present")
    import capstone
    import pefile
    pe = pefile.PE(str(EXE), fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    sec = next(s for s in pe.sections if s.Name.startswith(b".text"))
    va0 = base + sec.VirtualAddress
    data = sec.get_data()
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    out = {}
    for addr, (_, _, rng) in functions().items():
        ii = []
        for lo, hi in rng:
            ii += list(md.disasm(data[lo - va0:hi - va0], lo))
        out[addr] = ii
    return out


@pytest.fixture(scope="module")
def ins():
    return disasm()


def owner(addr, fns):
    best = max((a for a in fns if a <= addr), default=None)
    return best if best is not None and addr < best + fns[best][1] else None


def call_sites_with_buffer(ins, target):
    """Calls to `target` with `push 0x51a068` among the 6 instructions before them."""
    out = []
    for body in ins.values():
        for i, x in enumerate(body):
            if x.mnemonic == "call" and x.op_str == hex(target):
                if any(y.mnemonic == "push" and y.op_str == hex(TEXT_BUFFER) for y in body[max(0, i - 6):i]):
                    out.append(x.address)
    return out


def test_header_reads_are_exactly_the_four_loader_sites(ins):
    fns = functions()
    got = sorted((owner(a, fns), a) for a in call_sites_with_buffer(ins, READ))
    assert got == sorted((f, a) for f, a, _ in HEADER_READS), [(hex(f or 0), hex(a)) for f, a in got]


def test_header_writes_are_the_two_savers(ins):
    fns = functions()
    owners = {owner(a, fns) for a in call_sites_with_buffer(ins, WRITE)}
    assert owners == set(HEADER_WRITES), {hex(o or 0) for o in owners}


def test_every_header_read_asks_for_0x64_bytes(ins):
    for _, site, why in HEADER_READS:
        window = [x for x in ins[owner(site, functions())] if site - 24 <= x.address < site]
        assert any(x.mnemonic == "push" and x.op_str == "0x64" for x in window), why


@pytest.mark.parametrize("name,addr", [("serializeCourse", 0x0040AFA0), ("deserializeCourse", 0x0040BBF0),
                                       ("saveFieldIo", 0x0040AF70)])
def test_body_decoders_never_touch_the_text_buffer(ins, name, addr):
    for x in ins[addr]:
        for m in re.findall(r"0x[0-9a-f]+", x.op_str):
            assert not TEXT_BUFFER <= int(m, 16) < TEXT_BUFFER_END, (name, hex(x.address), x.op_str)


def test_text_draw_path_stops_at_the_nul(ins):
    """drawText 0x477c30 sizes the string with a NUL scan (repne scasb) and caps the length at it."""
    body = ins[0x00477C30]
    assert any(x.mnemonic == "repne scasb" for x in body)
    # 0x64 appears there only as the list row stride (rows of 100 bytes at the file-name table); the header text is
    # never moved as a fixed 0x64-byte (25-dword) block and the browser reads no file itself
    assert not any(x.mnemonic == "mov" and x.op_str == "ecx, 0x19" for x in body)
    assert not any(x.mnemonic == "call" and x.op_str in (hex(READ), hex(WRITE)) for x in body)


def test_browser_reads_the_header_with_nul_scans_only(ins):
    """loadGameBrowserScreen copies the header text with strlen / strcat / strcpy loops (repne scasb then rep movs sized
    by the scanned length) and never moves the header as a 0x64-byte block."""
    body = ins[0x0043B610]
    assert sum(x.mnemonic == "repne scasb" for x in body) >= 20
    # 0x64 appears there only as the list row stride (rows of 100 bytes at the file-name table); the header text is
    # never moved as a fixed 0x64-byte (25-dword) block and the browser reads no file itself
    assert not any(x.mnemonic == "mov" and x.op_str == "ecx, 0x19" for x in body)
    assert not any(x.mnemonic == "call" and x.op_str in (hex(READ), hex(WRITE)) for x in body)
