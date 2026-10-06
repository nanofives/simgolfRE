// Fixtures for C3 batch c3b (util helpers). Every value here is our own synthetic ASCII / numbers; no game
// text or data is read or recorded. Loaded after diff_fixtures.js.
Object.assign(globalThis.DIFF_FIXTURES, {
  // Eight NUL-terminated test strings laid out one per 0x20-byte slot in a 0x100-byte block, for the in-place
  // trims trimLeadingSpace (0x004924e0) and trimTrailingSpace (0x00492570). Each slot is a vector pointer
  // ($s0..$s7); the whole block is the state region so each arm starts from the same strings. Cases: leading
  // only, trailing only, both, neither, all-space, empty, tab/newline both ends, spaces around inner words.
  c3b_strings() {
    const cases = ['  lead', 'trail  ', '  both  ', 'none', '    ', '', '\t\n mix \t', ' a b c '];
    const obj = Memory.alloc(0x100);
    const fx = { obj };
    cases.forEach((s, i) => { const p = obj.add(i * 0x20); p.writeUtf8String(s); fx['s' + i] = p; });
    return fx;
  },

  // String table for appendString (0x0045b9f0): signed word offsets at g_strOff 0x0059d81c indexed by id, the
  // strings at g_strs 0x0056fcb0 + offset, appended onto the text buffer g_text 0x0051a068 (the state region).
  // All 0x80 offsets start at -1 (so any unseeded id returns 0); ids 0..11 point at "str<id>" except ids 3 and 7
  // left at -1 to exercise the offset == -1 branch. g_text seeded with our own prefix "T:".
  c3b_strtable() {
    const off = ptr('0x0059d81c'), strs = ptr('0x0056fcb0');
    for (let i = 0; i < 0x80; i++) off.add(i * 2).writeS16(-1);
    for (let id = 0; id < 12; id++) {
      if (id === 3 || id === 7) continue;
      off.add(id * 2).writeS16(id * 8);
      strs.add(id * 8).writeUtf8String('str' + id);
    }
    ptr('0x0051a068').writeUtf8String('T:');
    return { obj: ptr('0x0051a068') };
  },

  // Text + key/repl strings for replaceInText (0x0045b7c0). g_text 0x0051a068 (state region) seeded with our own
  // "xKEYyKEYz"; keys/repls live in a separate block exposed as $k0..$k3 / $r0..$r3. k0 "KEY" (mid), k1 "yKEY"
  // (mid, longer), k2 "MISSING" (absent -> no change), k3 "x" (at the front); r3 "" is an empty replacement.
  c3b_replace() {
    ptr('0x0051a068').writeUtf8String('xKEYyKEYz');
    const keys = ['KEY', 'yKEY', 'MISSING', 'x'];
    const repls = ['Z', 'QQ', 'R', ''];
    const obj = Memory.alloc(0x100);
    const fx = { obj };
    let o = 0;
    keys.forEach((s, i) => { const p = obj.add(o); p.writeUtf8String(s); fx['k' + i] = p; o += 0x10; });
    repls.forEach((s, i) => { const p = obj.add(o); p.writeUtf8String(s); fx['r' + i] = p; o += 0x10; });
    return fx;
  },

  // Block pool for allocBlock (0x0043d5d0) and freeBlock (0x0043d520): 100 (start, size) regions at
  // g_blockStart 0x00820b70 and g_blockSize 0x00820d00 (int each, start < 0 = empty slot), both the state
  // regions. Seeded free-list reaches every branch: slot0 (0,10) and slot1 (15,5) let freeBlock(10,5) extend
  // slot0 to end 15 and merge slot1; slot2 (100,50) is an exact-consume target for allocBlock(50); slot4 (200,5)
  // and slot5 (300,20) are extra regions; slot3 and up are empty for freeBlock's no-adjacent path.
  c3b_blocks() {
    const start = ptr('0x00820b70'), size = ptr('0x00820d00');
    for (let i = 0; i < 100; i++) { start.add(i * 4).writeS32(-1); size.add(i * 4).writeS32(0); }
    const regions = [[0, 10], [15, 5], [100, 50], [-1, 0], [200, 5], [300, 20]];
    regions.forEach(([s, n], i) => { start.add(i * 4).writeS32(s); size.add(i * 4).writeS32(n); });
    return { obj: start };
  },
});
