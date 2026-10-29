"""Patches of the converter's output for the S4.5 assembler (5_assemble.py): PATCHES = (block, old, new) exact replacements and
(block, pattern, replacement, "re") DOTALL regex replacements; VOID_OFF = for each function that returns nothing, the dword (offset
from `this`) negative-control mode 1 changes after the body.

Frame cells (U-1440): locals the original passes by address live in 16-byte cells of the image's tail padding (frame_cell, as in
audio_chan.cpp, which uses cells 0..4). S4.5 numbers its cells from 8 and gives a function that can be on a call chain with another
one of its own a cell of its own:
  8  Midi_Device::drainCommandQueue / dispatchCommand (head, tail)       9  Midi_Device::loadFile / loadFromMemory (tag, header)
  10 Seq::loadFile / Midi_loadMemoryAndPreRoll (list out slot)           11 Seq_preRollTrack / stepTrackMeta (track, count)
  12 argument-slot cells of the one-argument handlers (Seq_cmdHandler12, getTrackFlagHigh, getListElementAt, emitEvent, ...)
"""

PATCHES = [
    # ---- Midi_Device::drainCommandQueue: the unlink is the shared list helper, the two ring cursors are a frame cell
    ("Midi_Device::drainCommandQueue", r"void c3fd_unlinkLocal\(.*?\n}\n",
     "void c3fd_unlinkLocal(Ctx &ctx, u32 self, int32_t hOff, u32 key) {\n"
     "  list_unlink(ctx, self + hOff, self + hOff + 4, self + hOff + 8, self + hOff + 0xc, key);\n}\n", "re"),
    ("Midi_Device::drainCommandQueue",
     "        snd_call(ctx, 0x0000196f, SndAbi::Thiscall, {self, (&h), (&t)}).eax;  // 0xa36d\n",
     "        const u32 cq = fcell(ctx, 0);  // the slots of `h` and `t` the callee takes by address (0xa36d)\n"
     "        st<u32>(ctx, cq, h);\n        st<u32>(ctx, cq + 4, t);\n"
     "        snd_call(ctx, 0x0000196f, SndAbi::Thiscall, {self, cq, cq + 4});  // 0xa36d\n"
     "        h = ld<u32>(ctx, cq);\n        t = ld<u32>(ctx, cq + 4);\n"),
    ("Midi_Device::drainCommandQueue", "        u32 e = ld<u32>(ctx, self + 0x50 + 4 * idx);  // 0xa390\n",
     "        u32 e = ld<u32>(ctx, self + 0x50 + 4 * idx);  // 0xa390\n"),
    # ---- Midi_resetFileState: the sound timer's period
    ("Midi_resetFileState", "    u32 timer = (c3dh_gSoundTimer(ctx));  // 0x1d4fd\n", "    u32 timer = gref(ctx, 0xb49ec);  // 0x1d4fd (g_soundTimer)\n"),
    ("Midi_resetFileState", "        unsigned period = *timer;  // 0x1d506\n", "        unsigned period = ld<uint32_t>(ctx, timer);  // 0x1d506\n"),
    ("Midi_resetFileState", "unsigned q = 0x3e8 / period;  // 0x1d517", "unsigned q = udiv32(0x3e8, period);  // 0x1d517"),
    ("Midi_resetFileState", "st<uint32_t>(ctx, self + 0x174, 0x10000 / q);  // 0x1d51f / 0x1d521", "st<uint32_t>(ctx, self + 0x174, udiv32(0x10000, q));  // 0x1d51f / 0x1d521"),
    # ---- Midi_Device::dispatchCommand
    ("Midi_Device::dispatchCommand",
     "        snd_call(ctx, 0x0000196f, SndAbi::Thiscall, {d, c3fl_u(ctx, &readL), c3fl_u(ctx, &writeL)}).eax;  // 0xa69d-0xa6a9\n",
     "        const u32 cq = fcell(ctx, 1);  // the slots of readL / writeL the callee takes by address (0xa69d-0xa6a9)\n"
     "        st<u32>(ctx, cq, readL);\n        st<u32>(ctx, cq + 4, writeL);\n"
     "        snd_call(ctx, 0x0000196f, SndAbi::Thiscall, {d, cq, cq + 4});\n"
     "        readL = ld<u32>(ctx, cq);\n        writeL = ld<u32>(ctx, cq + 4);\n"),
    ("Midi_Device::dispatchCommand", "snd_obj(ctx, &SoundState::g_soundThreadState).va()", "snd_arr(ctx, &SoundState::g_soundThreadState).va()"),
    # ---- Seq::loadFile / Midi_loadMemoryAndPreRoll / Seq_preRollTrack / Seq_stepTrackMeta (c3ex helpers)
    ("Seq::loadFile", "uint32_t c3ex_trackCount(Ctx &ctx, u32 self) { return c3ex_vslot(self, 0xb4)(self, 0); }",
     "uint32_t c3ex_trackCount(Ctx &ctx, u32 self) { return snd_vcall(ctx, self, 0xb4, SndAbi::Thiscall, {}).eax; }"),
    ("Seq::loadFile", "    u32 x = 0;\n    c3ex_listGetAt(ctx, self, &x, 1);  // 0x202c3\n",
     "    const u32 x = fcell(ctx, 4);  // the out slot of Seq_listGetAt (0x202c3)\n    st<u32>(ctx, x, 0);\n"
     "    c3ex_listGetAt(ctx, self, x, 1);  // 0x202c3\n"),
    ("Midi_loadMemoryAndPreRoll", "    u32 x = 0;\n    c3ex_listGetAt(ctx, self, &x, 1);  // 0x20387\n",
     "    const u32 x = fcell(ctx, 5);  // the out slot of Seq_listGetAt (0x20387)\n    st<u32>(ctx, x, 0);\n"
     "    c3ex_listGetAt(ctx, self, x, 1);  // 0x20387\n"),
    ("Midi_loadMemoryAndPreRoll",
     "            u32 t = 0;\n            if (!c3ex_listGetAt(ctx, self, &t, i)) st<uint8_t>(ctx, t + 0x38, static_cast<uint8_t>(ld<uint8_t>(ctx, t + 0x38) | 8));  // 0x203fa-0x20407\n",
     "            st<u32>(ctx, x + 4, 0);\n"
     "            if (!c3ex_listGetAt(ctx, self, x + 4, i)) {  // 0x203fa-0x20407\n"
     "                const u32 t = ld<u32>(ctx, x + 4);\n"
     "                st<uint8_t>(ctx, t + 0x38, static_cast<uint8_t>(ld<uint8_t>(ctx, t + 0x38) | 8));\n            }\n"),
    ("Seq_preRollTrack", r"uint32_t c3ex_walkEvents\(.*?\n}\n",
     "uint32_t c3ex_walkEvents(Ctx &ctx, u32 self, u32 t, u32 v) {\n"
     "  u32 p = ld<u32>(ctx, t + 0x2c);\n"
     "  if (c3ex_readVarLen(ctx, p, v)) return 0;\n"
     "  for (;;) {\n"
     "    const unsigned char stat = static_cast<uint8_t>(ld<uint8_t>(ctx, p + 1) & 0xf0);\n"
     "    if (stat == 0x90 || stat == 0xc0) return 0;\n"
     "    c3ex_advance(ctx, t, ld<u32>(ctx, v));\n"
     "    st<u32>(ctx, v, 0);\n"
     "    if (c3ex_playEvent(ctx, self, t, v) == 0x29) return 0x29;\n"
     "    c3ex_advance(ctx, t, ld<u32>(ctx, v));\n"
     "    p = ld<u32>(ctx, t + 0x2c);\n"
     "    if (c3ex_readVarLen(ctx, p, v)) return 0;\n"
     "  }\n}\n", "re"),
    ("Seq_preRollTrack",
     "    u32 t = 0;\n    if (c3ex_listGetAt(ctx, self, &t, i) == 0x24) return 0x24;  // 0x2061f-0x20627\n",
     "    const u32 tc = fcell(ctx, 6);  // the track slot (+0) and the byte-count slot (+4)\n    st<u32>(ctx, tc, 0);\n"
     "    if (c3ex_listGetAt(ctx, self, tc, i) == 0x24) return 0x24;  // 0x2061f-0x20627\n    const u32 t = ld<u32>(ctx, tc);\n"),
    ("Seq_preRollTrack", "    unsigned v = 0;\n    return c3ex_walkEvents(ctx, self, t, &v);  // 0x20656-0x206cf\n",
     "    st<u32>(ctx, tc + 4, 0);\n    return c3ex_walkEvents(ctx, self, t, tc + 4);  // 0x20656-0x206cf\n"),
    ("Seq_stepTrackMeta", "    unsigned v = 0;\n    return c3ex_walkEvents(ctx, self, t, &v);  // 0x20749-0x207b4\n",
     "    const u32 v = fcell(ctx, 7) + 4;\n    st<u32>(ctx, v, 0);\n    return c3ex_walkEvents(ctx, self, t, v);  // 0x20749-0x207b4\n"),
    # ---- unused parameters
    ("Seq_cmdHandler8", "uint32_t c3ew_u(Ctx &ctx, u32 p) { return p; }", "uint32_t c3ew_u(Ctx &, u32 p) { return p; }"),
    ("Midi_Device::dispatchCommand", "uint32_t c3fl_u(Ctx &ctx, u32 p) { return p; }", "uint32_t c3fl_u(Ctx &, u32 p) { return p; }"),
    ("Midi_loadMemoryAndPreRoll", "uint32_t mem, uint32_t unused3) {", "uint32_t mem, uint32_t) {"),
    ("Seq_copyChannelParams", "fldc<unsigned>(src, kDwords[i])", "ld<uint32_t>(ctx, src + kDwords[i])"),
    # ---- Midi_Device::noteOn: the slot Channel_allocVoiceSlot writes through its pointer argument
    ("Midi_Device::noteOn", "    unsigned slot;  // [esp+0x28], written by Channel_allocVoiceSlot\n",
     "    const u32 slotc = fcell(ctx, 17);  // [esp+0x28], written by Channel_allocVoiceSlot\n    st<u32>(ctx, slotc, 0);\n"),
    ("Midi_Device::noteOn", "{ins + 0x70, c3fl_u(ctx, &slot)}", "{ins + 0x70, slotc}"),
    ("Midi_Device::noteOn", "{ins + 0x70, slot}", "{ins + 0x70, ld<u32>(ctx, slotc)}"),
    # ---- Midi_Device::sendShortMsg
    ("Midi_Device::sendShortMsg", "c3fd_gref(ctx, kMidiDevice)", "gref(ctx, kMidiDevice)"),
    # ---- Midi_Device::setChannelVolume: the two direct calls through function pointers
    ("Midi_Device::setChannelVolume", "    VoiceSetLevel_t setLevel = static_cast<VoiceSetLevel_t>(snd_va(ctx, 0x383b0));\n", ""),
    ("Midi_Device::setChannelVolume", "setLevel(current, 0, amount);", "snd_call(ctx, 0x000383b0, SndAbi::Thiscall, {current, amount});"),
    ("Midi_Device::setChannelVolume",
     r"void\*\* vtbl = ld<u32>\(ctx, group\);\s*static_cast<GroupSlot40_t>\(vtbl\[0x40 / 4\]\)\(group, 0, ld<uint8_t>\(ctx, event \+ 1\)\);",
     "snd_vcall(ctx, group, 0x40, SndAbi::Thiscall, {ld<uint8_t>(ctx, event + 1)});", "re"),
    # ---- Seq_findTrackById
    ("Seq_findTrackById", "static_cast<TrackSetField8_t>(snd_va(ctx, 0x2e430))(elem, 0, static_cast<uint32_t>(value));",
     "snd_call(ctx, 0x0002e430, SndAbi::Thiscall, {elem, static_cast<uint32_t>(value)});"),
    # ---- Seq_buildTrack9: the out slot of Seq_listGetAt
    ("Seq_buildTrack9", "    u32 local = 0;  // 0x22a48\n", "    const u32 local = fcell(ctx, 12);  // 0x22a48\n    st<u32>(ctx, local, 0);\n"),
    ("Seq_buildTrack9", "{(self + 0x60), (&local), i}", "{(self + 0x60), local, i}"),
    ("Seq_buildTrack9", "        if (local) st<u32>(ctx, local + 0x458, node);  // 0x22a72-0x22a7a\n",
     "        if (ld<u32>(ctx, local)) st<u32>(ctx, ld<u32>(ctx, local) + 0x458, node);  // 0x22a72-0x22a7a\n"),
]

# global regex substitutions applied to every block: the converter's `fldc<T>(base, off)` field reads
GLOBAL_RE = [
    (r"fldc<void\*>\((\w+), (0x[0-9A-Fa-f]+|\d+)\)", r"ld<u32>(ctx, \1 + \2)"),
    (r"fldc<const char\*>\((\w+), (0x[0-9A-Fa-f]+|\d+)\)", r"ld<u32>(ctx, \1 + \2)"),
    (r"fldc<unsigned char>\((\w+), (0x[0-9A-Fa-f]+|\d+)\)", r"ld<uint8_t>(ctx, \1 + \2)"),
    (r"fldc<unsigned short>\((\w+), (0x[0-9A-Fa-f]+|\d+)\)", r"ld<uint16_t>(ctx, \1 + \2)"),
    (r"fldc<unsigned>\((\w+), (0x[0-9A-Fa-f]+|\d+)\)", r"ld<uint32_t>(ctx, \1 + \2)"),
    (r"fldc<int>\((\w+), (0x[0-9A-Fa-f]+|\d+)\)", r"ld<int32_t>(ctx, \1 + \2)"),
]

# void functions: the dword (offset from this) mode 1 changes after the body. Default 0.
VOID_OFF = {
    "Midi_advanceMsgRing": 0x20c,
    "Seq_setPlayRange": 0x20c,
    "Midi_resetFileState": 0x4c,
    "Track_resetReadState": 0x4c,
    "Seq_rewindTracks": 0x4c,
    "Seq_clearTrackFlag8All": 0x1a4,
    "Track_setField8RaisingMax": 8,
}
