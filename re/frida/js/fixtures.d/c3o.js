// Fixtures for C3 batch c3o (Window::dispatchCommand, Window::key, Window::mouseDispatch254, setDragTarget,
// buildToolLabel, drawMarkupRun). Only our own synthetic values are written; the three markup tokens drawMarkupRun
// compares against are copied at run time from the game image (0x4e4254 / 0x4e4248 / 0x4e423c), never stored here.
// Names are prefixed c3o_. Allocations go through __keepAlloc (diff_hook rewrites Memory.alloc) and the
// NativeCallbacks are pushed onto __diffKeepAlive.
Object.assign(globalThis.DIFF_FIXTURES, {
  _c3oZero(p, n) { for (let i = 0; i < n; i += 4) p.add(i).writeS32(0); return p; },

  // Call log shared by every recording stub: +0 count, then up to 48 entries of 4 dwords {tag, id, a, b}. The id is
  // the dword at +4 of the object the stub was called on (0 for the cdecl callbacks). Both A/B arms call the same
  // stubs with the same inputs, so the log is identical when the call sequence is identical.
  _c3oLog(rec) {
    return (tag, id, a, b) => {
      const n = rec.readS32();
      if (n < 48) {
        const e = rec.add(4 + n * 16);
        e.writeS32(tag); e.add(4).writeS32(id); e.add(8).writeS32(a); e.add(12).writeS32(b);
      }
      rec.writeS32(n + 1);
    };
  },

  // Window objects (0x280 bytes, zeroed) sharing ONE fake vtable; panel objects (0x20 bytes) sharing a second fake
  // vtable with only slot 0x1c. Window +4 = id (not read by the three handlers), +8 = the base value the command /
  // key slots return, +0xc = what slot 0x44 returns (0 sends mouseDispatch254 on to slot 0x84).
  // Window vtable slots (thiscall stubs): 0x94 (cmd) -> +8 + cmd; 0xd4 (code) -> 0; 0x128 () -> 0;
  // 0x8c (a, vk) -> +8 + 3a + vk; 0x44 (a, b, rect*) -> +0xc (logs a, b and rect left/top);
  // 0x84 (x, y) -> 0; 0xb4 (x, y) -> 0. Callbacks (cdecl): +0x264 (cmd) -> 2cmd + 1; +0x25c (a, vk) -> a + vk;
  // +0x254 (x, y) -> 0. Focus child: +0x140 = 1, +0x138 = 1, +0x13c = a node whose +4 is the child window.
  // Hot-spot list at +0xbc: +0x58 count, +0x50 entries (32 bytes: rect l,t,r,b at +4, second out value at +0x14,
  // first out value at +0x18; read by HotList::hitTestRect 0x00492b10, searched from the last entry down).
  c3o_windows() {
    const rec = this._c3oZero(__keepAlloc(4 + 48 * 16), 4 + 48 * 16);
    const log = this._c3oLog(rec);
    const id = (p) => p.isNull() ? 0 : p.add(4).readS32();
    const keep = [];
    const cb = (f, ret, args, abi) => { const c = new NativeCallback(f, ret, args, abi); keep.push(c); return c; };
    const vt = this._c3oZero(__keepAlloc(0x200), 0x200);
    vt.add(0x94).writePointer(cb((t, cmd) => { log(0x94, id(t), cmd, 0); return (t.add(8).readS32() + cmd) | 0; },
      'int', ['pointer', 'int'], 'thiscall'));
    vt.add(0xd4).writePointer(cb((t, code) => { log(0xd4, id(t), code, 0); return 0; }, 'int', ['pointer', 'int'], 'thiscall'));
    vt.add(0x128).writePointer(cb((t) => { log(0x128, id(t), 0, 0); return 0; }, 'int', ['pointer'], 'thiscall'));
    vt.add(0x8c).writePointer(cb((t, a, vk) => { log(0x8c, id(t), a, vk); return (t.add(8).readS32() + 3 * a + vk) | 0; },
      'int', ['pointer', 'int', 'int'], 'thiscall'));
    vt.add(0x44).writePointer(cb((t, a, b, r) => {
      log(0x44, id(t), a, b); log(0x45, id(t), r.readS32(), r.add(4).readS32()); return t.add(0xc).readS32();
    }, 'int', ['pointer', 'int', 'int', 'pointer'], 'thiscall'));
    vt.add(0x84).writePointer(cb((t, x, y) => { log(0x84, id(t), x, y); return 0; }, 'int', ['pointer', 'int', 'int'], 'thiscall'));
    vt.add(0xb4).writePointer(cb((t, x, y) => { log(0xb4, id(t), x, y); return 0; }, 'int', ['pointer', 'int', 'int'], 'thiscall'));
    const pvt = this._c3oZero(__keepAlloc(0x40), 0x40);
    pvt.add(0x1c).writePointer(cb((t) => { log(0x1c, id(t), 0, 0); return 0; }, 'int', ['pointer'], 'thiscall'));
    const cb264 = cb((cmd) => { log(0x264, 0, cmd, 0); return (2 * cmd + 1) | 0; }, 'int', ['int'], 'default');
    const cb25c = cb((a, vk) => { log(0x25c, 0, a, vk); return (a + vk) | 0; }, 'int', ['int', 'int'], 'default');
    const cb254 = cb((x, y) => { log(0x254, 0, x, y); return 0; }, 'int', ['int', 'int'], 'default');
    globalThis.__diffKeepAlive.push(...keep);

    const panel = (pid) => { const p = this._c3oZero(__keepAlloc(0x20), 0x20); p.writePointer(pvt); p.add(4).writeS32(pid); return p; };
    const hot = (w, entries) => {
      const arr = this._c3oZero(__keepAlloc(0x20 * Math.max(entries.length, 1)), 0x20 * Math.max(entries.length, 1));
      entries.forEach((e, i) => {
        const p = arr.add(i * 0x20);
        [e.l, e.t, e.r, e.b].forEach((v, k) => p.add(4 + 4 * k).writeS32(v));
        p.add(0x14).writeS32(e.second); p.add(0x18).writeS32(e.first);
      });
      w.add(0xbc + 0x50).writePointer(arr); w.add(0xbc + 0x58).writeS32(entries.length);
    };
    const win = (wid, o = {}) => {
      const w = this._c3oZero(__keepAlloc(0x280), 0x280);
      w.writePointer(vt); w.add(4).writeS32(wid);
      w.add(8).writeS32(o.base || 0); w.add(0xc).writeS32(o.hit || 0);
      if (o.flags) w.add(0x9c).writeU32(o.flags);
      if (o.a0) w.add(0xa0).writeU8(o.a0);
      if (o.cbs) { w.add(0x264).writePointer(cb264); w.add(0x25c).writePointer(cb25c); w.add(0x254).writePointer(cb254); }
      if (o.panels) [0x30, 0x38, 0x40, 0x64].forEach((off) => w.add(off).writePointer(panel(wid * 16 + off)));
      if (o.child) {
        const node = this._c3oZero(__keepAlloc(0x10), 0x10);
        node.add(4).writePointer(o.child);
        w.add(0x140).writeS32(1); w.add(0x138).writeS32(1); w.add(0x13c).writePointer(node);
      }
      return w;
    };
    const HOT = [{ l: 0, t: 0, r: 100, b: 100, first: 11, second: 22 }, { l: 50, t: 50, r: 150, b: 150, first: 33, second: 44 }];
    const ignA = win(1, { flags: 0x200000, base: 5, cbs: true, panels: true });   // +0x9c bit 0x200000
    const ignB = win(2, { a0: 0x08, base: 5, cbs: true, panels: true });          // +0xa0 bit 0x8
    const plain = win(3, { base: 100, cbs: true, panels: true, hit: 1 });           // no child, callbacks, panels
    const bare = win(4, { base: -7 });                                              // no callbacks, no panels
    const leafYes = win(5, { base: 9, cbs: true, panels: true });                   // child that answers non-zero
    const parentYes = win(6, { base: 50, cbs: true, panels: true, child: leafYes });
    const leafIgn = win(7, { flags: 0x200000 });                                    // child that answers 0
    const parentNo = win(8, { base: 60, cbs: true, panels: true, child: leafIgn });
    const leafZero = win(9, { base: 0 });                                           // child whose sum can be 0
    const parentZero = win(10, { base: 70, cbs: true, child: leafZero });
    const grand = win(11, { base: 80, cbs: true, panels: true, child: parentNo });  // two levels down
    hot(plain, HOT);
    const miss = win(12, { base: 1, cbs: true, panels: true, hit: 0 }); hot(miss, HOT);   // slot 0x44 returns 0
    hot(bare, []);
    hot(ignA, HOT); hot(ignB, HOT);
    return { obj: plain, rec, ignA, ignB, plain, bare, parentYes, parentNo, parentZero, grand, miss, leafYes };
  },

  // setDragTarget 0x0047d840: twelve distinct zeroed 0x100-byte blocks. Their +0xb0 is 0, so currentFocusOwner
  // 0x0047f2f0 (the only reader of 0x0083ab60, 0x47f312) stops at its +0xb0 test if the game reads the global
  // between the two arms; the value is restored after every vector.
  c3o_drag() {
    const fx = {};
    for (let i = 0; i < 12; i++) { fx['b' + i] = this._c3oZero(__keepAlloc(0x100), 0x100); fx['b' + i].add(4).writeS32(i + 1); }
    fx.obj = fx.b0;
    return fx;
  },

  // buildToolLabel 0x0040a160 (main-menu state only): tile types 0x005722e8 all 0 except (2,2) = 0x14 (blocked for
  // tileBlocked 0x0040bf60); flag words 0x0053caf0 all 0, item bytes 0x005830b8 all 0xff, placed objects
  // 0x0058bcb8 all empty (type -1) except record 0 = {type 2 at x 10, y 10} and record 1 = {type 4 at x 12, y 20}
  // (types 2 and 4 have footprint 1 in the .data size bytes 0x004c26c0 + 20*type); then the per-cell values listed
  // in log/c3/c3o_purpose.md. Item names at 0x00578350 + 48*item for items 0, 3 and 0x7c get synthetic strings.
  c3o_label() {
    const types = ptr('0x005722e8'), flags = ptr('0x0053caf0'), items = ptr('0x005830b8');
    for (let i = 0; i < 2500; i++) { types.add(i).writeU8(0); flags.add(i * 2).writeU16(0); items.add(i).writeU8(0xff); }
    types.add(2 * 50 + 2).writeU8(0x14);
    for (let r = 0; r < 256; r++) ptr('0x0058bcb8').add(r * 16).writeS16(-1);
    const obj = (r, t, x, y) => { const o = ptr('0x0058bcb8').add(r * 16); o.writeS16(t); o.add(2).writeS16(x); o.add(4).writeS16(y); };
    obj(0, 2, 10, 10); obj(1, 4, 12, 20);
    const cell = (x, y, f, it) => { flags.add((x * 50 + y) * 2).writeU16(f); if (it !== undefined) items.add(x * 50 + y).writeU8(it); };
    cell(0, 0, 0x8000); cell(3, 3, 0x8000); cell(3, 4, 0x8000 | 0x400 | 0x200, 0x81);
    cell(10, 10, 0x400); cell(12, 20, 0x400 | 0x200, 0x81);
    cell(4, 4, 0x400); cell(4, 5, 0x400 | 0x20);
    [0x80, 0x81, 0x90, 0x93, 0x85, 0xfe].forEach((it, k) => cell(5, k, 0, it));
    cell(49, 49, 0, 0x81);
    [0, 3, 0x7c].forEach((it, k) => cell(6, k, 0, it));
    cell(6, 3, 0x200, 3);                                      // item < 0x7d wins over the flag word
    cell(7, 0, 0x200, 0x7d); cell(7, 1, 0x20, 0x7e); cell(7, 2, 0x1000, 0x7f); cell(7, 3, 0, 0x7d);
    cell(8, 0, 0x200 | 0x20); cell(8, 1, 0x1000 | 0x20); cell(8, 2, 0x1000); cell(8, 3, 0x40);
    const name = (it, s) => { const p = ptr('0x00578350').add(it * 48); for (let i = 0; i <= s.length; i++) p.add(i).writeU8(i < s.length ? s.charCodeAt(i) : 0); };
    name(0, 'Qa0'); name(3, 'Qbb3'); name(0x7c, 'Qccc124');
    return { obj: ptr('0x0051a068') };
  },

  // drawMarkupRun 0x00477280. Fonts: four font objects F0..F3 whose +4 points at a measurer; the measurers share ONE
  // fake vtable whose slot 0x10 (thiscall (text, count), called by measureTextWidth 0x00483930) returns
  // count * W with W = measurer +4 (3, 5, 7, 11), and logs {W, count, first char} into `rec`. Text objects (0x80
  // bytes): +0x38 style, +0x3c link state, +0x40 markup on, +0x4c dropdown state, +0x54 arrow width selector,
  // +0x5c..+0x68 fonts per style. Strings live in one buffer; each is NUL-terminated and followed by 16 zero bytes.
  c3o_markup() {
    const rec = this._c3oZero(__keepAlloc(4 + 48 * 16), 4 + 48 * 16);
    const log = this._c3oLog(rec);
    const mvt = this._c3oZero(__keepAlloc(0x40), 0x40);
    const meas = new NativeCallback((m, text, count) => {
      const w = m.add(4).readS32();
      log(0x10, w, count, text.isNull() ? -1 : text.readU8());
      return (count * w) | 0;
    }, 'int', ['pointer', 'pointer', 'int'], 'thiscall');
    globalThis.__diffKeepAlive.push(meas);
    mvt.add(0x10).writePointer(meas);
    const font = (w) => {
      const m = this._c3oZero(__keepAlloc(0x10), 0x10); m.writePointer(mvt); m.add(4).writeS32(w);
      const f = this._c3oZero(__keepAlloc(0x10), 0x10); f.add(4).writePointer(m); return f;
    };
    const F = [font(3), font(5), font(7), font(11)];
    const tobj = (o) => {
      const t = this._c3oZero(__keepAlloc(0x80), 0x80);
      t.add(0x38).writeS32(o.style || 0); t.add(0x40).writeS32(o.markup === undefined ? 1 : o.markup);
      t.add(0x4c).writeS32(o.drop || 0); t.add(0x54).writeS32(o.narrow || 0);
      const fonts = o.fonts || F;
      for (let i = 0; i < 4; i++) if (fonts[i]) t.add(0x5c + 4 * i).writePointer(fonts[i]);
      return t;
    };
    const fx = {
      rec,
      tm: tobj({}),                                      // markup on, all four fonts
      tpart: tobj({ fonts: [F[0], null, F[2], null] }),  // styles 1 and 3 fall back to the base font
      tplain: tobj({ markup: 0, fonts: [F[1]] }),        // markup off: one measure of the whole run
      tnofont: tobj({ markup: 0, fonts: [] }),           // base font 0: filled from the default font 0x0083ad44
      tdrop: tobj({ drop: 1 }),                          // starts in the dropdown state (scanToken)
      tlink: tobj({ style: 3 }),                         // starts in style 3 (skipToken)
      tnarrow: tobj({ narrow: 1 }),                      // arrow width 0x19 instead of 0x1e
      tpend: tobj({ drop: 2 }),                          // pending arrow at the end of the run
    };
    fx.obj = fx.tm;
    // Strings: plain ASCII plus markup characters; T_LINK / T_DD / T_DL are the image tokens (6 / 9 / 9 bytes).
    const tok = (va, n) => Array.from(new Uint8Array(ptr(va).readByteArray(n)));
    const T_LINK = tok('0x004e4254', 6), T_DD = tok('0x004e4248', 9), T_DL = tok('0x004e423c', 9);
    const b = (s) => Array.from(s, (c) => c.charCodeAt(0));
    const S = {
      s_plain: b('abcdefgh'),
      s_braces: b('ab{cd}ef'),
      s_brackets: b('x[yy]zzz'),
      s_mixed: b('a{b[c]d}e'),
      s_double: b('{{a}}b[[c]]d'),
      s_close: b('abc}'),
      s_closeb: b('abc]'),
      s_dollar: b('a$bcdefghijk'),
      s_link: [...b('ab'), ...T_LINK, ...b('uu=vv>cd')],
      s_linknogt: [...b('ab'), ...T_LINK, ...b('uu=vvvv')],
      s_dd: [...b('ab'), ...T_DD, ...b('(xy^zw')],
      s_ddend: [...b('q'), ...T_DD, ...b('(xyz')],
      s_dl: [...b('ab'), ...T_DL, ...b('(xy^zz')],
      s_ddbrace: [...b('ab{123456789cd^ef')],
      s_caret: b('ab^cd=ef>g'),
      s_eq: b('ab=cd>ef'),
    };
    let size = 0;
    for (const k in S) size += S[k].length + 1 + 16;
    const buf = this._c3oZero(__keepAlloc((size + 3) & ~3), (size + 3) & ~3);
    let off = 0;
    for (const k in S) {
      buf.add(off).writeByteArray(S[k]);
      fx[k] = buf.add(off);
      off += S[k].length + 1 + 16;
    }
    return fx;
  },

  // hitTestTree 0x0047f340. Windows (0x280 bytes) share ONE fake vtable whose slot 0x11c (thiscall, no arguments)
  // returns the window's +0xc and logs the call; surfaces are objects whose +4 points at a fake DirectDraw-surface
  // object D (+8 width, +0xc height, +0x10 pixel buffer, 256x256 32-bit pixels) sharing a SECOND fake vtable:
  // 0xd8 width, 0xdc height, 0x10 lock -> buffer, 0xe0 pitch (= width), 0xe4 format -> {32}, 0x24 unlock (read by
  // getPixel 0x00478df0 and its pixel-address helper 0x004796a0). Pixel values: `key` = the live transparent colour
  // (0x004e432c & 0x7fffffff, read at fixture time) or an opaque value != key; patterns by column:
  //   Dtop key iff x < 8, Dbottom key iff x < 10, Dleft key iff x < 12, Dright key iff x < 14,
  //   Demb (embedded surface at +0x274) key iff x < 8, zero iff 8 <= x < 16, else opaque.
  // Trees (all roots have flag 0x20 so the vector's point is used, except P_top):
  //   P (plain, rect +0x1ac (10,10,110,110), flags 0x20|0x1000000, a4 0, owner self, slot 0x11c 1) with children
  //     C0 (no 0x20, far rect: pass 1, fails), Cinv (invisible), C2 (0x20|0x8000: skipped by pass 3),
  //     C1 (0x20|0x1000000, rect (5,5,25,25), a4 1); variants P_b2 (+0x9c bit 2), P_v0 (slot 0x11c 0),
  //     P_n1m (no 0x1000000), P_100 (bit 0x100), P_nob0 (owner 0), P_leaf (no children), P_top (no 0x20: the point
  //     comes from 0x00839ab8/0x00839abc); P1 has one child Cg without 0x20 whose rect surrounds that global point
  //     (pass 1 hit); Winv is invisible.
  //   F (framed: a0 bit 2, outer rect +0x1bc (100,100,300,300), client rect (20,30,175,170), flags 0x20|0x10000000,
  //     +0x180 40, +0x184 30, +0x188 -1, surfaces top/right/left/bottom at +0x11c/+0x120/+0x124/+0x128, slot 0x11c 1)
  //     with children FC (0x20|0x8000|0x1000000, rect (0,0,40,15), a4 1: pass 2), FC2 (0x20, rect (40,40,60,60),
  //     slot 0x11c 0: pass 3), FC3 (0x8000 only: pass 1, far rect); variants F_v0, F_n1 (no 0x10000000), F_10
  //     (bit 0x10: top band +0x180), F_10_4 (0x10|0x400000: +0x184), F_b (+0x188 = 25), F_nob0, F_leaf.
  // Vector i calls (w_i, &px_i, &py_i); the coordinate cells and 0x0083ab18 are the state (the call log too).
  c3o_hittree() {
    const rec = this._c3oZero(__keepAlloc(4 + 48 * 16), 4 + 48 * 16);
    const log = this._c3oLog(rec);
    const keep = [];
    const cb = (f, ret, args) => { const c = new NativeCallback(f, ret, args, 'thiscall'); keep.push(c); return c; };
    const wvt = this._c3oZero(__keepAlloc(0x200), 0x200);
    wvt.add(0x11c).writePointer(cb((t) => { log(0x11c, t.add(4).readS32(), 0, 0); return t.add(0xc).readS32(); }, 'int', ['pointer']));
    const fmt = __keepAlloc(4); fmt.writeS32(32);
    const dvt = this._c3oZero(__keepAlloc(0x100), 0x100);
    dvt.add(0xd8).writePointer(cb((d) => d.add(8).readS32(), 'int', ['pointer']));
    dvt.add(0xdc).writePointer(cb((d) => d.add(0xc).readS32(), 'int', ['pointer']));
    dvt.add(0xe0).writePointer(cb((d) => d.add(8).readS32(), 'int', ['pointer']));
    dvt.add(0x10).writePointer(cb((d) => d.add(0x10).readPointer(), 'pointer', ['pointer']));
    dvt.add(0xe4).writePointer(cb((d) => fmt, 'pointer', ['pointer']));
    dvt.add(0x24).writePointer(cb((d, a) => 0, 'int', ['pointer', 'int']));
    globalThis.__diffKeepAlive.push(...keep);
    const key = ptr('0x004e432c').readU32() & 0x7fffffff;
    const opaque = (x, y) => { const v = (key + 1 + ((x + y) & 7)) & 0x7fffffff; return v === 0 || v === key ? 1 : v; };
    const N = 256;
    const dsurf = (fn) => {
      const px = __keepAlloc(N * N * 4);
      const a = new Uint32Array(N * N);
      for (let y = 0; y < N; y++) for (let x = 0; x < N; x++) a[y * N + x] = fn(x, y);
      px.writeByteArray(a.buffer);
      const d = this._c3oZero(__keepAlloc(0x20), 0x20);
      d.writePointer(dvt); d.add(8).writeS32(N); d.add(0xc).writeS32(N); d.add(0x10).writePointer(px);
      return d;
    };
    const Dtop = dsurf((x, y) => x < 8 ? key : opaque(x, y));
    const Dbottom = dsurf((x, y) => x < 10 ? key : opaque(x, y));
    const Dleft = dsurf((x, y) => x < 12 ? key : opaque(x, y));
    const Dright = dsurf((x, y) => x < 14 ? key : opaque(x, y));
    const Demb = dsurf((x, y) => x < 8 ? key : x < 16 ? 0 : opaque(x, y));
    const surf = (d) => { const s = this._c3oZero(__keepAlloc(0x10), 0x10); s.add(4).writePointer(d); return s; };
    const Stop = surf(Dtop), Sbottom = surf(Dbottom), Sleft = surf(Dleft), Sright = surf(Dright);
    const rect = (w, off, r) => r.forEach((v, k) => w.add(off + 4 * k).writeS32(v));
    let nextId = 1;
    const win = (o) => {
      const w = this._c3oZero(__keepAlloc(0x280), 0x280);
      w.writePointer(wvt); w.add(4).writeS32(nextId++); w.add(0xc).writeS32(o.vs || 0);
      w.add(0x9c).writeU32(o.f || 0); w.add(0xa0).writeU8(o.a0 === undefined ? 1 : o.a0); w.add(0xa4).writeS32(o.a4 || 0);
      if (o.parent) w.add(0x130).writePointer(o.parent);
      if (o.client) rect(w, 0x1ac, o.client);
      if (o.outer) rect(w, 0x1bc, o.outer);
      w.add(0x278).writePointer(Demb);                       // embedded surface +0x274: its +4 is the D object
      if (o.frame) {
        w.add(0x11c).writePointer(Stop); w.add(0x120).writePointer(Sright);
        w.add(0x124).writePointer(Sleft); w.add(0x128).writePointer(Sbottom);
        w.add(0x180).writeS32(40); w.add(0x184).writeS32(30); w.add(0x188).writeS32(o.b188 === undefined ? -1 : o.b188);
      }
      w.add(0xb0).writePointer(o.nob0 ? ptr(0) : w);
      return w;
    };
    const kids = (w, list) => {
      const arr = this._c3oZero(__keepAlloc(4 * Math.max(list.length, 1)), 4 * Math.max(list.length, 1));
      list.forEach((c, i) => arr.add(4 * i).writePointer(c));
      w.add(0x224).writePointer(arr); w.add(0x22c).writeS32(list.length);
    };
    const W = {};
    const FAR = [-1000, -1000, -999, -999];
    const pBase = { f: 0x20 | 0x1000000, client: [10, 10, 110, 110], vs: 1 };
    W.P = win(pBase);
    const pKids = [
      win({ f: 0, parent: W.P, client: FAR }),                                     // C0
      win({ f: 0, a0: 0, parent: W.P, client: FAR }),                              // Cinv
      win({ f: 0x20 | 0x8000, parent: W.P, client: [0, 0, 100, 100] }),           // C2
      win({ f: 0x20 | 0x1000000, parent: W.P, client: [5, 5, 25, 25], vs: 1, a4: 1 }),   // C1
    ];
    kids(W.P, pKids);
    const pVar = (name, o, withKids = true) => { W[name] = win(Object.assign({}, pBase, o)); if (withKids) kids(W[name], pKids); };
    pVar('P_b2', { f: pBase.f | 2 }); pVar('P_v0', { vs: 0 }); pVar('P_n1m', { f: 0x20 }); pVar('P_100', { f: pBase.f | 0x100 });
    pVar('P_nob0', { nob0: true }); pVar('P_leaf', {}, false); pVar('P_top', { f: 0x1000000 }, false);
    const gx = ptr('0x00839ab8').readS32(), gy = ptr('0x00839abc').readS32();
    W.P1 = win({ f: 0x20, client: [0, 0, 1, 1], vs: 1 });
    kids(W.P1, [win({ f: 0, parent: W.P1, client: [gx - 5, gy - 5, gx + 5, gy + 5] })]);   // Cg
    W.Winv = win({ f: 0x20, a0: 0, client: [0, 0, 1000, 1000] });
    const fBase = { f: 0x20 | 0x10000000, a0: 3, outer: [100, 100, 300, 300], client: [20, 30, 175, 170], vs: 1, frame: true };
    W.F = win(fBase);
    const fKids = [
      win({ f: 0x20 | 0x8000 | 0x1000000, parent: W.F, client: [0, 0, 40, 15], vs: 1, a4: 1 }),   // FC
      win({ f: 0x20, parent: W.F, client: [40, 40, 60, 60] }),                                    // FC2
      win({ f: 0x8000, parent: W.F, client: FAR }),                                               // FC3
    ];
    kids(W.F, fKids);
    const fVar = (name, o, withKids = true) => { W[name] = win(Object.assign({}, fBase, o)); if (withKids) kids(W[name], fKids); };
    fVar('F_v0', { vs: 0 }); fVar('F_n1', { f: 0x20 }); fVar('F_10', { f: fBase.f | 0x10 });
    fVar('F_10_4', { f: fBase.f | 0x10 | 0x400000 }); fVar('F_b', { b188: 25 }); fVar('F_nob0', { nob0: true });
    fVar('F_leaf', {}, false);
    const V = [
      ['Winv', 5, 5],
      ['P', 5, 5], ['P', 12, 40], ['P', 20, 70], ['P', 40, 70], ['P', 32, 16], ['P', 25, 16], ['P', 109, 109], ['P', 110, 50],
      ['P_b2', 40, 70], ['P_v0', 40, 70], ['P_n1m', 40, 70], ['P_100', 40, 70], ['P_nob0', 32, 16], ['P_leaf', 40, 70],
      ['P_top', 40, 70], ['P1', 0, 0], ['P1', 500, 500],
      ['F', 50, 50], ['F', 120, 103], ['F', 112, 103], ['F', 103, 120], ['F', 145, 120], ['F', 103, 280], ['F', 145, 280],
      ['F', 103, 200], ['F', 112, 200], ['F', 276, 200], ['F', 290, 200], ['F', 150, 160], ['F', 165, 175], ['F', 300, 300],
      ['F', 299, 299],
      ['F_v0', 145, 120], ['F_n1', 145, 120], ['F_10', 109, 135], ['F_10_4', 109, 135], ['F', 109, 135], ['F_b', 111, 272],
      ['F', 111, 272], ['F_b', 109, 280], ['F_nob0', 120, 103], ['F_leaf', 120, 103],
    ];
    const cells = this._c3oZero(__keepAlloc(V.length * 8), V.length * 8);
    const fx = { obj: W.P, rec, cells, n: V.length };
    V.forEach(([k, x, y], i) => {
      cells.add(i * 8).writeS32(x); cells.add(i * 8 + 4).writeS32(y);
      fx['w' + i] = W[k]; fx['px' + i] = cells.add(i * 8); fx['py' + i] = cells.add(i * 8 + 4);
    });
    for (const k in W) fx[k] = W[k];
    return fx;
  },
  // Window::resized 0x0047bc60. Parents (0x280 bytes) and children share ONE fake window vtable: slot 0x5c (a, b)
  // and slot 0xc (a, b, c, d) are logging stubs returning 0. Children's +0x278 surface objects share a SECOND fake
  // vtable: 0xd8 returns +8 (width), 0xdc returns +0xc (height). Children are flagged 0x20 with +0xa0 bit 1 set and
  // no parent (+0x130 = 0), so Window::moveTo 0x0047b420 -> relayoutClip 0x0047e140 only shifts their rects and
  // copies the clip rect onto itself (the 0x20 branch); every child lives in one block (`kids`), the state region.
  // Parents: client rect +0x1ac = (10, 20, 210, 170) (200 x 150). Children: Hc (surface 17 x 13), Hn (no surface),
  // Hf (surface 29 x 31, +0xa0 bit 2: moveTo shifts its outer rect), Vc (surface 19 x 23), Vn (no surface).
  // Variants: R_none, R_H, R_Hn, R_V, R_Vn, R_HV, R_HnV, R_HVn, R_HfV, R_40 (+0x9c bit 0x40), R_cb (callback +0x230),
  // R_sb (+0x150/+0x154/+0x158 children for layoutScrollbars 0x0047d570, +0x194 = 4, +0x180 = 6, +0x184 = 8).
  c3o_resized() {
    const rec = this._c3oZero(__keepAlloc(4 + 48 * 16), 4 + 48 * 16);
    const log = this._c3oLog(rec);
    const keep = [];
    const id = (t) => t.add(4).readS32();
    const wvt = this._c3oZero(__keepAlloc(0x200), 0x200);
    const c5c = new NativeCallback((t, a, b) => { log(0x5c, id(t), a, b); return 0; }, 'int', ['pointer', 'int', 'int'], 'thiscall');
    const c0c = new NativeCallback((t, a, b, c, d) => { log(0x0c, id(t), a, b); log(0x0d, id(t), c, d); return 0; },
      'int', ['pointer', 'int', 'int', 'int', 'int'], 'thiscall');
    const cb230 = new NativeCallback((a, b) => { log(0x230, 0, a, b); return 0; }, 'int', ['int', 'int'], 'default');
    const svt = this._c3oZero(__keepAlloc(0x100), 0x100);
    const sw = new NativeCallback((s) => s.add(8).readS32(), 'int', ['pointer'], 'thiscall');
    const sh = new NativeCallback((s) => s.add(0xc).readS32(), 'int', ['pointer'], 'thiscall');
    keep.push(c5c, c0c, cb230, sw, sh);
    globalThis.__diffKeepAlive.push(...keep);
    wvt.add(0x5c).writePointer(c5c); wvt.add(0xc).writePointer(c0c);
    svt.add(0xd8).writePointer(sw); svt.add(0xdc).writePointer(sh);
    const surf = (w, h) => { const s = this._c3oZero(__keepAlloc(0x10), 0x10); s.writePointer(svt); s.add(8).writeS32(w); s.add(0xc).writeS32(h); return s; };
    const NK = 8;
    const kids = this._c3oZero(__keepAlloc(NK * 0x280), NK * 0x280);
    let k = 0, nextId = 100;
    const child = (o) => {
      const c = kids.add(0x280 * k++);
      c.writePointer(wvt); c.add(4).writeS32(nextId++);
      c.add(0x9c).writeU32(0x20); c.add(0xa0).writeU8(o.framed ? 3 : 1);
      [3, 4, 33, 24].forEach((v, i) => c.add(0x1ac + 4 * i).writeS32(v + k));        // client rect
      [1, 2, 41, 30].forEach((v, i) => c.add(0x1bc + 4 * i).writeS32(v + k));        // outer rect
      if (o.surf) c.add(0x278).writePointer(o.surf);
      return c;
    };
    const Hc = child({ surf: surf(17, 13) }), Hn = child({}), Hf = child({ surf: surf(29, 31), framed: true });
    const Vc = child({ surf: surf(19, 23) }), Vn = child({});
    const S1 = child({ surf: surf(5, 7) }), S2 = child({}), S3 = child({});
    let pid = 1;
    const parent = (h, v, o = {}) => {
      const w = this._c3oZero(__keepAlloc(0x280), 0x280);
      w.writePointer(wvt); w.add(4).writeS32(pid++);
      [10, 20, 210, 170].forEach((x, i) => w.add(0x1ac + 4 * i).writeS32(x));
      if (o.f) w.add(0x9c).writeU32(o.f);
      if (h) w.add(0x26c).writePointer(h);
      if (v) w.add(0x270).writePointer(v);
      if (o.cb) w.add(0x230).writePointer(cb230);
      if (o.sb) {
        w.add(0x150).writePointer(S3); w.add(0x154).writePointer(S2); w.add(0x158).writePointer(S1);
        w.add(0x194).writeS32(4); w.add(0x180).writeS32(6); w.add(0x184).writeS32(8);
        [0, 0, 300, 0].forEach((x, i) => w.add(0x1bc + 4 * i).writeS32(x));
      }
      return w;
    };
    return {
      obj: kids, rec, kids,
      R_none: parent(null, null), R_H: parent(Hc, null), R_Hn: parent(Hn, null), R_V: parent(null, Vc),
      R_Vn: parent(null, Vn), R_HV: parent(Hc, Vc), R_HnV: parent(Hn, Vc), R_HVn: parent(Hc, Vn), R_HfV: parent(Hf, Vc),
      R_40: parent(Hc, Vc, { f: 0x40 }), R_cb: parent(Hc, Vc, { cb: true }), R_sb: parent(null, null, { sb: true }),
    };
  },
});
