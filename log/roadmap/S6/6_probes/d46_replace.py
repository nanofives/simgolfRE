"""D46: replaceInText 0x0045b7c0 saves the text from the match on (strstr result, 0x0045b7d4) into a 0x200-byte stack buffer at esp + 0x10 (0x0045b7f6 / 0x0045b803).
Tail lengths around 0x200: how the return address ends up."""
import plib as P

TEXT = 0x51a068
NEEDLE, REPL = 0x10000000, 0x10001000


def run(taillen, label):
    im = P.Img(P.start_image("sandbox_basic", "00"))
    text = b"aaaa" + b"%N" + b"b" * (taillen - 2) + b"\0"      # tail = from "%N" on, strlen = taillen
    im.wb(TEXT, text + b"\0" * 8)
    blocks = [(NEEDLE, b"%N\0"), (REPL, b"X\0")]
    r = P.emu(0x45b7c0, [NEEDLE, REPL], im.done(), blocks=blocks, max_insns=2_000_000)
    P.show(r, f"{label}: tail strlen {taillen:#x} (strlen + 1 = {taillen + 1:#x} bytes copied)", 0)
    return r


print("text buffer symbol at 0x51a068:")
P.pr_syms(0x51a068, 0x51a070)
for tl in (0x100, 0x1ff, 0x200, 0x204, 0x210):
    run(tl, "replaceInText")
