"""D43: placeAnimal 0x004017d0, idiv by the sprite frame count at 0x0040189a ([esi*4 + 0x53f3e8], esi = animal kind word + 0x140 offset word)."""
import plib as P
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_ESI

ANIM, AS = 0x56d1b8, 0x3c
CNT = 0x53f3e8


def force_visible(uc):
    """0x0042fb90 (the on-screen test, cdecl): return 1 so the draw arm is entered"""
    def on(u, addr, size, _):
        esp = u.reg_read(UC_X86_REG_ESP)
        ret = int.from_bytes(u.mem_read(esp, 4), "little")
        u.reg_write(UC_X86_REG_EAX, 1)
        u.reg_write(UC_X86_REG_ESP, esp + 4)
        u.reg_write(UC_X86_REG_EIP, ret)
    uc.hook_add(UC_HOOK_CODE, on, None, 0x42fb90, 0x42fb90)


def build(scen, count):
    im = P.Img(P.start_image(scen, "00"))
    # animal record 0: kind word at +0x20 (0x56d1d8), frame offset word +0x2c (0x56d1e4); frame word +0x2a (0x56d1e2) < 0x80
    im.w(0x56d1d8, 3, 2)
    im.w(0x56d1e4, 0x140, 2)
    im.w(0x56d1e2, 0, 2)
    im.w(CNT + (3 + 0x140) * 4, count, 4)
    return im


for scen in ("sandbox_basic", "course_season"):
    for count in (0, 6):
        seen = []
        r = P.emu(0x4017d0, [0], build(scen, count).done(), max_insns=2_000_000,
                  hooks=[force_visible, P.visit([0x401832, 0x40189a, 0x4018a1], seen)])
        P.show(r, f"{scen} frame count {count}", 2)
        print("   visited:", [hex(a) for a in seen], " fault eip:", hex(r.eip))
