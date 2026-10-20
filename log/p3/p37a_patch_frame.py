"""Phase 3.7a: the hand edits of the converted mainLoop frame (log/p3/p37a_conv.py output) -> port/src/sim/tick_frame.inc.

Each edit replaces one exact piece of the converted text (asserted to occur exactly `count` times) and says why. The
edits are the original's imports (clock, GetAsyncKeyState), the x87 / register-saving asm helpers, the display vtable call,
host-pointer string compares, and the five byte index tables of switch statements inside mainLoop's .text (written as
switch functions in tick.cpp: control-flow structure, like a decompiler's case lists).
usage: py -3.12 log/p3/p37a_patch_frame.py <frame_raw.inc> <out.inc>
"""
import re
import sys

EDITS = [
    # frame head: the clock value comes from the trace (FrameInput.t_head)
    ("    FU(0x1EC) = SV_timeGetTime();", "    FU(0x1EC) = clock_head();", 1),
    # switch byte tables in .text -> switch functions (tick.cpp)
    ("switch (GU8(CODE_421740 + (u32)k))", "switch (sw_p4_kind(k))", 1),
    ("ecx = GU8(CODE_42191c + eax);", "ecx = sw_h1_key(eax);", 1),
    ("ecx = GU8(eax + CODE_4219f4);", "ecx = sw_h1_41de23(eax);", 1),
    ("switch ((u32)edi > 0x13u ? 6 : (int)GU8(CODE_421ac0 + (u32)edi))", "switch ((u32)edi > 0x13u ? 6 : sw_h2_tool(edi))", 1),
    ("int sel = GU8(CODE_421af0 + (u32)kind);", "int sel = sw_h2_kind(kind);", 1),
    # GetAsyncKeyState through the import slot -> the run's key state
    ("SHORT ks = reinterpret_cast<SHORT(WINAPI*)(int)>(GU32(CODE_4ba1f0))(0x10);", "int16_t ks = (int16_t)key_async(0x10);", 1),
    ("    SHORT s = reinterpret_cast<SHORT(WINAPI*)(int)>(slot)(vk);\n    return (int)(u8)((u16)s >> 8);",
     "    (void)slot;\n    return (int)(u8)((u16)key_async(vk) >> 8);", 1),
    ("u32 key = GU32(CODE_4ba1f0);", "u32 key = 0;", 1),
    # showInfoCursorPanel through the register-saving asm helper -> a plain call
    ("P6_CallSavingRegs2(CODE_433190, FD(0xb8), FD(0xb0));", "cc(\"showInfoCursorPanel\", FD(0xb8), FD(0xb0));", 1),
    # host-pointer strcmp -> the same compare over the address space
    ("""        const char* a = reinterpret_cast<const char*>((GS(f_strpool_4c6064) + 64));
        const char* b = reinterpret_cast<const char*>((GS(g_golferTypes) + 16));
        int cmp;
        while (*a == *b) {                    // ML+0xcbdd loop
            if (*a == 0) { cmp = 0; break; }  // ML+0xcbe9 je (equal end)
            a++; b++;
        }
        if (*a != *b) cmp = (*(const u8*)a < *(const u8*)b) ? -1 : 1;  // ML+0xcc05 sbb/sbb""",
     """        u32 a = (GS(f_strpool_4c6064) + 64);
        u32 b = (GS(g_golferTypes) + 16);
        int cmp = 0;
        while (GU8(a) == GU8(b)) {            // ML+0xcbdd loop
            if (GU8(a) == 0) { cmp = 0; break; }  // ML+0xcbe9 je (equal end)
            a++; b++;
        }
        if (GU8(a) != GU8(b)) cmp = (GU8(a) < GU8(b)) ? -1 : 1;  // ML+0xcc05 sbb/sbb""", 1),
    # H1's wait: g_frameMs from the frame's clock value at its call site; the wait loop itself is pacing (dropped)
    ("""        DWORD (WINAPI* tgt)() = reinterpret_cast<DWORD(WINAPI*)()>(GU32(CODE_4ba21c));  // ML+0xf079 edi=[code:ba21c]
        eax = (int)tgt();                              // ML+0xf07f call edi (timeGetTime)""",
     """        eax = (int)clock_site(kSiteFrameMs);           // ML+0xf07f call edi (timeGetTime)""", 1),
    ("""        int c = (int)tgt();                            // ML+0xf09f call edi
        int t = esi + ((((G32(GS(g_flags)) & 0x20000) != 0 ? 0x4B : 0) + 0xAF) / 2);  // ML+0xf0a3..ML+0xf0be
        if ((u32)c < (u32)t) {                         // ML+0xf0c0 cmp ecx,eax; jae done
            do {
                cc("waitTicks", ebx, ebp);              // ML+0xf0c6 waitTicks (push ebp; push ebx)
                c = (int)tgt();                        // ML+0xf0ce call edi
                t = esi + ((((G32(GS(g_flags)) & 0x20000) != 0 ? 0x4B : 0) + 0xAF) / 2);  // ML+0xf0d2..ML+0xf0ed
            } while ((u32)c < (u32)t);                 // ML+0xf0ef cmp ecx,eax; jb loop
        }""",
     """        // ML+0xf09f..ML+0xf0ef: the wait until frame head + 87 / 125 ms (timeGetTime, waitTicks(1, 0)): pacing only, dropped
        // (re/analysis/sim/time_model.md section 3; no message is dispatched inside it, section 7). The harness's memory
        // writes made inside it are applied here, and a run whose last checkpoint was taken inside it stops the frame here
        // (TkBase::wait_point).
        wait_point();""", 1),
    # the two pumps of the frame body that dispatch recorded input: P6's pumpInput483bd0 and H1's pumpMessages (TraceInput)
    ('    cc("pumpInput483bd0");', '    pump_at("pumpInput483bd0", kPumpSiteP6);', 1),
    ('    cc("pumpMessages");', '    pump_at("pumpMessages", kPumpSiteH1);', 1),
]

FUNC_REPL = {
    # fild / fsqrt / __ftol on [F+0x34]: exact at the 0x027f word on SSE2 too (one correctly rounded sqrt, then truncation)
    "int P4_sqrtFtol(u32 F) {": """int P4_sqrtFtol(u32 F) {
    int p = FA(0x34);
    return (int)ftol_low32(std::sqrt(static_cast<double>(G32(p))));   // ML+0x7d6f fild, ML+0x7d73 fsqrt, ML+0x7d75 __ftol
}""",
    # the display's slot +0x9c (scale): drawing; the display object is outside the state (its vtable is in the heap)
    "void P6_DisplaySlot9c() {": """void P6_DisplaySlot9c() {
    // [[g_graphicsDevice] + 0x9c](1, 1, 1): the display's scale slot, drawing only (dropped; the object is the renderer's)
}""",
    "int P6_CallSavingRegs2(u32 addr, int a, int b) {": "",
}


def replace_func(text, head, body):
    i = text.index(head)
    depth, j = 0, text.index("{", i)
    while True:
        c = text[j]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                break
        j += 1
    return text[:i] + body + text[j + 1:]


def main():
    text = open(sys.argv[1], encoding="utf8").read()
    for old, new, n in EDITS:
        got = text.count(old)
        if got != n:
            sys.exit("edit expected %d, found %d: %s" % (n, got, old[:80]))
        text = text.replace(old, new)
    for head, body in FUNC_REPL.items():
        text = replace_func(text, head, body)
    left = re.findall(r"CODE_[0-9a-f]+|WINAPI|__asm|reinterpret_cast", text)
    if left:
        sys.exit("left: %s" % sorted(set(left)))
    header = ("// GENERATED by log/p3/p37a_conv.py from shim/src/re/c3fy.cpp (the C3 transcription of mainLoop's frame:\n"
              "// FramePreamble helpers .. H2) and edited by log/p3/p37a_patch_frame.py. Included inside struct Frame (tick.cpp).\n"
              "// Citations: ML+0x<off> = mainLoop + offset, <object>+0x<off>, <function>.\n")
    open(sys.argv[2], "w", encoding="utf8", newline="\n").write(header + text)


main()
