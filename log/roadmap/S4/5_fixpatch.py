import pathlib
p = pathlib.Path(__file__).resolve().parent / "s5_patches.py"
t = p.read_text()
NL = chr(10)
BS = chr(92)
nl_esc = BS + "n"
bad1 = '"    u32 timer = (c3dh_gSoundTimer(ctx));  // 0x1d4fd' + NL + '", "    u32 timer = gref(ctx, 0xb49ec);  // 0x1d4fd (g_soundTimer)' + NL + '"'
good1 = '"    u32 timer = (c3dh_gSoundTimer(ctx));  // 0x1d4fd' + nl_esc + '", "    u32 timer = gref(ctx, 0xb49ec);  // 0x1d4fd (g_soundTimer)' + nl_esc + '"'
assert bad1 in t
t = t.replace(bad1, good1)
bad2 = '"        unsigned period = *timer;  // 0x1d506' + NL + '", "        unsigned period = ld<uint32_t>(ctx, timer);  // 0x1d506' + NL + '"'
good2 = '"        unsigned period = *timer;  // 0x1d506' + nl_esc + '", "        unsigned period = ld<uint32_t>(ctx, timer);  // 0x1d506' + nl_esc + '"'
assert bad2 in t
t = t.replace(bad2, good2)
p.write_text(t)
print("ok")
