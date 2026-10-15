# C3 batch c3fp (round 35): sound.dll renderChannelStreamed 0x10036660 (log/c3/c3fp_notes.txt). Module-aware hook; addr is the
# RVA, VA = 0x10000000 + RVA. Reimplementation in shim/src/re/c3fp.cpp, fixtures in re/frida/js/fixtures.d/c3fp.js, Purpose and
# branch table in log/c3/c3fp_purpose.md. Every key freezes the first thread, holds the multimedia timer off (0x100b49a8 = 5),
# swaps g_soundEngine 0x100b4a20 for a marker block and replaces the seven thunks the body calls with recording stubs (A/B
# thread only, into $log). One vector = one channel with its own object graph in the arena $v<i>; the x87 control word is set
# per key (_w27f: 0x027f, the A/B thread's own; _wf7f: 0x0f7f, what the Voice_mix* leaves leave on the timer thread).

_SND = dict(module="sound.dll")
_N = {"copy": 19, "chain": 13, "fade": 33}     # the table lengths in fixtures.d/c3fp.js (F.c3fp_counts)

HOOKS = {}
for _g, _w in (("copy", "w27f"), ("chain", "w27f"), ("fade", "w27f"), ("fade", "wf7f")):
    HOOKS[f"renderChannelStreamed_{_g}_{_w}"] = dict(
        _SND, addr=0x00036660, abi="thiscall", ret="uint32", args=["pointer", "uint32", "uint32", "pointer"],
        fixture=f"c3fp_{_g}_{_w}",
        state=[(f"$v{i}", 0, 0x7000) for i in range(_N[_g])] + [("$log", 0, 0x2000), ("$eng", 0, 0x40), ("$geng", 0, 4)],
        vectors=[(f"$c{i}", 0xA1A10000 + i, 0xA2A20000 + i, f"$o{i}") for i in range(_N[_g])])
