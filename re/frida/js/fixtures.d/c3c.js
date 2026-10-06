// Fixtures for C3 batch c3c (ui/audio/video leaves). Each returns { obj, ... } with pointers that vectors and
// `state` regions reference as "$name". Only our own synthetic values are written; no game text or game pointers.
// Constructors and field writers are seeded onto fresh Memory.alloc arenas, so no game structure is touched.
Object.assign(globalThis.DIFF_FIXTURES, {
  // Fill a region with a non-zero per-byte pattern so a reimplementation that omits a field (leaving the garbage)
  // reads RED against the original that overwrote it.
  _c3cGarbage(base, size, seed) {
    for (let i = 0; i < size; i++) base.add(i).writeU8(((seed * 131 + i * 37 + 0x5b) & 0xfe) | 1);
  },

  // Snd::setButtonA 0x00490c80: Sel490 objects. +4 (the "remember" flag) is kept 0 so the pointer global 0x0083b9b4
  // is never written (only the three int globals 0x0083b9b8/bc/c0 are), keeping every game pointer untouched.
  c3c_buttons() {
    const arena = Memory.alloc(4 * 0x10);
    const fx = { obj: arena };
    for (let i = 0; i < 4; i++) {
      const p = arena.add(i * 0x10);
      p.writeS32(0x1000 + i);   // +0: arbitrary payload, unread by setButtonA
      p.add(4).writeS32(0);     // +4: remember-flag clear
      fx['b' + i] = p;
    }
    return fx;
  },

  // Widget_setQuad 0x00476310: fresh widget slots (>= 0xa0 bytes) filled with garbage; setQuad overwrites the
  // dwords at +0x6c/+0x7c/+0x8c/+0x9c. State region: the whole arena.
  c3c_widgets_quad() {
    const SLOT = 0x100, N = 10;
    const arena = Memory.alloc(SLOT * N);
    const fx = { obj: arena };
    for (let i = 0; i < N; i++) {
      globalThis.DIFF_FIXTURES._c3cGarbage(arena.add(i * SLOT), SLOT, i + 1);
      fx['w' + i] = arena.add(i * SLOT);
    }
    return fx;
  },

  // Window::visible 0x004801f0: a tree linked by +0x130 (parent); visibility is bit 0 of the byte at +0xa0.
  // Chains produce both 0 and 1, including a self-hidden child under a visible parent and a visible child under a
  // hidden parent. Pure reader (no state region).
  c3c_windows() {
    const SLOT = 0x140, N = 10;
    const arena = Memory.alloc(SLOT * N);
    const w = i => arena.add(i * SLOT);
    const set = (i, flag, parent) => { w(i).add(0xa0).writeU32(flag); w(i).add(0x130).writePointer(parent); };
    set(0, 1, ptr(0));     // visible root               -> 1
    set(1, 0, ptr(0));     // hidden root                -> 0
    set(2, 1, w(0));       // visible under visible       -> 1
    set(3, 0, w(0));       // self-hidden under visible   -> 0
    set(4, 1, w(1));       // visible under hidden         -> 0
    set(5, 1, w(2));       // visible under 2 visible      -> 1
    set(6, 0, w(2));       // self-hidden deeper           -> 0
    set(7, 1, w(5));       // visible three deep           -> 1
    set(8, 1, w(4));       // visible under an ancestor-hidden node -> 0
    set(9, 0xfe, ptr(0));  // bit 0 clear in a non-zero flag word   -> 0
    const fx = { obj: arena };
    for (let i = 0; i < N; i++) fx['v' + i] = w(i);
    return fx;
  },

  // Widget_value 0x00477580: widget slots whose +0x5c points at a distinct record {+8 i, +0xc base0, +0x10 base}.
  // +0x5c is pre-set non-null so the lazy init from the global 0x0083ad44 is skipped and no game pointer is read.
  // Result is base + i when i >= 0, else base0; i and the bases vary so both branches and several values occur.
  c3c_widgets_value() {
    const N = 10;
    const widgets = Memory.alloc(N * 0x60);
    const recs = Memory.alloc(N * 0x14);
    const vals = [[7, 100, 1000], [-1, 50, 2000], [0, -9, 500], [-128, 11, 3], [255, 0, -17], [1, -1, -1],
                  [0x7fffffff, 123, -456], [-0x80000000, 77, 88], [2, 0x40000000, 0x3fffffff], [-2, -3, -4]];
    const fx = { obj: widgets };
    for (let i = 0; i < N; i++) {
      const r = recs.add(i * 0x14);
      r.add(8).writeS32(vals[i][0]);     // +8  i
      r.add(0xc).writeS32(vals[i][1]);   // +0xc base0
      r.add(0x10).writeS32(vals[i][2]);  // +0x10 base
      const wgt = widgets.add(i * 0x60);
      wgt.add(0x5c).writePointer(r);
      fx['w' + i] = wgt;
    }
    return fx;
  },

  // View::toLocal 0x0047b290 / View::toGlobal 0x0047b2d0: one view object plus a run of (x, y) coordinate pairs.
  // The view's +0x9c flags are 0 (bit 0x20 clear) and +0x130 parent is 0, so the inner transforms 0x0047b170 /
  // 0x0047b200 take the straight-line path reading the origin (+0x1ac/+0x1b0) and the offset (+0x1bc/+0x1c0).
  // Each vector mutates one 8-byte pair (x at +0, y at +4); the pairs are the state region, seeded with varied
  // values so the affine results differ across vectors.
  c3c_view() {
    const view = Memory.alloc(0x200);
    view.add(0x9c).writeU32(0);        // flags: no recursion
    view.add(0x130).writePointer(ptr(0));
    view.add(0x1ac).writeS32(40);      // origin x
    view.add(0x1b0).writeS32(-25);     // origin y
    view.add(0x1bc).writeS32(7);       // offset x
    view.add(0x1c0).writeS32(13);      // offset y
    const N = 10;
    const coords = Memory.alloc(N * 8);
    const seeds = [[0, 0], [100, 200], [-50, 75], [1, -1], [0x7fff, -0x8000], [320, 240], [-1000, 1000],
                   [5, 9], [-7, -3], [12345, -6789]];
    const fx = { obj: view, view, coords };
    for (let i = 0; i < N; i++) {
      coords.add(i * 8).writeS32(seeds[i][0]);
      coords.add(i * 8 + 4).writeS32(seeds[i][1]);
      fx['px' + i] = coords.add(i * 8);
      fx['py' + i] = coords.add(i * 8 + 4);
    }
    return fx;
  },

  // Constructor arenas: fresh slots filled with garbage. The ctor installs a vtable at +0 and zeroes its fields;
  // the whole arena is the state region, so any field the reimplementation forgets keeps its garbage and reads RED.
  _c3cCtorArena(slot, n) {
    const arena = Memory.alloc(slot * n);
    const fx = { obj: arena };
    for (let i = 0; i < n; i++) {
      globalThis.DIFF_FIXTURES._c3cGarbage(arena.add(i * slot), slot, i + 3);
      fx['o' + i] = arena.add(i * slot);
    }
    return fx;
  },
  c3c_cursor_objs() { return globalThis.DIFF_FIXTURES._c3cCtorArena(0x20, 10); },  // Cursor::ctor 0x00488490 (no callees -> 10)
  c3c_bink_objs() { return globalThis.DIFF_FIXTURES._c3cCtorArena(0x20, 10); },    // BinkPlayer::ctor 0x00487000 (no callees -> 10)
  c3c_snd_base_objs() { return globalThis.DIFF_FIXTURES._c3cCtorArena(0x80, 6); }, // Snd::ctorBase 0x00484150
  c3c_snd_derived_objs() { return globalThis.DIFF_FIXTURES._c3cCtorArena(0x80, 6); }, // Snd::ctorDerived 0x00484820
});
