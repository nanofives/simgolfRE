// Fixtures for C3 batch c3av: jgld.dll's 8-bit Surface drawing primitives (hline, vline, clear8, fillRectC8,
// ditherRectC8, fill8, fill2_8, dashedHLine8, dashedVLine8).
//
// Every surface is OURS: a 0x820-byte block whose +0 is the REAL Surface vtable (jgld.dll RVA 0x11d0b0,
// re/analysis/systems/jgld_2.md), so the accessors the primitives call are jgld's own code. Nothing live is
// touched: not the Display at 0x10128420, not a screen surface, not DirectDraw. The fields the vtable accessors
// read, proven by their bodies:
//   +0x24 depth          (slot +0xe4 returns this+0x24; the pixel-address switch at VA 0x100088b3 reads it)
//   +0x40 row stride     (slot +0xe0 returns it, VA 0x1000ab10)
//   +0x44 clip RECT      (slot +0xcc returns this+0x44, VA 0x1000a9b0)
//   +0x54 bounds RECT    (slot +0xd4 returns this+0x54, VA 0x1000aa50; width = +0x5c - +0x54, height = +0x60 - +0x58)
//   +0x4c0 pixel base    (slot +0x10 copies it to +0x4cc and bumps the use count at +0x4c8, VA 0x10008753)
// All eight pixel buffers live inside ONE block ($pix), so a single state region covers them and the final hash
// says which surface changed; the per-surface lock windows ($l0..$l7 = surface+0x4c0, 0x14 bytes) are separate
// state regions so a missing slot-+0x24 release reads RED.
Object.assign(globalThis.DIFF_FIXTURES, {
  c3av_surfaces() {
    const vt = Process.getModuleByName('jgld.dll').base.add(0x11d0b0);

    // One pixel block: surface i draws into $pix + i * 0x400. The 0x800-byte tail is slack for the vectors whose
    // clip rectangle is wider than the surface (s7), which the original walks past the nominal 0x400.
    const PIX = 0x2800, STRIDE = 0x400;
    const pix = Memory.alloc(PIX);
    for (let i = 0; i < PIX; i++) pix.add(i).writeU8((i * 37 + 11) & 0xff);

    const fx = { obj: pix, pix: pix };

    // depth, pitch, bounds {l,t,r,b}, clip {l,t,r,b}, hasBits
    const SPECS = [
      [8, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],    // s0 clip == bounds (clear8's fast path)
      [8, 32, [0, 0, 32, 16], [4, 2, 20, 12], true],    // s1 clip strictly inside the bounds
      [8, 19, [0, 0, 19, 11], [0, 0, 19, 11], true],    // s2 odd width and odd stride (w & 3 = 3)
      [8, 32, [0, 0, 32, 16], [1, 1, 31, 15], true],    // s3 clip offset by one on every edge
      [8, 32, [0, 0, 32, 16], [0, 0, 32, 16], false],   // s4 no pixel base: every pixel address is NULL
      [16, 16, [0, 0, 16, 10], [0, 0, 16, 10], true],   // s5 depth 16: the pixel address scales by two
      [12, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   // s6 depth 12: the pixel-address switch has no case
      [8, 32, [0, 0, 32, 16], [0, 0, 64, 40], true],    // s7 clip wider than the surface (addresses go NULL)
      // s8 is clear8's replacement for s7 on the slot-+0x44 path: the same "clip wider than the bounds" shape,
      // but the clip width (32) equals the stride, so the row skip it parks in the global at 0x1012847c is 0
      // instead of s7's -32. Its buffer is the 0x800-byte tail of $pix (0x2000..0x2800) and its fill is only
      // 4 x 32 = 128 contiguous bytes, so even a cursor walking at the game's own skip of 420 per row would stay
      // inside the block. There is no $l8 state region: $pix is the evidence here, and the eight lock windows
      // already in the state prove the slot-+0x24 release.
      [8, 32, [0, 0, 16, 8], [0, 0, 32, 4], true],      // s8 clip wider than the bounds, stride == clip width
    ];
    const surf = Memory.alloc(SPECS.length * 0x820);
    SPECS.forEach((sp, i) => {
      const [depth, pitch, b, k, hasBits] = sp;
      const s = surf.add(i * 0x820);
      for (let o = 0; o < 0x820; o += 4) s.add(o).writeU32(0);
      s.writePointer(vt);
      s.add(0x24).writeS32(depth);
      s.add(0x40).writeS32(pitch);
      for (let j = 0; j < 4; j++) s.add(0x44 + j * 4).writeS32(k[j]);
      for (let j = 0; j < 4; j++) s.add(0x54 + j * 4).writeS32(b[j]);
      s.add(0x4c0).writePointer(hasBits ? pix.add(i * STRIDE) : ptr(0));
      fx['s' + i] = s;
      fx['l' + i] = s.add(0x4c0);       // +0x4c0 base, +0x4c8 use count, +0x4cc current base
    });

    // The module global the 8-bit rectangle fill parks its row skip in (VA 0x1012847c, written at 0x10011eb2).
    fx.gskip = Process.getModuleByName('jgld.dll').base.add(0x12847c);

    // Rectangles for the two rectangle fills. Empty and inverted ones make IntersectRect fail; r_nullx sits inside
    // s7's wide clip rectangle but outside the surface, so the pixel address comes back NULL.
    const RECTS = {
      r_full: [0, 0, 32, 16], r_in: [4, 2, 20, 12], r_odd: [3, 1, 14, 10], r_even: [2, 2, 14, 10],
      r_1x1: [5, 5, 6, 6], r_w32h15: [0, 0, 32, 15], r_out: [100, 100, 110, 110], r_neg: [-10, -5, 8, 6],
      r_empty: [10, 10, 10, 10], r_inv: [20, 20, 5, 5], r_5x3: [1, 1, 6, 4], r_s16: [0, 0, 16, 10],
      r_s19: [0, 0, 19, 11], r_nullx: [33, 17, 50, 30], r_7x5: [2, 3, 9, 8], r_1x4: [5, 5, 6, 9],
    };
    const rblock = Memory.alloc(Object.keys(RECTS).length * 0x10);
    Object.keys(RECTS).forEach((name, i) => {
      const r = rblock.add(i * 0x10);
      RECTS[name].forEach((v, j) => r.add(j * 4).writeS32(v));
      fx[name] = r;
    });

    // Two 256-byte lookup tables for fill2_8: one bijective (7 is odd, so it is a permutation of 0..255) and one
    // that folds many inputs onto few outputs, so the mapped surface differs between them.
    const lut = Memory.alloc(0x100), lut2 = Memory.alloc(0x100);
    for (let i = 0; i < 0x100; i++) {
      lut.add(i).writeU8((i * 7 + 3) & 0xff);
      lut2.add(i).writeU8((i & 0x1f) | 0x40);
    }
    fx.lut = lut;
    fx.lut2 = lut2;
    return fx;
  },

  // Same surfaces, but with the game's render thread held still for the duration of the key's vectors. Used ONLY
  // by Surface::clear8 and Surface::fillRectC8, the two primitives the running game itself calls.
  //
  // Why: Surface::fillRectC8 0x10011dd0 keeps its per-call row skip in the module global at 0x1012847c — it parks
  // it at 0x10011eb2 and re-reads it once per row at 0x10011ee6 — and log/c3/alt/c3av_probe.py measured the game's
  // main thread entering that function about 8_000 times a second at the menu, with the global flipping between 0
  // and 420 about 43 times a second. Two threads inside one function with its loop state in a global corrupt each
  // other both ways: a value the game parks lands in OUR loop and moves our rows, and the value WE park lands in
  // the game's loop. The second one is what killed a boot. When the game fills full-width rows its own skip is 0
  // and the fill exactly covers its drawing buffer, so substituting any positive skip of ours makes it run off the
  // end of that buffer. There is no value that is safe to park: only the one the game parked itself is.
  //
  // So the vectors run with that thread suspended, and the global is put back to the value it had at suspend time
  // before it is resumed, so the game resumes on exactly the state it was stopped on. The resume is scheduled with
  // setTimeout(0): Frida's JS runtime is single-threaded, so the callback cannot run until the rpc `run` call that
  // is driving the vectors has returned. The window stops repainting for the second or so a key takes.
  //
  // Everything the vectors need (module lookups, allocations) is built BEFORE the suspend, and the vectors
  // themselves only call jgld code over the fixture's own memory: no allocation, no lock, no Win32 call while the
  // thread is down.
  c3av_surfaces_frozen() {
    const fx = globalThis.DIFF_FIXTURES.c3av_surfaces();
    const k32 = Process.getModuleByName('kernel32.dll');
    const openThread = new NativeFunction(k32.getExportByName('OpenThread'), 'pointer',
                                          ['uint32', 'int', 'uint32'], 'stdcall');
    const suspend = new NativeFunction(k32.getExportByName('SuspendThread'), 'uint32', ['pointer'], 'stdcall');
    const resume = new NativeFunction(k32.getExportByName('ResumeThread'), 'uint32', ['pointer'], 'stdcall');
    const closeHandle = new NativeFunction(k32.getExportByName('CloseHandle'), 'int', ['pointer'], 'stdcall');
    // Toolhelp enumerates threads in creation order, so [0] is the process's first thread: the one the probe saw
    // calling fillRectC8. Never the thread running this script.
    const threads = Process.enumerateThreads();
    const tid = threads.length ? threads[0].id : 0;
    if (!tid || tid === Process.getCurrentThreadId()) return fx;
    const h = openThread(0x0002 /* THREAD_SUSPEND_RESUME */, 0, tid);
    if (h.isNull()) return fx;
    if (suspend(h) === 0xffffffff) { closeHandle(h); return fx; }
    const parked = fx.gskip.readS32();
    // Status record so a run can be checked afterwards: `resumed` stays -1 until the timer fires and then holds
    // ResumeThread's return value, which is the thread's suspend count before the resume (1 = it is running again).
    globalThis.__c3avFreeze = { tid: tid, parked: parked, resumed: -1 };
    setTimeout(function () {
      fx.gskip.writeS32(parked);
      globalThis.__c3avFreeze.resumed = resume(h);
      closeHandle(h);
    }, 0);
    return fx;
  },
});
