// Fixtures for batch c3di (round 26, 2026-10-10): the Window tree teardown of golf_clean.exe. layoutNode 0x00479f30,
// Window::ctorInit 0x00480610, Tooltip::hide 0x00480360, Window::detach 0x0047b820, View_dtor 0x0047a3c0, Image_dtor
// 0x00474c40, Base4805a0::dtor 0x004805a0 and the deleting destructors / this-adjustors over them.
// Reimplementations: shim/src/re/c3di.cpp. Registry: re/frida/registry.d/c3di.py. Purposes: log/c3/c3di_purpose.md.
//
// These functions read and write the LIVE window globals (z-list 0x0083a2d8 / count 0x0083ab94, draw-list count
// 0x0083ab90, capture 0x0083aa98, tooltip owner/text 0x0083aac0/4, tooltip callback 0x0083aabc, tooltip image
// 0x0083aaa0, 0x0083aae0, 0x0083aaf0, drag capture 0x0083ab30/34, hover pair 0x0083ab40/44, modal top 0x0083ab54, the
// display 0x0083ad50, the input phase 0x0083ad4c written by 0x00483da0, 0x0083ac18 read by Window::ctorInit). Every key:
//   1. installs re/frida/js/heapstandin.js over malloc 0x004a504f (Vec_release 0x00479bf3), free 0x004a5007 and
//      operator delete 0x004a4ffc (free and delete only record), and re/frida/js/importstandin.js over ReleaseCapture
//      (0x00479b2a), timeKillEvent and KillTimer (Timer::stop 0x00486eda / 0x00486ef3);
//   2. freezes the game's first thread (re/frida/js/freeze.js) and, while it is down, points those globals at a
//      SYNTHETIC window tree in one arena ($arena, a state region of every key); all are written back before the resume.
// Windows are 0x600 bytes of a junk pattern with every field these chains dereference set explicitly. Their vtable is
// a recording one (every slot a NativeCallback that appends {tag, this, arg0, arg1} to the recorder at arena+REC), except
// in the destructor keys, whose bodies store the game's real vtables themselves. Shown windows (bit 0 of +0xa0) are
// never in the z-list, so rebuildDrawList 0x0047e450 never reaches 0x0047e330 / 0x0047e140 (the live draw list 0x00839ac0).
(function () {
  const keep = function (x) { globalThis.__diffKeepAlive.push(x); return x; };
  const ARENA = 0x10000;
  const WSZ = 0x600;          // windows: 26 * 0x600 = 0x9c00
  const OFF_IMG = 0x9c00;     // images: 6 * 0x2c0 = 0x1080 -> 0xac80
  const ISZ = 0x2c0;
  const OFF_OBJ = 0xad00;     // objects: 0x30 each, up to 64 -> 0xb900
  const OSZ = 0x30;
  const OFF_MISC = 0xb900;    // child arrays, node lists, blobs -> 0xe000
  const OFF_REC = 0xe000;     // recorder: +0 count, entries of 16 bytes from +0x10 (up to 0x1ff entries)
  const REC_MAX = 0x1ff;

  // ------------------------------------------------------------------------------------------------ recorder
  const S = { rec: null };
  function log(tag, self, a0, a1) {
    const rec = S.rec;
    if (rec === null) return;
    const n = rec.readU32();
    rec.writeU32(n + 1);
    if (n >= REC_MAX) return;
    const e = rec.add(0x10 + n * 16);
    e.writeU32(tag >>> 0);
    e.add(4).writePointer(self);
    e.add(8).writeU32(a0 >>> 0);
    e.add(12).writeU32(a1 >>> 0);
  }
  // thiscall callback with n stack arguments; fn(self, args) gives the result (default 0)
  function cb(tag, n, fn) {
    const types = ['pointer'];
    for (let i = 0; i < n; i++) types.push('uint32');
    return keep(new NativeCallback(function (self) {
      const a = Array.prototype.slice.call(arguments, 1);
      log(tag, self, a.length > 0 ? a[0] : 0, a.length > 1 ? (a[1] ^ ((a.length > 2 ? a[2] : 0) << 16)) : 0);
      return fn ? fn(self, a) : 0;
    }, 'uint32', types, 'thiscall'));
  }
  function vtable(tagBase, nargs, special) {
    const vt = keep(Memory.alloc(0x200));
    for (let s = 0; s < 0x200; s += 4) {
      const sp = special[s];
      vt.add(s).writePointer(sp ? cb(tagBase + s, sp[0], sp[1]) : cb(tagBase + s, nargs, null));
    }
    return vt;
  }

  // Window vtable: slot +0x1c (layoutNode 0x00479fdb / 0x0047a010, one argument) returns the replacement the fixture
  // stored at obj+8 (0 for a NULL argument); every other slot takes no stack argument (+8 0x00479fba, +0x10 0x00479f8a,
  // +0xf8 0x0047b826, +0x110 0x0047b8c1) and returns 0.
  const VT_WIN = vtable(0xA1000000, 0, {
    0x1c: [1, function (self, a) { return a[0] === 0 ? 0 : ptr(a[0]).add(8).readU32(); }] });
  // Object vtable: slot 0 (`push 1` / `push 3; call [eax]`: layoutNode 0x00479ff1, 0x0047a224, HotList::releaseArray
  // 0x0049283e) and slot +0xc (`push 1; call [eax + 0xc]`: layoutNode 0x0047a35d, Window::ctorInit 0x004806ab) take one
  // argument; slot +0xc returns the object's id (+4) and, when the object has an owner (+0x10), writes a node list
  // into the owner's +0x138 / +0x13c / +0x140 / +0x148 from the object's +0x14 / +0x20 / +0x18 / +0x1c (the only way
  // View_dtor's block 0x0047a407..0x0047a47c can run: layoutNode always leaves +0x138 at 0, 0x0047a142).
  const VT_OBJ = vtable(0xB1000000, 0, {
    0: [1, null],
    0xc: [1, function (self) {
      const owner = self.add(0x10).readPointer();
      if (!owner.isNull()) {
        owner.add(0x138).writeU32(self.add(0x14).readU32());
        owner.add(0x13c).writeU32(self.add(0x20).readU32());
        owner.add(0x140).writeU32(self.add(0x18).readU32());
        owner.add(0x148).writeU32(self.add(0x1c).readU32());
      }
      return self.add(4).readU32();
    }] });
  // Display vtable: +0x30 (Window::detach 0x0047b8d9), +0x8c (Image_releaseAll 0x00474cc8), +0xa4 (0x0047cfba) take one
  // argument, +0x9c (0x0047cf88) three; +0x20 (Timer::stop 0x00486eef) returns the synthetic HWND 0xbeef, +0xa4 returns
  // 0xd1a4 (it becomes Tooltip::hide's eax through 0x0047cdb0); +0x150 (0x00483dbb) and every other slot take none.
  const VT_DISP = vtable(0xD1000000, 0, {
    0x30: [1, null], 0x8c: [1, null], 0xa4: [1, function () { return 0xd1a4; }], 0x9c: [3, null],
    0x20: [0, function () { return 0xbeef; }] });
  // Tooltip callback 0x0083aabc (`call eax`, 0x0047cdb9): cdecl, no argument.
  const TIP_CB = keep(new NativeCallback(function () { log(0xCB000000, ptr(0), 0, 0); }, 'void', [], 'mscdecl'));

  globalThis.__c3di = { S: S, log: log, VT_WIN: VT_WIN, VT_OBJ: VT_OBJ, VT_DISP: VT_DISP, TIP_CB: TIP_CB,
                        K: { ARENA: ARENA, WSZ: WSZ, OFF_IMG: OFF_IMG, ISZ: ISZ, OFF_OBJ: OFF_OBJ, OSZ: OSZ,
                             OFF_MISC: OFF_MISC, OFF_REC: OFF_REC } };
})();

// ---------------------------------------------------------------------------------------------- the synthetic tree
// Window specs (name -> fields). z: in the z-list. kids/cap: child array +0x224 / count +0x22c / capacity +0x228 (each
// kid's +0x130 = this). nodes: node list at +0x138 (n nodes, circular {+4 win, +8 data, +0xc next, +0x10 prev}; own 0 ->
// +0x148 = 0, nodes freed; data: per node, 1 = a blob, 0 = NULL; win: the window named in node `at`; zeroCount: head set
// with +0x140 = 0). f9c: +0x9c flags (0x40 maps +0x26c/+0x270 through slot +0x1c, 0x100000 unlinks from the parent's
// node list). r26c / r270: replaceable objects (map: 'obj' -> slot +0x1c returns another object, 0 -> it returns NULL,
// absent -> returns the object itself). s150/s154/s158, s11c/s120/s124/s128, p14c, hot (HotList +0xbc array object
// +0x10c), o10 (+0x10, Window::detach 0x0047b8e5), o568 (Window::ctorInit 0x004806ab), img4 (Image +4 at +0x278),
// frees (Image +0x1b4.. slots at +0x428.., freed by Image_releaseAll 0x00474d08), shown (bit 0 of +0xa0), timer (the
// HotList timer at +0xdc: 'kill' -> id 0x7101 with +0x20 = 10 (timeKillEvent), 'killTimer' -> id 0x7202 with +0x20 =
// 0x40 (display slot +0x20, KillTimer), 'idle' -> bit 0 of +8 clear (0x00486f06 jmp 0x00483da0)), writer: an object at
// +0x128 whose slot +0xc writes a node list into this window's +0x138 (View_dtor's block).
globalThis.__c3di.WINS = [
  ['W0', { z: 1 }],
  ['W1', { z: 1, kids: ['W2', 'W3'], cap: 12, hot: 1 }],
  ['W2', { kids: ['W4'], cap: 0 }],
  ['W3', { s150: 1, r270: {} }],
  ['W4', {}],
  ['W5', { z: 1, f9c: 0x40, r26c: { map: 'obj' }, r270: { map: 0 } }],
  ['W6', { z: 1, r26c: {}, timer: 'idle' }],
  ['W7', { z: 1, s150: 1, s158: 1 }],
  ['W8', { z: 1, f9c: 0x00000040, r26c: {}, r270: {} }],
  ['W9', { f9c: 0x100000 }],
  ['W10', { z: 1, kids: ['W9'], cap: 0, nodes: { n: 3, own: 0, data: [1, 0, 1], at: 1, win: 'W9' } }],
  ['W11', { z: 1, nodes: { n: 3, own: 0, data: [1, 0, 1] }, p14c: 1, s150: 1, s154: 1, s158: 1, s11c: 1, s124: 1,
            s128: 1, hot: 1, frees: [0, 5, 39] }],
  ['W12', { z: 1, f9c: 0x100040, nodes: { n: 2, own: 1, data: [1, 1] }, r270: { map: 'obj' } }],
  ['W13', { z: 1, nodes: { n: 2, own: 0, data: [1, 1], zeroCount: 1 } }],
  ['W14', { z: 1, s11c: 1, s120: 1 }],
  ['W15', { z: 1, kids: ['W25'], cap: 11 }],
  ['W16', { p14c: 1, writer: { n: 2, own: 0, data: [1, 1], zeroCount: 1 } }],
  ['W17', { z: 1, kids: ['W18', 'W19', 'W20'], cap: 3 }],
  ['W18', { s154: 1 }],
  ['W19', { s154: 1 }],
  ['W20', { s154: 1 }],
  ['W21', { z: 1, writer: { n: 3, own: 0, data: [1, 1, 0] }, o568: 1, img4: 0x1d21 }],
  ['W22', { shown: 1, o568: 1 }],
  ['W23', { shown: 1, o10: 1, timer: 'kill', img4: 0x1d23, frees: [1, 2] }],
  ['W24', { z: 1, timer: 'killTimer', o568: 1, writer: { n: 2, own: 1, data: [1, 1] } }],
  ['W25', { f9c: 0x40, r26c: { map: 0 }, r270: { map: 'obj' }, hot: 1 }],
];
// Images (standalone, 0x2c0 bytes): img4 (+4, display slot +0x8c), frees (+0x1b4.. slots), hot (HotList objects at
// +0x104 (HotList +0xb4) and +0x2a4 (HotList +0x254)), timers (+0xd4 / +0x274: 'kill', 'killTimer', 'idle', or stopped).
globalThis.__c3di.IMGS = [
  ['I0', {}],
  ['I1', { img4: 0x1d01, frees: [0, 5, 39], hotA: 1, hotB: 1 }],
  ['I2', { timerA: 'idle', timerB: 'idle' }],
  ['I3', { timerA: 'kill', hotB: 1, frees: [7] }],
  ['I4', { timerB: 'killTimer', img4: 0x1d04 }],
  ['I5', { hotA: 1, frees: [38, 39] }],
];

(function () {
  const C = globalThis.__c3di, K = C.K;
  function junk(p, n, tag) { for (let k = 0; k < n; k += 4) p.add(k).writeU32((tag + k) >>> 0); }
  // Timer at t: +8 flags (bit 0 set: Timer::stop 0x00486f04 returns without 0x00483da0), +0xc id (0: no import call,
  // 0x00486ed1), +0x20 interval (< 0x32: timeKillEvent 0x00486eda, else display slot +0x20 and KillTimer 0x00486ef3).
  function timerInit(t, mode) {
    t.add(8).writeU32(mode === 'idle' ? 0x6 : 0x7);
    t.add(0xc).writeU32(mode === 'kill' ? 0x7101 : mode === 'killTimer' ? 0x7202 : 0);
    t.add(0x20).writeU32(mode === 'killTimer' ? 0x40 : 10);
  }
  C.junk = junk;
  C.timerInit = timerInit;

  // The tree in a fresh arena. Returns fx with the windows (W<n>, W<n>_274 = the Image sub-object), images (I<n>),
  // the display, the tooltip text and the helpers the globals step needs.
  C.buildTree = function (opts) {
    const arena = Memory.alloc(K.ARENA);
    junk(arena, K.ARENA, 0x5a000000);
    for (let k = 0; k < 0x2000; k += 4) arena.add(K.OFF_REC + k).writeU32(0);
    const fx = { arena: arena, obj: arena };
    C.S.rec = arena.add(K.OFF_REC);
    let misc = K.OFF_MISC, objN = 0;
    const take = function (n) {
      const p = arena.add(misc);
      misc += (n + 15) & ~15;
      if (misc > K.OFF_REC) throw new Error('c3di: misc region full');
      return p;
    };
    const newObj = function (map) {
      if (objN >= 64) throw new Error('c3di: object region full');
      const o = arena.add(K.OFF_OBJ + objN * K.OSZ);
      objN++;
      for (let k = 0; k < K.OSZ; k += 4) o.add(k).writeU32(0);
      o.writePointer(C.VT_OBJ);
      o.add(4).writeU32(0x0b000000 + objN * 0x10);
      o.add(8).writePointer(map === undefined ? o : (map === 0 ? ptr(0) : newObj()));
      return o;
    };
    const W = {};
    C.WINS.forEach(function (e, i) {
      W[e[0]] = arena.add(i * K.WSZ);
      fx[e[0]] = W[e[0]];
      fx[e[0] + '_274'] = W[e[0]].add(0x274);
    });
    // node list: n nodes {+0 tag, +4 window, +8 data, +0xc next, +0x10 prev}, circular
    const nodeList = function (spec) {
      const nodes = [];
      for (let k = 0; k < spec.n; k++) nodes.push(take(0x20));
      nodes.forEach(function (p, k) {
        p.writeU32(0x4e000000 + k);
        p.add(4).writePointer(spec.win && k === spec.at ? W[spec.win] : ptr(0x57570000 + k));
        p.add(8).writePointer(spec.data[k] ? take(0x10) : ptr(0));
        p.add(0xc).writePointer(nodes[(k + 1) % spec.n]);
        p.add(0x10).writePointer(nodes[(k + spec.n - 1) % spec.n]);
      });
      return { head: nodes[0], count: spec.zeroCount ? 0 : spec.n, own: spec.own ? 1 : 0 };
    };
    // Image fields at b (a standalone Image or a window's sub-object at +0x274). +0xac stays 0, so Image_releaseAll
    // never reaches Cache483010::dtor (0x00474cde).
    const imageInit = function (b, d) {
      b.add(4).writeU32(opts.disp ? (d.img4 || 0) : 0);
      b.add(0xac).writeU32(0);
      for (let k = 0; k < 40; k++) {
        b.add(0x1b4 + 4 * k).writeU32((d.frees || []).indexOf(k) >= 0 ? (0x0fee0000 + 4 * k) : 0);
      }
      b.add(0x104).writePointer(d.hotA ? newObj() : ptr(0));
      b.add(0x2a4).writePointer(d.hotB ? newObj() : ptr(0));
      timerInit(b.add(0xd4), d.timerA);
      timerInit(b.add(0x274), d.timerB);
    };
    C.WINS.forEach(function (e) {
      const d = e[1], w = W[e[0]];
      w.writePointer(C.VT_WIN);
      w.add(0x10).writePointer(d.o10 ? newObj() : ptr(0));
      w.add(0x9c).writeU32(d.f9c || 0);
      w.add(0xa0).writeU32(d.shown ? 0x5 : 0x4);
      w.add(0x10c).writePointer(d.hot ? newObj() : ptr(0));
      timerInit(w.add(0xdc), d.timer);
      w.add(0x26c).writePointer(d.r26c ? newObj(d.r26c.map) : ptr(0));
      w.add(0x270).writePointer(d.r270 ? newObj(d.r270.map) : ptr(0));
      w.add(0x130).writePointer(ptr(0));
      if (d.kids) {
        const arr = take(0x40);
        d.kids.forEach(function (k, j) { arr.add(4 * j).writePointer(W[k]); });
        w.add(0x224).writePointer(arr);
        w.add(0x22c).writeU32(d.kids.length);
        w.add(0x228).writeU32(d.cap);
      } else {
        w.add(0x224).writePointer(ptr(0));
        w.add(0x22c).writeU32(0);
        w.add(0x228).writeU32(0);
      }
      if (d.nodes) {
        const l = nodeList(d.nodes);
        w.add(0x138).writePointer(l.head);
        w.add(0x13c).writePointer(l.head);
        w.add(0x140).writeU32(l.count);
        w.add(0x148).writeU32(l.own);
      } else {
        [0x138, 0x13c, 0x140, 0x148].forEach(function (o) { w.add(o).writeU32(0); });
      }
      w.add(0x14c).writePointer(d.p14c ? take(0x10) : ptr(0));
      [0x150, 0x154, 0x158, 0x11c, 0x120, 0x124, 0x128].forEach(function (o) {
        w.add(o).writePointer(d['s' + o.toString(16)] ? newObj() : ptr(0));
      });
      if (d.writer) {
        const o = newObj(), l = nodeList(d.writer);
        o.add(0x10).writePointer(w);
        o.add(0x14).writePointer(l.head);
        o.add(0x18).writeU32(l.count);
        o.add(0x1c).writeU32(l.own);
        o.add(0x20).writePointer(l.head);
        w.add(0x128).writePointer(o);
      }
      w.add(0x568).writePointer(d.o568 ? newObj() : ptr(0));
      imageInit(w.add(0x274), d);
    });
    C.WINS.forEach(function (e) {
      (e[1].kids || []).forEach(function (k) { W[k].add(0x130).writePointer(W[e[0]]); });
    });
    C.IMGS.forEach(function (e, i) {
      const b = arena.add(K.OFF_IMG + i * K.ISZ);
      imageInit(b, e[1]);
      fx[e[0]] = b;
    });
    const disp = take(0x10);
    disp.writePointer(C.VT_DISP);
    disp.add(4).writeU32(0xd15b0000);
    fx.disp = disp;
    const tipText = take(0x10);
    tipText.writeUtf8String('tip');
    fx.tipText = tipText;
    fx.W = W;
    return fx;
  };
})();

(function () {
  const C = globalThis.__c3di;
  // opts: disp (display present), text (tooltip text 0x0083aac4 set), cb (tooltip callback 0x0083aabc set), ab40 / ab44
  // (hover pair), capture, tipOwner, drag, drag2, aaf0, modal (a window name, 0, or a number), ac18 (0x0083ac18 value)
  function build(opts) {
    const fx = C.buildTree(opts);
    const W = fx.W;
    // stand-ins first (heap, then imports), then the freeze and the globals
    const h = __heapStandIn({ module: 'golf_clean.exe', alloc: [0xa504f], free: [0xa5007, 0xa4ffc], arenaSize: 0x400,
                              exhaust: 'null' });
    fx.heap = h.arena;
    fx.freelog = h.log;
    const s = __importStandIn({ module: 'golf_clean.exe', imports: {
      'USER32.dll!ReleaseCapture': { args: 0, ret: 1 },
      'WINMM.dll!timeKillEvent': { ret: 0 },
      'USER32.dll!KillTimer': { ret: 1 } } });
    fx.implog = s.log;
    const G = function (v) {
      if (v === undefined || v === 0) return 0;
      return typeof v === 'number' ? v >>> 0 : W[v].toUInt32();
    };
    __freeze.thread();
    const zs = C.WINS.filter(function (e) { return e[1].z; }).map(function (e) { return e[0]; });
    if (zs.length > 16) throw new Error('c3di: z-list longer than 16');
    const zl = ptr(0x0083a2d8);
    __freeze.save(zl, 0x40);
    for (let i = 0; i < 16; i++) zl.add(4 * i).writeU32(i < zs.length ? W[zs[i]].toUInt32() : 0);
    __freeze.setU32(ptr(0x0083ab94), zs.length);
    __freeze.setU32(ptr(0x0083ab90), 0);
    __freeze.setU32(ptr(0x0083aae0), 0);
    __freeze.setU32(ptr(0x0083aaa0), 0);
    __freeze.save(ptr(0x0083ad4c), 4);
    __freeze.setU32(ptr(0x0083ad50), opts.disp ? fx.disp.toUInt32() : 0);
    __freeze.setU32(ptr(0x0083aabc), opts.cb ? C.TIP_CB.toUInt32() : 0);
    __freeze.setU32(ptr(0x0083aac4), opts.text ? fx.tipText.toUInt32() : 0);
    __freeze.setU32(ptr(0x0083aac0), G(opts.tipOwner));
    __freeze.setU32(ptr(0x0083aa98), G(opts.capture));
    __freeze.setU32(ptr(0x0083aaf0), G(opts.aaf0));
    __freeze.setU32(ptr(0x0083ab30), G(opts.drag));
    __freeze.setU32(ptr(0x0083ab34), G(opts.drag2));
    __freeze.setU32(ptr(0x0083ab40), G(opts.ab40));
    __freeze.setU32(ptr(0x0083ab44), G(opts.ab44));
    __freeze.setU32(ptr(0x0083ab54), G(opts.modal));
    __freeze.setU32(ptr(0x0083ac18), opts.ac18 >>> 0);
    // the recorder stops writing once the key's flush runs (no stray callback can write a freed arena)
    __freeze.atFlush(function () { C.S.rec = null; });
    delete fx.W;
    return fx;
  }

  const BASE = { disp: 1, text: 1, cb: 1, capture: 'W22', tipOwner: 'W7', drag: 'W5', drag2: 'W6', aaf0: 'W8',
                 modal: 'W22' };
  const V = function (o) { return Object.assign({}, BASE, o); };
  Object.assign(globalThis.DIFF_FIXTURES, {
    // hover pair: 0x0083ab40 = 0, 0x0083ab44 = W3 (a child of W1); tooltip text and callback set
    c3di_a() { return build(V({ ab40: 0, ab44: 'W3', ac18: 0x0ac18a01 })); },
    // 0x0083ab40 = W14 (taken over 0x0083ab44 = W4); no tooltip callback
    c3di_b() { return build(V({ cb: 0, ab40: 'W14', ab44: 'W4', ac18: 0x0ac18b02 })); },
    // 0x0083ab40 = W4 (a grandchild of W1, through W2), 0x0083ab44 = 0
    c3di_c() { return build(V({ ab40: 'W4', ab44: 0, ac18: 0x0ac18c03 })); },
    // no display, no tooltip text or callback, no hover window
    c3di_n() { return build(V({ disp: 0, text: 0, cb: 0, ab40: 0, ab44: 0, ac18: 0x0ac18d04 })); },
    // destructor keys (the bodies store the game's real vtables): as c3di_a, but the modal top 0x0083ab54 is no window,
    // so the real Window::detach never reaches the real modal pop 0x0047bbb0 (slot +0x110 of 0x004ba8b4)
    c3di_r() { return build(V({ ab40: 0, ab44: 'W3', modal: 0x0bad0054, ac18: 0x0ac18e05 })); },
    // as c3di_r with 0x0083ab40 = W14 and no tooltip callback
    c3di_rb() { return build(V({ cb: 0, ab40: 'W14', ab44: 'W4', modal: 0x0bad0054, ac18: 0x0ac18e06 })); },
    // as c3di_r with no display, no tooltip text, no hover window
    c3di_rn() { return build(V({ disp: 0, text: 0, cb: 0, ab40: 0, ab44: 0, modal: 0x0bad0054, ac18: 0x0ac18f07 })); },
  });
})();
