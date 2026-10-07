// Fixtures for C3 batch c3ab (clearCost, landmarkName, stripNewline, trimSpaces, measureTextWidth,
// Widget_applyPalette, Widget_fillRectR, Widget_fillArea). Only our own synthetic values (no game text).
// Names are prefixed c3ab_. Allocations go through Memory.alloc (diff_hook rewrites it to __keepAlloc so
// field-only references are not freed); NativeCallbacks are pushed onto __diffKeepAlive by hand.
(function () {
  function keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; }
  function zero(p, size) { for (let i = 0; i < size; i += 4) p.add(i).writeU32(0); }

  // clearCost: seed the base tile tables (golf_tables), then set specific tile types at 0x005722e8 and the type
  // records at 0x00578370 + type*0x30 (byte +2, +3, +6) so each branch of clearCost is reached, and the course-type
  // byte 0x005a34e0. tileBlocked 0x0040bf60 returns nonzero only for type 0x14 or out-of-range (x,y).
  function cost(ct) {
    globalThis.DIFF_FIXTURES.golf_tables();
    const T = ptr('0x005722e8');                               // tile type [x*50 + y]
    const setType = (x, y, t) => T.add(x * 50 + y).writeS8(t);
    setType(1, 1, 4);     // type 4    -> 12
    setType(1, 2, 0x15);  // type 0x15 -> -16
    setType(1, 3, 5);     // rec[2] <= 0 -> -8
    setType(1, 4, 0x11);  // type 0x11 -> 32
    setType(1, 5, 0x12);  // type 0x12 -> 16 when ct==1, else the +3*+2/2 fall-through
    setType(1, 6, 6);     // rec[6]==13 -> +3*5/2
    setType(1, 7, 7);     // else       -> +3*+2/2
    setType(5, 5, 0x14);  // tileBlocked -> 0
    const rec = (t, m2, m3, m6) => {
      const b = ptr('0x00578370').add(t * 0x30);
      b.add(2).writeS8(m2); b.add(3).writeS8(m3); b.add(6).writeS8(m6);
    };
    rec(5, -1, 0, 0);       // byte +2 = -1 (<= 0)
    rec(0x11, 3, 0, 0);     // byte +2 = 3 (> 0)
    rec(0x12, 4, 7, 0);     // byte +2 = 4; ct0 fall-through gives 7*4/2 = 14
    rec(6, 5, 4, 13);       // byte +6 = 13 -> 4*5/2 = 10
    rec(7, 6, 5, 0);        // else -> 5*6/2 = 15
    ptr('0x005a34e0').writeS8(ct);                             // course-type byte
    return { obj: T };
  }

  // landmarkName appends into the scratch buffer 0x0051a068; start it empty so each append gives just the name.
  function landmark() { ptr('0x0051a068').writeU8(0); return {}; }

  // One block of small string buffers b0..bN with our own ASCII; stripNewline / trimSpaces each modify the one
  // buffer a vector passes. The caller lists every buffer as a state region.
  function strBuffers(strings) {
    const fx = {};
    for (let i = 0; i < strings.length; i++) {
      const b = Memory.alloc(64);
      zero(b, 64);
      b.writeUtf8String(strings[i]);
      fx['b' + i] = b;
    }
    fx.obj = fx.b0;
    return fx;
  }

  // Fake widget/surface for the thiscall forwarders: this+4 -> surface, surface[0] -> vtable. The vtable slots the
  // four functions use carry NativeCallbacks whose return (or write) is deterministic, so the A/B observes that the
  // reimplementation read this+4, did its branch, and passed the right arguments.
  //   slot 4  (+0x10) measureTextWidth: returns the length argument (echo of min(maxlen, strlen)).
  //   slot 13 (+0x34) fillRectR:        returns its argument (echo).
  //   slot 17 (+0x44) fillArea:         returns a*31 + b.
  //   slot 59 (+0xec) applyPalette:     writes its argument into widget+0xf0 (a state region).
  function wsurf(nofont) {
    const widget = Memory.alloc(0x100);
    zero(widget, 0x100);
    if (!nofont) {
      const vt = Memory.alloc(0x100);
      zero(vt, 0x100);
      vt.add(0x10).writePointer(keep(new NativeCallback(function (self, str, len) { return len; },
        'int', ['pointer', 'pointer', 'int'], 'thiscall')));
      vt.add(0x34).writePointer(keep(new NativeCallback(function (self, a) { return a; },
        'int', ['pointer', 'int'], 'thiscall')));
      vt.add(0x44).writePointer(keep(new NativeCallback(function (self, a, b) { return (Math.imul(a, 31) + b) | 0; },
        'int', ['pointer', 'int', 'int'], 'thiscall')));
      vt.add(0xec).writePointer(keep(new NativeCallback(function (self, x) { widget.add(0xf0).writeS32(x); },
        'void', ['pointer', 'int'], 'thiscall')));
      const surface = Memory.alloc(0x10);
      zero(surface, 0x10);
      surface.writePointer(vt);                 // surface[0] = vtable
      widget.add(4).writePointer(surface);      // widget+4 = surface
    }
    const fx = { obj: widget };
    // argument objects for applyPalette: +4 holds the value the virtual receives.
    for (let i = 0; i < 10; i++) {
      const a = Memory.alloc(0x10);
      zero(a, 0x10);
      a.add(4).writeS32(100 + i * 7);
      fx['arg' + i] = a;
    }
    // string buffers for measureTextWidth (our own ASCII of several lengths).
    const S = ['', 'a', 'xy', 'hello', 'abcdefghij'];
    for (let i = 0; i < S.length; i++) {
      const b = Memory.alloc(64);
      zero(b, 64);
      b.writeUtf8String(S[i]);
      fx['s' + i] = b;
    }
    return fx;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3ab_cost() { return cost(1); },
    c3ab_cost_ct0() { return cost(0); },
    c3ab_landmark() { return landmark(); },
    // '\n' at various positions, and buffers with none: b0 "ab\ncd", b1 "\nxyz", b2 "end\n", b3 "no newline here",
    // b4 "a\nb\nc" (first '\n' only), b5 "" (empty), b6 "\n", b7 "0123456789".
    c3ab_strnl() { return strBuffers(['ab\ncd', '\nxyz', 'end\n', 'no newline here', 'a\nb\nc', '', '\n', '0123456789']); },
    // leading/trailing spaces and tabs: b0 "  hi  ", b1 "trail   ", b2 "   lead", b3 "none", b4 "   " (all spaces),
    // b5 "" (empty), b6 "\tmix \t", b7 " a b c ".
    c3ab_trim() { return strBuffers(['  hi  ', 'trail   ', '   lead', 'none', '   ', '', '\tmix \t', ' a b c ']); },
    c3ab_wsurf() { return wsurf(false); },
    c3ab_wsurf_nofont() { return wsurf(true); },
  });
})();
