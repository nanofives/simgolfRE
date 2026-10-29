// Fixtures for step S4.6's emulator-only row Voice::update (sound.dll rva 0x1f8c0): the row has no C3 key, so this file is the
// vector set the port is judged on (injected into the fixture VM by log/roadmap/S4/6_run.py: sound_emu.script_list is extended
// with this file; nothing under re/frida is edited). Helpers are copies of re/frida/js/fixtures.d/c3fd.js's (freeze guard, quiet
// timer word, swapped module global, recording stub on an incremental-link thunk that only the A/B thread takes, recording
// vtables and objects). All values are synthetic; every object, vtable, list and log is allocated by the fixture.
//
// A Voice: a 0x800-byte block whose vtable slots +0x134 (the engine rate), +0xb4 (the note count), +0xb8, +0x20 and +0x14 are
// recording thiscall callbacks answering the dword at this + 0x100 + slot. Fields: +0x58 byte flags, +0x5c, +0xac, +0x214, +0x23c
// / +0x240 / +0x238 (count and index array), the doubles +0x720 / +0x728, the list at this+0x60 served by a stub on the thunk
// 0x15fa (node pointers at this+0x300+4k, result codes at this+0x340+4k, k >= 16 returns 0x24), the helper thunks 0x139d (cdecl,
// two arguments), 0x1c7b (thiscall, one), 0x2068 (thiscall, two) and 0x1956 (thiscall, one) as recording stubs, the registry
// list (head 0x100b5000, tail 0x100b5004, cursor 0x100b5008, count 0x100b500c) swapped per key, operator delete recorded.
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const keep = function (x) { globalThis.__diffKeepAlive.push(x); return x; };
  const snd = function (rva) { return Process.getModuleByName('sound.dll').base.add(rva); };
  function fill(p, n, b) { p.writeByteArray(new Array(n).fill(b)); return p; }
  function marker(p, n, tag) { for (let k = 0; k < n; k += 4) p.add(k).writeU32((tag + k) >>> 0); }
  function u32(p, off, v) {
    if (v instanceof NativePointer) p.add(off).writePointer(v); else p.add(off).writeU32(v >>> 0);
  }
  function block(i, tag, n) {
    const sz = n || 0x200;
    const b = fill(keep(Memory.alloc(sz)), sz, 0x3b);
    marker(b, sz, (tag >>> 0) + 0x10000 * i);
    return b;
  }
  let frozenNow = false;
  function guard() {
    if (frozenNow) return;
    __freeze.thread();
    frozenNow = true;
    __freeze.atFlush(function () { frozenNow = false; });
    __freeze.negOn();
  }
  function quietTimer(out) {
    guard();
    const g = snd(0xb49a8);
    out.g49a8 = g;
    __freeze.setU32(g, 5);
  }
  function swapGlobal(rva, value, out, name) {
    const g = snd(rva);
    if (name) out[name] = g;
    __freeze.setU32(g, (value instanceof NativePointer) ? value.toUInt32() : value);
    return g;
  }
  const LOGSZ = 0x820;
  function stubLog() { const l = keep(Memory.alloc(LOGSZ)); fill(l, LOGSZ, 0); return l; }
  function logCall(log, tag, a) {
    const n = log.readU32();
    if (4 + 0x20 * (n + 1) <= LOGSZ) {
      const p = log.add(4 + 0x20 * n);
      p.writeU32(tag >>> 0);
      for (let i = 0; i < a.length && i < 7; i++) p.add(4 + 4 * i).writeU32(ptr(a[i]).toUInt32());
    }
    log.writeU32(n + 1);
  }
  let stubLive = {};
  function stubOn(log, rva, nargs, thiscall, retfn, mask) {
    guard();
    const base = snd(rva), key = base.toString();
    if (stubLive[key]) { try { Interceptor.revert(base); } catch (e) { /* gone */ } delete stubLive[key]; Interceptor.flush(); }
    const tid = Process.getCurrentThreadId();
    const argt = (thiscall === true ? ['pointer'] : []).concat(Array(nargs).fill('pointer'));
    const abi = thiscall === true ? 'thiscall' : (thiscall === 'cdecl' ? 'default' : 'stdcall');
    const orig = keep(new NativeFunction(base, 'uint32', argt, abi));
    const cb = keep(new NativeCallback(function () {
      const a = Array.prototype.slice.call(arguments);
      if (Process.getCurrentThreadId() !== tid) return orig.apply(null, a);
      logCall(log, rva, mask ? a.map(function (v, k) { return mask.indexOf(k) >= 0 ? ptr(0x5ac0) : v; }) : a);
      const r = retfn ? retfn(a) : 0;
      return (typeof r === 'number') ? (r >>> 0) : ptr(r).toUInt32();
    }, 'uint32', argt, abi));
    Interceptor.replace(base, cb);
    Interceptor.flush();
    stubLive[key] = true;
    __freeze.atFlush(function () {
      if (!stubLive[key]) return;
      Interceptor.revert(base);
      Interceptor.flush();
      delete stubLive[key];
    });
  }
  function recVtable(log, slots) {
    const vt = fill(keep(Memory.alloc(0x200)), 0x200, 0);
    Object.keys(slots).forEach(function (k) {
      const off = parseInt(k, 10), n = slots[k];
      const argt = ['pointer'].concat(Array(n).fill('uint32'));
      vt.add(off).writePointer(keep(new NativeCallback(function () {
        const a = Array.prototype.slice.call(arguments);
        logCall(log, 0x5100 + off, a);
        return a[0].add(0x100 + off).readU32();
      }, 'uint32', argt, 'thiscall')));
    });
    return vt;
  }
  function heap(out, size) {
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], free: [0x4249a], arenaSize: size || 0x400 });
    out.arena = h.arena;
    out.freelog = h.log;
    return h;
  }
  function dbl(p, off, v) { p.add(off).writeDouble(v); }
  const NAN_SIG = [0x01, 0, 0, 0, 0, 0, 0xf0, 0x7f];     // a signaling NaN
  function dblBytes(p, off, bytes) { p.add(off).writeByteArray(bytes); }

  // A row: b58 (byte at +0x58), f5c, d720, d728 (a number or the bytes of a NaN), rate (the engine rate slot's answer), ac
  // (+0xac), f214, n23c (+0x23c), n240 (+0x240), ix (the index array), cnt (slot +0xb4's count), nodes (the list: f38, f34, p2c
  // non-zero or 0, i4c, i50), res (result codes by index; an index past the nodes ends the list with 0x24), h139 (the answer of the
  // thunk 0x139d), r2068 (the answer of playEvent's thunk), arg (the ticks argument).
  const N = function (f38, f34, p2c, i4c, i50) { return { f38: f38, f34: f34, p2c: p2c, i4c: i4c, i50: i50 }; };
  const VU = [
    { b58: 0, arg: 100, d720: 0, d728: 1.0, ac: 0, f214: 0, nodes: [] },                                              // 0 ticks path
    { b58: 0, arg: 7, d720: 3.5, d728: 0.25, ac: 0, f214: 0x20, nodes: [N(0x08, 0, 0, 0, 0)] },                        // 1 node, p2c 0
    { b58: 2, f5c: 50, rate: 1000, d720: 100, d728: 0.5, ac: 0, f214: 0, nodes: [] },                                  // 2 timed, no nodes
    { b58: 0, arg: 1, d720: 100, d728: 2.0, ac: 0, f214: 0, h139: 7, r2068: 0,
      nodes: [N(0x08, 0, 0x1234, 3, 4)] },                                                                              // 3 helper rounds
    { b58: 0, arg: 1, d720: 100, d728: 2.0, ac: 0, f214: 0, h139: 7, r2068: 0x29,
      nodes: [N(0x08, 0, 0x1234, 3, 4)] },                                                                              // 4 playEvent 0x29
    { b58: 0, arg: 1, d720: 5, d728: 2.0, ac: 0, f214: 0, h139: 7, nodes: [N(0x08, 0, 0x1234, 3, 4)] },               // 5 not below D
    { b58: 0, arg: 3, d720: 1, d728: 1.0, ac: 8, f214: 0, nodes: [N(0x08, 0, 0x55, 1, 1)] },                           // 6 bit 3 of ac
    { b58: 2, f5c: 3, rate: 10, d720: 1000, d728: 1.0, ac: 0, f214: 0x200, cnt: 3, n23c: 1, n240: 2, ix: [1, 2],
      nodes: [N(0, 0, 0, 0, 0), N(0, 0, 0, 0, 0)] },                                                                    // 7 low, bit 9 arm, index list
    { b58: 2, f5c: 3, rate: 10, d720: 1000, d728: 1.0, ac: 0, f214: 0x220, cnt: 0, n23c: 0, n240: 2, ix: [0, 1],
      nodes: [N(0, 0, 0, 0, 0)] },                                                                                      // 8 bit 9 with bit 5, no list
    { b58: 2, f5c: 3, rate: 10, d720: 1000, d728: 1.0, ac: 0, f214: 0x220, cnt: 2, n23c: 1, n240: 3, ix: [0, 9, 1],
      nodes: [N(0, 0, 0, 0, 0), N(0, 0, 0, 0, 0)], res: [0, 0x24, 0] },                                                 // 9 list lookups
    { b58: 2, f5c: 3, rate: 10, d720: 1000, d728: 1.0, ac: 0, f214: 0x800, cnt: 2, nodes: [] },                         // 10 bit 11: unlink
    { b58: 2, f5c: 3, rate: 10, d720: 1000, d728: 1.0, ac: 0, f214: 0x800, cnt: 0, nodes: [] },                         // 11 bit 11, no notes
    { b58: 2, f5c: 3, rate: 10, d720: 1000, d728: 1.0, ac: 0, f214: 0x0, cnt: 1, nodes: [] },                           // 12 low, no arm
    { b58: 0, arg: 2, d720: 0, d728: 1.0, ac: 0x208, f214: 0, cnt: 1, nodes: [] },                                      // 13 stop arm, bit 9 of ac
    { b58: 0, arg: 2, d720: 0, d728: 1.0, ac: 0x8, f214: 0, cnt: 2, nodes: [] },                                        // 14 stop arm, bit 9 clear
    { b58: 0, arg: 2, d720: 0, d728: 1.0, ac: 0x4, f214: 0, cnt: 1, nodes: [] },                                        // 15 ac bit 3 clear: return
    { b58: 0, arg: 2, d720: 0, d728: 1.0, ac: 0x8, f214: 0x400, cnt: 1, nodes: [] },                                    // 16 bit 10 of f214
    { b58: 0, arg: 2, d720: 0, d728: 1.0, ac: 0, f214: 0x400, cnt: 1, nodes: [] },                                      // 17 bit 10, ac clear
    { b58: 0, arg: 9, d720: 0, d728: 0.0, ac: 0, f214: 0x4, nodes: [] },                                                // 18 E == 0: 0x13
    { b58: 0, arg: 9, d720: 0, d728: 'nan', ac: 0, f214: 0x4, nodes: [] },                                              // 19 E unordered: 0x13
    { b58: 2, f5c: 4, rate: 3, d720: 0, d728: 4.0, ac: 0, f214: 0x4, nodes: [N(0x0e, 0, 0x77, 2, 2), N(0x0a, 0, 0x78, 1, 1)],
      h139: 3, r2068: 0 },                                                                                              // 20 timed, nodes 0x0e / 0x0a
    { b58: 0, arg: 5, d720: 1e9, d728: 1.5, ac: 0, f214: 0x4, h139: 1, r2068: 0,
      nodes: [N(0x08, 0x02, 0x9, 1, 1), N(0x08, 0, 0x9, 2, 2), N(0x0c, 0, 0x9, 3, 3)] },                               // 21 node flag +0x34 bit 1
    { b58: 0, arg: 5, d720: 0, d728: 1.5, ac: 0, f214: 0, nodes: [N(0x0a, 0, 0x9, 1, 1)] },                           // 22 f38 bit 1 without bit 2
    { b58: 0, arg: 5, d720: 1e9, d728: 1.5, ac: 0, f214: 0, h139: 0x7fffffff, r2068: 0,
      nodes: [N(0x0e, 0, 0x9, 0x7fffffff, 1)] },                                                                        // 23 wrapping sum
    { b58: 2, f5c: 0xffffffff, rate: 0xffffffff, d720: 1e30, d728: 1.0, ac: 0, f214: 0x0, nodes: [] },                  // 24 large product
    { b58: 0, arg: 0xffffffff, d720: -5.0, d728: 3.0, ac: 0, f214: 0x0, nodes: [] },                                    // 25 ticks wrap
    { b58: 0, arg: 5, d720: 1e9, d728: 1.5, ac: 0, f214: 0, h139: 1, r2068: 0, poke34: 1,
      nodes: [N(0x08, 0, 0x9, 1, 1)] },                                                                                 // 26 playEvent sets +0x34 bit 1
    { b58: 0, arg: 2, d720: 0, d728: 1.0, ac: 0x208, f214: 0, cnt: 1, nodes: [] },                                      // 27 like 13 (second voice of that arm)
    { b58: 0, arg: 3, d720: 0, d728: 1.0, ac: 0, f214: 0, nodes: [N(0x00, 0, 0x9, 1, 1), N(0x08, 0, 0, 0, 0)] },       // 28 a node with bit 3 clear
    { b58: 2, f5c: 3, rate: 10, d720: 1000, d728: 1.0, ac: 0, f214: 0x220, cnt: 1, n23c: 1, n240: 0, ix: [],
      nodes: [] },                                                                                                      // 29 list count 0
    { b58: 2, f5c: 3, rate: 10, d720: 1000, d728: 1.0, ac: 0, f214: 0x220, cnt: 1, n23c: 1, n240: 1, ix: [0],
      nodes: [], res: [0] },                                                                                            // 30 NULL node, result 0
    { b58: 2, f5c: 3, rate: 10, d720: 30, d728: 1.0, ac: 0, f214: 0x200, cnt: 1, nodes: [N(0x08, 0, 0x9, 1, 1)] },   // 31 X equals [+0x720]
    { b58: 0, arg: 1, d720: 100, d728: 2.0, ac: 0, f214: 0x800, cnt: 1, h139: 7, r2068: 0,
      nodes: [N(0x08, 0, 0x1234, 3, 4)] },                                                                              // 32 found with bit 11
    { b58: 0, arg: 1, d720: 100, d728: 2.0, ac: 0, f214: 0, cnt: 1, h139: 7, r2068: 0, pokeAc: 1,
      nodes: [N(0x08, 0, 0x1234, 3, 4)] },                                                                              // 33 playEvent sets bit 3 of +0xac
  ];

  function build(variant) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    heap(out, 0x100);
    const vt = recVtable(out.log, { 0x134: 0, 0xb4: 0, 0xb8: 0, 0x20: 0, 0x14: 0 });
    out.nd = block(0, 0x5e000000, 0x60 * 8 * VU.length);
    out.ix = block(0, 0x1d000000, 0x20 * VU.length);
    const objs = [];
    VU.forEach(function (r, i) {
      const o = block(i, 0x21c00000, 0x800);
      o.writePointer(vt);
      fill(o.add(0x100), 0x200, 0);
      u32(o, 0x100 + 0x134, r.rate || 0);
      u32(o, 0x100 + 0xb4, r.cnt || 0);
      o.add(0x58).writeU8(r.b58 || 0);
      u32(o, 0x5c, r.f5c || 0);
      u32(o, 0xac, r.ac || 0);
      u32(o, 0x214, r.f214 || 0);
      u32(o, 0x23c, r.n23c || 0);
      u32(o, 0x240, r.n240 !== undefined ? r.n240 : 0);
      u32(o, 0x238, out.ix.add(0x20 * i));
      (r.ix || []).forEach(function (v, k) { u32(out.ix, 0x20 * i + 4 * k, v); });
      u32(o, 0x1f0, r.r2068 || 0);
      u32(o, 0x1f4, r.poke34 || 0);
      u32(o, 0x1f8, r.pokeAc || 0);
      u32(o, 0x71c, 0x71c0 + i);
      u32(o, 0x724, 0x7240 + i);
      dbl(o, 0x720, r.d720 || 0);
      if (r.d728 === 'nan') dblBytes(o, 0x728, NAN_SIG); else dbl(o, 0x728, r.d728);
      const nodes = r.nodes || [];
      for (let k = 0; k < 16; k++) {
        if (k < nodes.length) {
          const n = out.nd.add(0x60 * (8 * i + k));
          n.add(0x38).writeU8(nodes[k].f38);
          n.add(0x34).writeU8(nodes[k].f34);
          u32(n, 0x2c, nodes[k].p2c ? (((r.h139 || 0) + 0x1000) >>> 0) : 0);
          u32(n, 0x4c, nodes[k].i4c);
          u32(n, 0x50, nodes[k].i50);
          u32(o, 0x300 + 4 * k, n);
        } else {
          u32(o, 0x300 + 4 * k, 0);
        }
        u32(o, 0x340 + 4 * k, (r.res && r.res[k] !== undefined) ? r.res[k] : (k < nodes.length ? 0 : 0x24));
      }
      out['o' + i] = o;
      objs.push(o);
    });
    // the registry list: variant 'a' every voice once in row order with a stranger between (middle nodes); 'b' / 'e' / 'f' the
    // voices of the three unlink sites at the head and the tail (rows 10 / 11, 13 / 27, 14 / 16) with the others between; 'c' no
    // voice at all; 'd' / 'g' / 'h' one voice only (a node that is head and tail: rows 10, 13, 14); 'z' an empty list
    const members = [];
    const stranger = block(0, 0x30000000, 0x40);
    const ends = { b: [10, 11], e: [13, 27], f: [14, 16] }, single = { d: 10, g: 13, h: 14 };
    if (variant === 'a') objs.forEach(function (o) { members.push(stranger); members.push(o); });
    if (ends[variant]) {
      members.push(objs[ends[variant][0]]);
      objs.forEach(function (o, i) { if (i !== ends[variant][0] && i !== ends[variant][1]) members.push(o); });
      members.push(objs[ends[variant][1]]);
    }
    if (variant === 'c') { members.push(stranger); members.push(stranger); }
    if (single[variant] !== undefined) members.push(objs[single[variant]]);
    out.tn = fill(keep(Memory.alloc(0x10 * (members.length + 1))), 0x10 * (members.length + 1), 0x4b);
    const tns = members.map(function (x, j) { return out.tn.add(0x10 * j); });
    tns.forEach(function (n, j) {
      u32(n, 0, j ? tns[j - 1] : 0);
      u32(n, 4, j + 1 < tns.length ? tns[j + 1] : 0);
      u32(n, 8, members[j]);
    });
    swapGlobal(0xb5000, tns.length ? tns[0] : 0, out, 'gth');
    swapGlobal(0xb5004, tns.length ? tns[tns.length - 1] : 0, out, 'gtt');
    swapGlobal(0xb5008, 0x58585858, out, 'gtc');
    swapGlobal(0xb500c, 0x100 + tns.length, out, 'gtn');
    // Seq_listGetAt's thunk: the node of index i (k >= 16 ends with 0x24; the out slot is the caller's)
    stubOn(out.log, 0x15fa, 2, true, function (a) {
      const self = a[0].sub(0x60), i = a[2].toUInt32();
      if (i >= 16) return 0x24;
      a[1].writePointer(self.add(0x300 + 4 * i).readPointer());
      return self.add(0x340 + 4 * i).readU32();
    }, [1]);
    // the helpers (answers live in the fixture's data): 0x139d (cdecl, the node's +0x2c and an out slot) answers +0x2c - 0x1000
    // and writes 0x1390 to the slot; 0x1c7b (thiscall node, one argument) and 0x1956 (thiscall, one argument) answer a constant;
    // 0x2068 (thiscall voice, node, out slot) answers the dword at voice + 0x1f0 and writes 0x2060 to the slot
    stubOn(out.log, 0x139d, 2, 'cdecl', function (a) { a[1].writeU32(0x1390); return (a[0].toUInt32() - 0x1000) >>> 0; }, [1]);
    stubOn(out.log, 0x1c7b, 1, true, function () { return 0x1c7b; });
    stubOn(out.log, 0x2068, 2, true, function (a) {
      a[2].writeU32(0x2060);
      if (a[0].add(0x1f4).readU32()) a[1].add(0x34).writeU8(a[1].add(0x34).readU8() | 2);   // the helper marks the node
      if (a[0].add(0x1f8).readU32()) a[0].add(0xac).writeU32(a[0].add(0xac).readU32() | 8);  // or the voice
      return a[0].add(0x1f0).readU32();
    }, [2]);
    stubOn(out.log, 0x1956, 1, true, function () { return 0x1956; });
    return out;
  }
  ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'z'].forEach(function (v) { F['s46_vu_' + v] = function () { return build(v); }; });
})();
