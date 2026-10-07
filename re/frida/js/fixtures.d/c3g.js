// Fixtures for the c3g batch (ui text builders, window-corner sizing, widget-table copies, message-ticker start,
// golfer-panel hit test). Every fixture seeds only the tables/globals the function and its callees read, with our
// own synthetic ASCII and numbers; the running game's live window/draw lists are never handed to an A/B. Loaded
// after diff_fixtures.js; Memory.alloc is rewritten to __keepAlloc so allocations survive the run.
Object.assign(globalThis.DIFF_FIXTURES, {
  // --- shared text buffer helpers (appendNumber/appendCents/appendDate/appendHoleName/appendCourseTitle) ---
  // The buffer at 0x0051a068 is the state region; each append starts from the seeded value and is restored per
  // vector. Only our own ASCII is written.
  c3g_text_empty() { ptr('0x0051a068').writeUtf8String(''); return {}; },
  // appendCents's negative branch overwrites a trailing '+' with '-'; seed a buffer that ends in '+'.
  c3g_text_plus() { ptr('0x0051a068').writeUtf8String('9+'); return {}; },

  // appendHoleName (0x00407280): hole record stride 0x208, flags byte at 0x00575cb0 + hole*0x208, par byte at
  // 0x00575ab0 + hole*0x208; the par-name pointer tables at 0x004c2e88 (par 3) / 0x004c2e38 (par 4) / 0x004c2ed8
  // (par 5) are indexed by hole; appendString (0x0045b9f0) reads the word table at 0x0059d81c[hole] and we set it
  // to 0xffff so it appends nothing and returns 0. Hole 0: flags 0 -> "Hole 0" path; 1: flags 0x01 par 3; 2: flags
  // 0x80 par 4; 3: flags 0x81 par 5; 4: flags 0 -> "Hole 4"; 5: flags 0x01 par 7 (default -> par-5 table).
  c3g_holes() {
    ptr('0x0051a068').writeUtf8String('');
    const names = ['c3gP3a', 'c3gP3b', 'c3gP3c', 'c3gP3d', 'c3gP3e', 'c3gP3f'].map(s => {
      const m = Memory.alloc(16); m.writeUtf8String(s); return m; });
    const flags = [0, 0x01, 0x80, 0x81, 0, 0x01];
    const pars  = [0, 3, 4, 5, 0, 7];
    for (let h = 0; h < 6; h++) {
      ptr('0x00575cb0').add(h * 0x208).writeU8(flags[h]);
      ptr('0x00575ab0').add(h * 0x208).writeU8(pars[h] & 0xff);
      ptr('0x0059d81c').add(h * 2).writeU16(0xffff);        // appendString: no append, returns 0
      // one synthetic name pointer per par table so whichever branch runs reads a valid string
      ptr('0x004c2e88').add(h * 4).writePointer(names[h % names.length]);
      ptr('0x004c2e38').add(h * 4).writePointer(names[(h + 1) % names.length]);
      ptr('0x004c2ed8').add(h * 4).writePointer(names[(h + 2) % names.length]);
    }
    return {};
  },

  // appendCourseTitle (0x0040daa0): course index dword 0x0059bf90, course record stride 0x2e with the site byte at
  // 0x00571ff4 + course*0x2e, site-name string at 0x004c1ea9 + site*0x82, hole count dword 0x005685f0 (bucketValue
  // 0x0044faf0 of holes-1 selects the suffix), appendString table 0x0059d81c[0] set to 0xffff so the site name is
  // appended. _a: 5 holes; _b: 18 holes (a different bucketValue arm).
  _c3g_course(holes) {
    ptr('0x0051a068').writeUtf8String('');
    ptr('0x0059d81c').writeU16(0xffff);                     // appendString(0) -> 0
    ptr('0x0059bf90').writeS32(0);                          // course 0
    ptr('0x00571ff4').writeU8(0);                           // course 0 site = 0
    ptr('0x004c1ea9').writeUtf8String('c3gClub');           // site 0 name
    ptr('0x005685f0').writeS32(holes);
    return {};
  },
  c3g_course_a() { return globalThis.DIFF_FIXTURES._c3g_course(5); },    // bucketValue(4) = 0 -> default arm
  c3g_course_b() { return globalThis.DIFF_FIXTURES._c3g_course(18); },   // bucketValue(17) = 2
  // review 2026-10-07: the two fixtures above reached two of the four suffix arms and always appended the site name
  c3g_course_c() {                                                       // bucketValue(8) = 1, course 1 -> site 2
    globalThis.DIFF_FIXTURES._c3g_course(9);
    ptr('0x0059bf90').writeS32(1);
    ptr('0x00571ff4').add(0x2e).writeU8(2);
    ptr('0x004c1ea9').add(2 * 0x82).writeUtf8String('c3gSiteTwo');
    return {};
  },
  c3g_course_d() { return globalThis.DIFF_FIXTURES._c3g_course(19); },   // bucketValue(18) = 3
  c3g_course_e() { return globalThis.DIFF_FIXTURES._c3g_course(30); },   // bucketValue(29) = -1 -> default arm
  c3g_course_f() {                                                       // appendString(0) returns 1: no site name
    globalThis.DIFF_FIXTURES._c3g_course(9);
    ptr('0x0059d81c').writeU16(0);                                       // string 0 at the table's first byte
    return {};
  },

  // Window::calcSizeFromCorners (0x00481760): four fake corner objects (width at +0x18, height at +0x1c) and four
  // window objects, two with flags byte +0x9c bit 0x10 set (corners +0x52c/+0x534/+0x530/+0x538) and two clear
  // (corners +0x54c/+0x550/+0x530/+0x538), with different corner sizes so the two max() outputs at +0x1a4/+0x1a8
  // differ per object. +0x1a4 (8 bytes) of each window is the state region.
  c3g_calcsize() {
    const corner = (w, h) => { const c = Memory.alloc(0x20); c.add(0x18).writeS32(w); c.add(0x1c).writeS32(h); return c; };
    const win = (flagBit, sizes) => {
      const o = Memory.alloc(0x600);   // corner pointers live at +0x52c..+0x550, outputs at +0x1a4/+0x1a8
      o.add(0x9c).writeU8(flagBit);
      // corners: index 0..5 -> +0x52c,+0x530,+0x534,+0x538,+0x54c,+0x550
      const offs = [0x52c, 0x530, 0x534, 0x538, 0x54c, 0x550];
      offs.forEach((off, i) => o.add(off).writePointer(corner(sizes[i][0], sizes[i][1])));
      return o;
    };
    const s0 = [[10, 1], [2, 20], [30, 3], [4, 40], [50, 5], [6, 60]];
    const s1 = [[7, 70], [80, 8], [9, 90], [100, 11], [12, 120], [130, 13]];
    const o0 = win(0x10, s0);   // uses +0x52c/+0x534/+0x530/+0x538
    const o1 = win(0x00, s0);   // uses +0x54c/+0x550/+0x530/+0x538
    const o2 = win(0x10, s1);
    const o3 = win(0x00, s1);
    return { o0, o1, o2, o3 };
  },

  // resetWidgetTable (0x00495eb0) / initWidgetTable (0x00495d30): the 38-int template at 0x0083fe78 is the source;
  // the function writes a reordered copy into the passed object (a private 0x100-byte block here, never the live
  // table). Two patterns so the copied object contents differ between the _a and _b keys.
  _c3g_widget(seed) {
    const src = ptr('0x0083fe78');
    for (let i = 0; i < 38; i++) src.add(i * 4).writeS32(seed + i * 7);
    const obj = Memory.alloc(0x100);
    return { obj };
  },
  c3g_widget_a() { return globalThis.DIFF_FIXTURES._c3g_widget(0x1000); },
  c3g_widget_b() { return globalThis.DIFF_FIXTURES._c3g_widget(-0x2000); },

  // startMessage (0x0040cb00): reads the direction byte 0x00569498, the busy dword 0x0053df54, the mode dword
  // 0x00567afc, the difficulty dword 0x00822c88 and the text buffer 0x0051a068 (strlen); writes 0x0053df54,
  // 0x00569498, 0x005a34ec, 0x005a7144, 0x004c2e08, 0x005694a4, 0x005a6d40 (message copy) and 0x0056d1a8/0x0056d1ac
  // from Random::range (seed at 0x00822d9c, in the state region so both arms draw the same numbers). _idle: not
  // busy -> proceeds; _busy: direction byte 1 -> refused when prio<=0; _mode3: mode 3 -> always refused.
  _c3g_msg(dir, busy, mode) {
    ptr('0x0051a068').writeUtf8String('A MESSAGE OF SOME LENGTH');
    ptr('0x00569498').writeU8(dir);
    ptr('0x0053df54').writeS32(busy);
    ptr('0x00567afc').writeS32(mode);
    ptr('0x00822c88').writeS32(0);          // difficulty 0
    ptr('0x00822d9c').writeU32(0x12345678); // RNG seed (restored each vector via the state region)
    return {};
  },
  c3g_msg_idle() { return globalThis.DIFF_FIXTURES._c3g_msg(0, 0, 0); },
  c3g_msg_busy() { return globalThis.DIFF_FIXTURES._c3g_msg(1, 0, 0); },
  c3g_msg_mode3() { return globalThis.DIFF_FIXTURES._c3g_msg(0, 0, 3); },

  // hitGolferPanel435f00 (0x00435f00): pure predicate; reads only the mode dword 0x00567afc (the 4..8 hotspots are
  // active only when it is 3). _mode0: mode 0; _mode3: mode 3.
  c3g_panel_mode0() { ptr('0x00567afc').writeS32(0); return {}; },
  c3g_panel_mode3() { ptr('0x00567afc').writeS32(3); return {}; },
});

// c3i fix-up (2026-10-06): the three widget/window leaves were held back for too few vectors (the leaf gate needs
// >= 10). Each writes only through its `this` pointer, so ten private objects exercise the same body ten times; the
// state list names all ten objects and only the one a vector targets changes, so the ten vectors give ten distinct
// final states. Never the live window list or the live widget table.
Object.assign(globalThis.DIFF_FIXTURES, {
  // Window::calcSizeFromCorners 0x00481760: ten window objects, alternating the +0x9c flag bit 0x10 (which selects the
  // +0x52c/+0x534 vs +0x54c/+0x550 corner pair), each over four corner objects (width +0x18, height +0x1c) with sizes
  // that differ per window, so each window's computed (width, height) at +0x1a4/+0x1a8 differs.
  c3i_calcsize() {
    const corner = (w, h) => { const c = Memory.alloc(0x20); c.add(0x18).writeS32(w); c.add(0x1c).writeS32(h); return c; };
    const offs = [0x52c, 0x530, 0x534, 0x538, 0x54c, 0x550];
    const out = {};
    for (let i = 0; i < 10; i++) {
      const o = Memory.alloc(0x600);
      o.add(0x9c).writeU8(i & 1 ? 0x10 : 0x00);
      offs.forEach((off, j) => o.add(off).writePointer(corner((i + 1) * (j + 2), (j + 1) * (i + 3))));
      out['o' + i] = o;
    }
    return out;
  },

  // resetWidgetTable 0x00495eb0 / initWidgetTable 0x00495d30: the 38-int template at 0x0083fe78 is the source; ten
  // private 0x100-byte objects pre-filled with 0xcc (which no template word equals) receive the reordered copy, so the
  // copy is a visible change and the ten objects' written regions all differ in position within the state list.
  c3i_widget10() {
    const src = ptr('0x0083fe78');
    for (let i = 0; i < 38; i++) src.add(i * 4).writeS32(0x4000 + i * 7);
    const out = {};
    for (let i = 0; i < 10; i++) {
      const o = Memory.alloc(0x100);
      for (let j = 0; j < 0x100; j++) o.add(j).writeU8(0xcc);
      out['o' + i] = o;
    }
    return out;
  },
});
