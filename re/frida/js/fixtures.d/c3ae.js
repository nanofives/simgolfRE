// Fixtures for C3 batch c3ae (rateLot, matchDirective, sceneryLabel, TextView_endSegment, Button::setColorNormal /
// setColorPressed, Widget_fillRect, Widget_get). Only our own synthetic values are written; the one place game bytes
// are used is matchDirective, where a real keyword is copied out of the binary's table at runtime to build a matching
// input (no game text is authored in this file or recorded). Allocations go through Memory.alloc (diff_hook rewrites
// it to __keepAlloc so field-only references survive); NativeCallbacks are pushed onto __diffKeepAlive by hand.
(function () {
  function keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; }
  function zero(p, size) { for (let i = 0; i < size; i += 4) p.add(i).writeU32(0); }
  function rw(p, size) { try { Memory.protect(p, size, 'rw-'); } catch (e) {} }

  // --- rateLot: seed the tile field so clearCost 0x0042ee80 returns a varied cost per cell, the 18-record table at
  //     0x00575cb8 (stride 0x208), the per-record positions 0x0059aea8 (stride 0x18), and the two globals. ----------
  function ratelot(aura) {
    const T = ptr('0x005722e8');                      // tile type [x*50 + y]
    const TYPES = [4, 0x11, 0x15, 5, 6];              // none is 0x14 (blocked); each yields a nonzero clearCost
    for (let x = 0; x < 50; x++)
      for (let y = 0; y < 50; y++)
        T.add(x * 50 + y).writeS8(TYPES[(x * 7 + y * 3) % 5]);
    // type records 0x00578370 + t*0x30, bytes +2 (gate/mult) +3 (mult) +6 (==13 selector) so clearCost is:
    //   4 -> 12 (constant), 0x15 -> -16 (constant), 0x11 -> 32 (needs rec[2] > 0), 5 -> -8 (rec[2] <= 0),
    //   6 -> rec[3]*5/2 = 10 (rec[6] == 13).
    const rec = (t, b2, b3, b6) => {
      const b = ptr('0x00578370').add(t * 0x30); rw(b, 8);
      b.add(2).writeS8(b2); b.add(3).writeS8(b3); b.add(6).writeS8(b6);
    };
    rec(0x11, 3, 0, 0); rec(5, -1, 0, 0); rec(6, 4, 4, 13);
    ptr('0x005a34e0').writeS8(1);                     // course type (unused by these clearCost branches)
    // 18 records: zero, then seed four to cover active/inactive, n==0, flags bits, pos.x == -1 / != -1.
    const R = ptr('0x00575cb8'); rw(R, 18 * 0x208); zero(R, 18 * 0x208);
    const setRec = (i, active, n, m, s158, flags, ax, ay, bx, by) => {
      const r = R.add(i * 0x208);
      r.writeS8(active);
      r.add(8).writeS32(ax); r.add(0xc).writeS32(ay);
      r.add(0x18).writeS32(bx); r.add(0x1c).writeS32(by);
      r.add(0x20).writeS32(n); r.add(0x24).writeS32(m);
      r.add(0x158).writeS16(s158); r.add(0x200).writeS32(flags);
    };
    setRec(0, 1, 10, 4, 50, 1, 10, 10, 30, 30);       // flags bit0 only; pos.x = -1 (skip 3rd distance)
    setRec(1, 1, 20, 8, 80, 3, 5, 5, 40, 40);         // flags bit0 | bit1; pos set below (3rd distance taken)
    setRec(2, 1, 0, 4, 60, 1, 7, 7, 20, 20);          // +0x20 == 0 -> record skipped
    setRec(3, 0, 15, 4, 70, 1, 1, 1, 2, 2);           // byte +0 == 0 -> record skipped
    const P = ptr('0x0059aea8'); rw(P, 18 * 0x18); zero(P, 18 * 0x18);
    for (let i = 0; i < 18; i++) P.add(i * 0x18).writeS32(-1);   // default: skip the 3rd distance
    P.add(1 * 0x18).writeS32(5 << 10); P.add(1 * 0x18 + 4).writeS32(5 << 10);  // record 1: real position (5,5)
    ptr('0x00822c88').writeS32(1);                    // (3 - 1) * 100 = 200
    ptr('0x00543cd0').writeS32(aura ? 2 : 0);         // aura multiplier branch
    return { obj: T };
  }

  // --- matchDirective: copy a real keyword out of the table 0x004e4584 into each matching buffer, and a non-matching
  //     buffer for the -1 vectors. Each buffer has its own holder cell (the char** argument). -------------------------
  function dir() {
    const table = ptr('0x004e4584');
    const fx = {};
    const mkBuf = (s) => { const b = Memory.alloc(64); zero(b, 64); b.writeUtf8String(s); return b; };
    for (let i = 0; i < 6; i++) {
      const kw = table.add(i * 4).readPointer().readUtf8String();  // real keyword i (used at runtime only)
      const b = mkBuf(kw + ' zz');                                 // leading keyword + trailing junk for trimSpaces
      const h = Memory.alloc(4); h.writePointer(b);
      fx['b' + i] = b; fx['h' + i] = h;
    }
    for (let i = 6; i < 8; i++) {
      const b = mkBuf('@@nomatch@@');
      const h = Memory.alloc(4); h.writePointer(b);
      fx['b' + i] = b; fx['h' + i] = h;
    }
    fx.obj = fx.h0;
    return fx;
  }

  // --- sceneryLabel: seed the 100-entry lookup table 0x005689e8 (short a, b, idx) with three matches, the three
  //     golfer-name records 0x004d6098 + idx*0x230 with our own ASCII, and the tile/type tables decorationName reads
  //     so it appends nothing (tile type 0x20, record +6 = 0xd, course type 0 -> its case 0xd returns 1). ------------
  function scenery() {
    const S = ptr('0x005689e8'); rw(S, 100 * 8); zero(S, 100 * 8);
    const put = (i, a, b, idx) => { const r = S.add(i * 8); r.writeS16(a); r.add(2).writeS16(b); r.add(4).writeS16(idx); };
    put(0, 5, 7, 0); put(1, 8, 9, 1); put(2, 10, 11, 2);
    const names = ['SCENERY-A', 'SCENERY-B', 'SCENERY-C'];
    for (let k = 0; k < 3; k++) { const n = ptr('0x004d6098').add(k * 0x230); rw(n, 0x40); zero(n, 0x40); n.writeUtf8String(names[k]); }
    const T = ptr('0x005722e8');
    [[5, 7], [8, 9], [10, 11]].forEach(([a, b]) => T.add(a * 50 + b).writeS8(0x20));
    const r20 = ptr('0x00578370').add(0x20 * 0x30); rw(r20, 8); r20.add(6).writeS8(0xd);  // decorationName case 0xd
    ptr('0x005a34e0').writeS8(0);                     // course type != 1 -> case 0xd returns 1 (no append)
    ptr('0x0051a068').writeU8(0);                     // scratch buffer starts empty
    return { obj: ptr('0x0051a068') };
  }

  // --- TextView_endSegment: a `this` with the Widget_value sub-object at +0x5c, plus the three measurement globals. -
  function endseg(aa8, aa0, sub8) {
    const self = Memory.alloc(0x80); zero(self, 0x80);
    const sub = Memory.alloc(0x20); zero(sub, 0x20);
    sub.add(8).writeS32(sub8); sub.add(0x10).writeS32(100); sub.add(0xc).writeS32(50);
    self.add(0x5c).writePointer(sub);
    ptr('0x00839aa8').writeS32(aa8); ptr('0x00839aa0').writeS32(aa0); ptr('0x00839aa4').writeS32(7);
    return { obj: self };
  }

  // --- Button color setters: `this` with the active flag at +0x130 and the font sub-object at +0x274 (its +4 == 0 so
  //     Widget_applyPalette returns 7 without a vtable). Widget_setQuad / setQuad74 write into the font at +0x6c.. ---
  function btn(active) {
    const self = Memory.alloc(0x400); zero(self, 0x400);
    self.add(0x130).writeS32(active ? 1 : 0);
    // self+0x274+4 already 0 from zero(); nothing else to seed.
    return { obj: self };
  }

  // --- Widget_fillRect: a `this` whose +4 points at a surface whose vtable slot 25 (+0x64) writes a function of the
  //     five arguments into surface+0x80. nosurf leaves this+4 null. ------------------------------------------------
  function fr(withSurf) {
    const self = Memory.alloc(0x10); zero(self, 0x10);
    const surf = Memory.alloc(0x100); zero(surf, 0x100);
    if (withSurf) {
      const vt = Memory.alloc(0x100); zero(vt, 0x100);
      vt.add(0x64).writePointer(keep(new NativeCallback(function (s, p1, p2, p3, p4, p5, one) {
        surf.add(0x80).writeS32((Math.imul(p1, 7) + Math.imul(p2, 5) + Math.imul(p3, 3) + p4 + p5) | 0);
      }, 'void', ['pointer', 'int', 'int', 'int', 'int', 'int', 'int'], 'thiscall')));
      surf.writePointer(vt);
      self.add(4).writePointer(surf);
    }
    return { obj: self, surf: surf };
  }

  // --- Widget_get: ten `this` objects whose +0x5c sub-object has +0x10 set, plus one with +0x5c == 0 (null branch). -
  function get() {
    const fx = {};
    for (let i = 0; i < 10; i++) {
      const sub = Memory.alloc(0x20); zero(sub, 0x20); sub.add(0x10).writeS32(100 + i * 13);
      const self = Memory.alloc(0x80); zero(self, 0x80); self.add(0x5c).writePointer(sub);
      fx['g' + i] = self;
    }
    const gn = Memory.alloc(0x80); zero(gn, 0x80);  // +0x5c == 0 -> null branch
    fx.gnull = gn;
    fx.obj = fx.g0;
    return fx;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3ae_ratelot() { return ratelot(false); },
    c3ae_ratelot_aura() { return ratelot(true); },
    c3ae_dir() { return dir(); },
    c3ae_scenery() { return scenery(); },
    c3ae_endseg_onpos() { return endseg(5, 1000, 10); },   // Widget_value +8 >= 0 -> returns [+0x10]+[+8] = 110
    c3ae_endseg_onneg() { return endseg(5, 2000, -1); },   // Widget_value +8 <  0 -> returns [+0xc]     = 50
    c3ae_endseg_off() { return endseg(0, 3000, 10); },     // flag 0x00839aa8 == 0 -> pen unchanged
    c3ae_btn() { return btn(true); },
    c3ae_btn_off() { return btn(false); },
    c3ae_fr() { return fr(true); },
    c3ae_fr_nosurf() { return fr(false); },
    c3ae_get() { return get(); },
  });
})();
