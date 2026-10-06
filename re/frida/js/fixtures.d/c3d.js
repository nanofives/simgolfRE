// Fixtures for the c3d batch (View / Node / HotList geometry and hit-tests). Every object is a private Memory.alloc
// built with only the fields the functions read; the running game never walks these. Only synthetic values are
// written, never game text. Loaded after diff_fixtures.js.
Object.assign(globalThis.DIFF_FIXTURES, {
  // Shared coordinate buffer for the View transforms (toParent/fromParent 0x0047b170/0x0047b200): 12 (x, y) int
  // pairs with varied signs and magnitudes. The whole buffer is the state region; each vector points at one pair,
  // so the per-vector results differ. x<i>/y<i> expose the i-th pair's two int cells.
  _c3d_coords(fx) {
    const pairs = [[0, 0], [1, 2], [-3, 5], [100, -200], [-1000, 1000], [7, 7], [-50, 0], [0, -50],
                   [32768, -32768], [123, 456], [-9999, -1], [0x40000, 0x40000]];
    const buf = Memory.alloc(pairs.length * 8);
    fx.coords = buf;
    pairs.forEach((p, i) => {
      buf.add(i * 8).writeS32(p[0]); buf.add(i * 8 + 4).writeS32(p[1]);
      fx['x' + i] = buf.add(i * 8); fx['y' + i] = buf.add(i * 8 + 4);
    });
    return fx;
  },
  // A View: flags byte at +0x9c (bit 0x20 = recurse into parent, bit 0x8000 = subtract parent origin), parent
  // pointer at +0x130, origin at +0x1ac/+0x1b0, scroll at +0x1bc/+0x1c0. The parent has flags 0 so it does not
  // recurse further; it carries its own non-zero origin/scroll so the 0x8000 branch is observable.
  _c3d_view(flags) {
    const parent = Memory.alloc(0x200);
    parent.add(0x9c).writeU32(0);
    parent.add(0x1ac).writeS32(400); parent.add(0x1b0).writeS32(800);
    parent.add(0x1bc).writeS32(40); parent.add(0x1c0).writeS32(80);
    const view = Memory.alloc(0x200);
    view.add(0x9c).writeU32(flags);
    view.add(0x130).writePointer(parent);
    view.add(0x1ac).writeS32(100); view.add(0x1b0).writeS32(200);
    view.add(0x1bc).writeS32(10); view.add(0x1c0).writeS32(20);
    return { obj: view };
  },
  c3d_view_flat() { return globalThis.DIFF_FIXTURES._c3d_coords(globalThis.DIFF_FIXTURES._c3d_view(0)); },
  c3d_view_parent() { return globalThis.DIFF_FIXTURES._c3d_coords(globalThis.DIFF_FIXTURES._c3d_view(0x20)); },
  c3d_view_scroll() { return globalThis.DIFF_FIXTURES._c3d_coords(globalThis.DIFF_FIXTURES._c3d_view(0x8020)); },

  // Rectangle buffer for offsetRectToParent/offsetRectToLocal (0x0047b0d0/0x0047b120): 10 varied RECTs
  // {left, top, right, bottom}. The whole array is the state region; r<i> exposes the i-th RECT.
  _c3d_rects(fx) {
    const rects = [[0, 0, 0, 0], [10, 20, 30, 40], [-5, -5, 5, 5], [100, 200, 300, 400],
                   [-100, -100, -50, -50], [1, 2, 3, 4], [0, 0, 800, 600], [-1000, -1000, 1000, 1000],
                   [32767, -32768, 32768, -32767], [7, -3, 11, -1]];
    const buf = Memory.alloc(rects.length * 16);
    fx.rects = buf;
    rects.forEach((r, i) => { r.forEach((v, j) => buf.add(i * 16 + j * 4).writeS32(v)); fx['r' + i] = buf.add(i * 16); });
    return fx;
  },
  // Flat view (no parent recursion) and a scrolling-parent view, each with the rect buffer.
  c3d_rect_flat() { return globalThis.DIFF_FIXTURES._c3d_rects(globalThis.DIFF_FIXTURES._c3d_view(0)); },
  c3d_rect_parent() { return globalThis.DIFF_FIXTURES._c3d_rects(globalThis.DIFF_FIXTURES._c3d_view(0x8020)); },

  // Node tree for Node::contains (0x0047b080): child count at +0x22c, child-pointer array at +0x224. root has
  // children A, B, C; A has D, E; C has F. Leaves have count 0. Z is an unrelated node. contains is a pure read
  // (no state region); the results are 0 or 1.
  c3d_node_tree() {
    const mk = (kids) => {
      const n = Memory.alloc(0x230);
      n.add(0x22c).writeS32(kids.length);
      if (kids.length) {
        const arr = Memory.alloc(kids.length * 4);
        kids.forEach((k, i) => arr.add(i * 4).writePointer(k));
        n.add(0x224).writePointer(arr);
      }
      return n;
    };
    const D = mk([]), E = mk([]), F = mk([]), B = mk([]), Z = mk([]);
    const A = mk([D, E]);
    const C = mk([F]);
    const root = mk([A, B, C]);
    return { obj: root, root, A, B, C, D, E, F, Z };
  },

  // HotList for hitTest/hitTestRect (0x00492a90/0x00492b10): count at +0x58, entry-array pointer at +0x50, entry
  // stride 0x20 with rect {l,t,r,b} at +4, the "b" dword at +0x14 and the "a" dword at +0x18. 6 entries; entry 2
  // ([50,50,90,90]) and entry 4 ([60,60,100,100]) overlap, so a point in both returns the higher index (the scan
  // runs high to low). inRect 0x00492610 is half-open: l <= x < r, t <= y < b. scratch holds the three output cells
  // (aOut, bOut, outRect) and is the state region.
  c3d_hotlist() {
    const N = 6;
    const rects = [[0, 0, 10, 10], [20, 20, 40, 40], [50, 50, 90, 90], [100, 0, 120, 20],
                   [60, 60, 100, 100], [200, 200, 210, 210]];
    const entries = Memory.alloc(N * 0x20);
    for (let i = 0; i < N; i++) {
      const e = entries.add(i * 0x20);
      rects[i].forEach((v, j) => e.add(4 + j * 4).writeS32(v));
      e.add(0x14).writeS32(2000 + i);   // b (+0x14)
      e.add(0x18).writeS32(1000 + i);   // a (+0x18)
    }
    const hl = Memory.alloc(0x60);
    hl.add(0x50).writePointer(entries);
    hl.add(0x58).writeS32(N);
    const scratch = Memory.alloc(0x20);
    return { obj: hl, scratch, aOut: scratch, bOut: scratch.add(4), outRect: scratch.add(8) };
  },

  // Spot table for nearestMenuSpot (0x004326a0), the global at 0x004c7930: one entry per 4 bytes (short sx at +0,
  // short sy at +2), terminated by an entry whose sx is -1. 8 synthetic spots then the sentinel. The function scores
  // a query (qx, qy) against spot i as approxDistance(qy - sx, qx - sy) * (i + 6) / 8, so a query equal to a spot's
  // (sy, sx) scores 0 at that spot. nearestMenuSpot has no per-vector state; the result is the winning index (with 6
  // mapped to -1). (main-menu state only; the global is overwritten, as the base tile/table fixtures do.)
  c3d_spots() {
    const spots = [[10, 20], [30, 60], [-40, 10], [100, 100], [5, 80], [200, -10], [70, 70], [-5, -5]];
    const base = ptr('0x004c7930');
    spots.forEach((s, i) => { base.add(i * 4).writeS16(s[0]); base.add(i * 4 + 2).writeS16(s[1]); });
    base.add(spots.length * 4).writeS16(-1);   // sentinel: sx == -1 ends the scan
    return { obj: base };
  },
});
