// Fixtures for C3 batch c3fk (sound.dll, round 34). Layouts come from the disassembly cited in shim/src/re/c3fk.cpp. Every
// object, vtable, list node and log here is allocated by the fixture; no live sound object, device, list, window or file is
// handed to an arm. Helpers copied from re/frida/js/fixtures.d/c3fd.js (freeze guard, swapped module global, recording stub on
// an incremental-link thunk that only the A/B thread takes, recording vtables and objects) and the quiet-timer method of
// re/frida/js/fixtures.d/c3fe.js (the multimedia timer is held off only at an idle moment). Module globals a fixture writes are
// saved first and restored by re/frida/js/freeze.js before the game's first thread is resumed. All values are synthetic.
//
// Recording objects: a block whose vtable slots are thiscall NativeCallbacks that log {0x5100 + slot, this, arguments} and
// return the dword at this + 0x100 + slot (the row's answer for that slot, inside the compared object). A block may carry a
// poke at +0x1e0..+0x1ef (target pointer, trigger tag, value, target offset): when the callback named by the trigger runs on
// that block, the value is written to target + offset.
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const keep = function (x) { globalThis.__diffKeepAlive.push(x); return x; };
  const snd = function (rva) { return Process.getModuleByName('sound.dll').base.add(rva); };
  function fill(p, n, b) { p.writeByteArray(new Array(n).fill(b)); return p; }
  function marker(p, n, tag) { for (let k = 0; k + 4 <= n; k += 4) p.add(k).writeU32((tag + k) >>> 0); }
  function u32(p, off, v) {
    if (v instanceof NativePointer) p.add(off).writePointer(v); else p.add(off).writeU32(v >>> 0);
  }
  function block(i, tag, n) {
    const sz = n || 0x200;
    const b = fill(keep(Memory.alloc(sz)), sz, 0x3b);
    marker(b, sz, (tag >>> 0) + 0x10000 * i);
    return b;
  }
  function cstr(s) { const p = keep(Memory.alloc(s.length + 8)); fill(p, s.length + 8, 0x5f); p.writeUtf8String(s); return p; }
  const k32 = Process.getModuleByName('kernel32.dll');
  const sleep = keep(new NativeFunction(k32.getExportByName('Sleep'), 'void', ['uint32'], 'stdcall', { scheduling: 'exclusive' }));

  // ---- freezing and the multimedia timer ------------------------------------------------------------------------
  let frozenNow = false;
  function guard() {
    if (frozenNow) return;
    __freeze.thread();
    frozenNow = true;
    __freeze.atFlush(function () { frozenNow = false; });
    __freeze.negOn();
  }
  // c3fe's method: wait for an idle moment (re-entrancy word 0x100b49a8 = 0 and the timer's critical section 0x100b4990
  // unowned), write 5 (SoundTimer::onTick then leaves at 0x10008edf before touching anything), wait until the word has stayed
  // 5 with the section unowned for 100 ms, and put the idle value back at the flush.
  function quietTimer(out) {
    guard();
    const g = snd(0xb49a8), owner = snd(0xb4990 + 0xc);   // CRITICAL_SECTION.OwningThread
    let idle = null;
    for (let i = 0; i < 400; i++) {
      if (g.readU32() === 0 && owner.readU32() === 0) { idle = 0; break; }
      sleep(1);
    }
    if (idle === null) {
      if (owner.readU32() !== 0) throw new Error('c3fk: timer section 0x100b4990 stays owned');
      idle = g.readU32();
    }
    g.writeU32(5);
    __freeze.atFlush(function () { g.writeU32(idle); });
    let stable = 0;
    for (let i = 0; i < 1000 && stable < 50; i++) {
      sleep(2);
      if (g.readU32() !== 5) { g.writeU32(5); stable = 0; continue; }
      if (owner.readU32() !== 0) { stable = 0; continue; }
      stable++;
    }
    if (stable < 50) throw new Error('c3fk: timer did not go quiet');
    out.g49a8 = g;
  }
  function swapGlobal(rva, value, out, name) {
    const g = snd(rva);
    if (name) out[name] = g;
    __freeze.setU32(g, (value instanceof NativePointer) ? value.toUInt32() : value);
    return g;
  }

  // ---- call log and recording stub (A/B thread only) ------------------------------------------------------------
  const LOGSZ = 0x820;                       // count at +0, then 0x20-byte entries (65 of them)
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
  function poke(self, tag) {
    const t = self.add(0x1e0).readPointer();
    if (t.isNull()) return;
    const trig = self.add(0x1e4).readU32();
    if (trig !== (tag >>> 0) && trig !== 0xffffffff) return;
    t.add(self.add(0x1ec).readU32()).writeU32(self.add(0x1e8).readU32());
  }
  let stubLive = {};
  // mask: indices of arguments (0 = ecx for thiscall) that are the caller's stack slots, logged as 0x5ac0 (the two arms'
  // frames differ)
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
    const vt = fill(keep(Memory.alloc(0x100)), 0x100, 0);
    Object.keys(slots).forEach(function (k) {
      const off = parseInt(k, 10), n = slots[k];
      const argt = ['pointer'].concat(Array(n).fill('uint32'));
      vt.add(off).writePointer(keep(new NativeCallback(function () {
        const a = Array.prototype.slice.call(arguments);
        logCall(log, 0x5100 + off, a);
        poke(a[0], 0x5100 + off);
        return a[0].add(0x100 + off).readU32();
      }, 'uint32', argt, 'thiscall')));
    });
    return vt;
  }
  function recObj(i, tag, vt, cfg, size) {
    const b = block(i, tag, size);
    if (vt) {
      b.writePointer(vt);
      fill(b.add(0x100), 0xf0, 0);   // unset slot answers and pokes are 0
    }
    Object.keys(cfg || {}).forEach(function (k) { u32(b, parseInt(k, 10), cfg[k]); });
    return b;
  }
  function pokeCfg(target, trig, value, off) { const c = {}; c[0x1e0] = target; c[0x1e4] = trig; c[0x1e8] = value; c[0x1ec] = off; return c; }
  function heap(out, size, opts) {
    const h = __heapStandIn(Object.assign({ module: 'sound.dll', alloc: [0x424d7], free: [0x4249a], arenaSize: size || 0x400 },
                                          opts || {}));
    out.arena = h.arena;
    out.freelog = h.log;
    return h;
  }
  globalThis.__c3fk = { fill: fill, marker: marker, u32: u32, block: block, cstr: cstr, guard: guard, quietTimer: quietTimer,
                        swapGlobal: swapGlobal, stubLog: stubLog, logCall: logCall, stubOn: stubOn, recVtable: recVtable,
                        recObj: recObj, pokeCfg: pokeCfg, heap: heap, snd: snd, keep: keep };

  // ===================================================================================================================
  // A. The callees round 33 named and the base initialiser 0x1002e3f0
  // ===================================================================================================================
  // getField204: twelve 0x210-byte marker blocks with a per-row value at +0x204 (zero, sign bit, all ones, small, large).
  const F204 = [0, 1, 0x80000000, 0xffffffff, 0x204, 0x7fffffff, 0x12345678, 2, 0xfffffffe, 0x100, 0xdeadbeef, 0x55aa55aa];
  F.c3fk_f204 = function () {
    const out = {};
    guard();
    F204.forEach(function (v, i) { const o = block(i, 0x20400000, 0x210); u32(o, 0x204, v); out['o' + i] = o; });
    return out;
  };

  // ChanRecord_init / NodeB000_initBase / NodeB000_ctor: twelve 0x40-byte marker blocks (every write lands inside, the
  // untouched words keep their markers).
  F.c3fk_blocks40 = function () {
    const out = {};
    guard();
    for (let i = 0; i < 12; i++) out['o' + i] = block(i, 0x40400000, 0x40);
    return out;
  };

  // ChanRecord_freeBuffers: +0x1c / +0x24 NULL or a fixture pointer per row; operator delete records (heapstandin.js).
  const CRF = [[0, 0], [1, 0], [0, 1], [1, 1], [1, 1], [0, 0], [1, 0], [0, 1], [1, 1], [1, 0], [0, 1], [1, 1]];
  F.c3fk_crfree = function () {
    const out = {};
    quietTimer(out);
    heap(out, 0x100);
    CRF.forEach(function (r, i) {
      const o = block(i, 0x1cd00000, 0x40);
      u32(o, 0x1c, r[0] ? (0x1c000000 + 0x100 * i) : 0);
      u32(o, 0x24, r[1] ? (0x24000000 + 0x100 * i) : 0);
      out['o' + i] = o;
    });
    return out;
  };

  // StrPtr_assign: objects whose *this is NULL or an old pointer; strings of length 0, 1, 2, 3, 4, 5, 7, 8, 9, 31 and NULL;
  // operator new carved from the arena (state), operator delete records.
  const SPA = [[0, 'a'], [1, 'ab'], [1, ''], [0, 'abc'], [1, 'abcd'], [1, null], [0, null], [1, 'abcdefg'],
               [0, 'abcdefgh'], [1, 'abcdefghi'], [1, 'abcdefghijklmnopqrstuvwxyz01234'], [0, 'xyzzy']];
  F.c3fk_strassign = function () {
    const out = {};
    quietTimer(out);
    heap(out, 0x200);
    SPA.forEach(function (r, i) {
      const o = block(i, 0x1b100000, 0x10);
      u32(o, 0, r[0] ? (0x51000000 + 0x100 * i) : 0);
      out['o' + i] = o;
      out['s' + i] = r[1] === null ? ptr(0) : cstr(r[1]);
    });
    return out;
  };

  // ChannelTable_callSlot4: tables of 0x220 bytes (slots +4 .. +0x200, the rest marker words), slot objects are recording
  // objects (vtable slot +4, one argument) of 0x1500 bytes with a member count at +0x1438 and members at +0x1440 + 4j (recording
  // objects too). Row specs: list of [slot index, object id]; objects: id -> member count (+0x1438) and optional poke.
  // Rows: empty; one object; the same object in consecutive slots (called once); A NULL A (called once, eax keeps A);
  // A B A (A twice); count 0 / 1 / 2 / 5 / -1 (signed `jle`); a slot call that replaces its own slot (the reload at
  // 0x1001ae7c reads the new object), a member call that replaces the slot (re-read at 0x1001ae9b), slots 0 and 0x7f.
  const CS4 = [
    { s: [], o: {} },
    { s: [[0, 0]], o: { 0: { n: 0 } } },
    { s: [[3, 0], [4, 0], [5, 0]], o: { 0: { n: 1 } } },
    { s: [[1, 0], [3, 0]], o: { 0: { n: 2 } } },
    { s: [[0, 0], [1, 1], [2, 0]], o: { 0: { n: 3 }, 1: { n: 0 } } },
    { s: [[7, 0], [0x7f, 1]], o: { 0: { n: 5 }, 1: { n: 2 } } },
    { s: [[2, 0], [9, 1]], o: { 0: { n: -1 }, 1: { n: 0x80000000 + 1 } } },
    { s: [[5, 0], [6, 1]], o: { 0: { n: 2, pokeSlot: [5, 2] }, 1: { n: 1 }, 2: { n: 3 } } },
    { s: [[10, 0]], o: { 0: { n: 4, memberPoke: [10, 1] }, 1: { n: 2 } } },
    { s: [[0x7e, 0], [0x7f, 0]], o: { 0: { n: 2 } } },
    { s: [[0, 0], [1, 1], [2, 2], [3, 3]], o: { 0: { n: 1 }, 1: { n: 2 }, 2: { n: 1 }, 3: { n: 3 } } },
    { s: [[20, 0], [21, 1], [22, 0], [40, 1]], o: { 0: { n: 2 }, 1: { n: 1 } } },
  ];
  F.c3fk_callslot4 = function () {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    const vt = recVtable(out.log, { 4: 1 });
    CS4.forEach(function (r, i) {
      const t = block(i, 0x1ae00000, 0x220);
      for (let k = 0; k < 0x80; k++) u32(t, 4 + 4 * k, 0);
      const objs = {};
      Object.keys(r.o).forEach(function (id) {
        const spec = r.o[id];
        const cfg = {};
        cfg[0x104] = 0x40000 + 0x100 * i + parseInt(id, 10);
        cfg[0x1438] = spec.n >>> 0;
        const o = recObj(parseInt(id, 10), 0x1a000000 + 0x100000 * i, vt, cfg, 0x1500);
        out['g' + i + '_' + id] = o;
        objs[id] = o;
      });
      Object.keys(r.o).forEach(function (id) {
        const spec = r.o[id], o = objs[id];
        const nm = Math.max(0, Math.min(8, (spec.n | 0) - 1));
        for (let j = 0; j < nm; j++) {
          const m = recObj(j, 0x1b000000 + 0x100000 * i + 0x1000 * parseInt(id, 10), vt, { 0x104: 0x50000 + j }, 0x200);
          out['m' + i + '_' + id + '_' + j] = m;
          u32(o, 0x1440 + 4 * j, m);
          if (spec.memberPoke && j === 0) {
            const c = pokeCfg(t, 0x5104, objs[spec.memberPoke[1]], 4 + 4 * spec.memberPoke[0]);
            Object.keys(c).forEach(function (k) { u32(m, parseInt(k, 10), c[k]); });
          }
        }
        if (spec.pokeSlot) {
          const c = pokeCfg(t, 0x5104, objs[spec.pokeSlot[1]], 4 + 4 * spec.pokeSlot[0]);
          Object.keys(c).forEach(function (k) { u32(o, parseInt(k, 10), c[k]); });
        }
      });
      r.s.forEach(function (e) { u32(t, 4 + 4 * e[0], objs[e[1]]); });
      out['t' + i] = t;
    });
    return out;
  };

  // Channel_ctorB: twelve 0x1400-byte marker blocks. The element constructor thunk 0x10001e56 (Voice_resetDefaults) is a
  // recording stub that marks its element (+0x10 = 0x5e5e5e5e) and returns it; the ??_L helper 0x10042902 runs for real. The
  // Channel_recomputeVoiceGains thunk 0x10001efb is a recording stub that also logs the fields the body writes before
  // (+0x1d8, +0x1c0, +0x1e4) and after (+0x220, +0x138c, +0x300) the call, so the order of the stores is compared.
  F.c3fk_chctorb = function () {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    for (let i = 0; i < 12; i++) out['o' + i] = block(i, 0x31b00000, 0x1400);
    stubOn(out.log, 0x1e56, 0, true, function (a) { a[0].add(0x10).writeU32(0x5e5e5e5e); return a[0]; });
    stubOn(out.log, 0x1efb, 1, true, function (a) {
      const o = a[0];
      logCall(out.log, 0x1efb0000, [o.add(0x1d8).readU32(), o.add(0x1c0).readU32(), o.add(0x1e4).readU32(),
                                    o.add(0x220).readU32(), o.add(0x138c).readU32(), o.add(0x300).readU32()]);
      return 0x1efb;
    });
    return out;
  };

  // ===================================================================================================================
  // B. Sequencer channel records, destructors, track builders
  // ===================================================================================================================
  // The shared channel-object list 0x100b4fe8 (head) / 0x100b4fec (tail) / 0x100b4ff0 (cursor) / 0x100b4ff4 (count), swapped
  // for the key: nodes {prev, next, payload} (0x10 bytes), payloads of 0x220 bytes with the name pointer at +0 and the use
  // count at +0x210. spec: [[name or null for a NULL payload, use count], ...]; out.p<k> / out.n<k>.
  function descList(out, spec, tag) {
    const nodes = spec.map(function (e, k) { return block(k, (tag || 0x4fe00000) + 0x1000, 0x10); });
    spec.forEach(function (e, k) {
      const n = nodes[k];
      u32(n, 0, k ? nodes[k - 1] : 0);
      u32(n, 4, k + 1 < nodes.length ? nodes[k + 1] : 0);
      if (e[0] === null) {
        u32(n, 8, 0);
      } else {
        const p = block(k, (tag || 0x4fe00000) + 0x2000, 0x220);
        u32(p, 0, cstr(e[0]));
        u32(p, 0x210, e[1]);
        u32(n, 8, p);
        out['p' + k] = p;
      }
      out['n' + k] = n;
    });
    swapGlobal(0xb4fe8, nodes.length ? nodes[0] : 0, out, 'gdh');
    swapGlobal(0xb4fec, nodes.length ? nodes[nodes.length - 1] : 0, out, 'gdt');
    swapGlobal(0xb4ff0, 0x4ff04ff0, out, 'gdc');
    swapGlobal(0xb4ff4, nodes.length + 0x40, out, 'gdn');
    return nodes;
  }
  // an old channel record (0x40 bytes) whose two buffers ChanRecord_freeBuffers hands to operator delete
  function oldRec(i, tag) {
    const r = block(i, tag, 0x40);
    u32(r, 0x1c, 0x1c1c0000 + 0x10 * i);
    u32(r, 0x24, (i & 1) ? 0 : (0x24240000 + 0x10 * i));
    return r;
  }

  // Seq_setupChannel(desc, flag): twelve 0xa00-byte sequencers, desc blocks of 0x40 bytes {+0 channel, +0x1c name, +0x24 alt name,
  // +0x3c the answer Seq_loadChannelDesc's stub returns}. 0x100b49c0 is swapped. ChanRecord_init (thunk 0x100014bf) and
  // ChanRecord_freeBuffers (0x10002004) run for real (both are c3fk hooks; the callees-off pass switches them off);
  // Sound_setAltName 0x10001604, Seq_mergeChannelDesc 0x10001d52, Sound_setName 0x100010f5 and Seq_loadChannelDesc 0x100015d2
  // are recording stubs. Rows: {ch, name, alt, old record in the slot, flag (low byte tested), load answer}.
  const SU = [
    { ch: 0x80, name: 1, alt: null, old: 0, flag: 0, ans: 0 },
    { ch: 0xffffffff, name: 1, alt: null, old: 0, flag: 1, ans: 0 },
    { ch: 5, name: 0, alt: 'beta', old: 1, flag: 1, ans: 0 },
    { ch: 5, name: 1, alt: null, old: 0, flag: 0, ans: 9 },
    { ch: 0x7f, name: 1, alt: null, old: 1, flag: 1, ans: 0 },
    { ch: 3, name: 1, alt: null, old: 0, flag: 1, ans: 7 },
    { ch: 3, name: 1, alt: null, old: 1, flag: 0x100, ans: 7 },
    { ch: 1, name: 1, alt: 'beta', old: 0, flag: 0, ans: 0 },
    { ch: 2, name: 1, alt: 'delta', old: 1, flag: 1, ans: 5 },
    { ch: 0, name: 1, alt: 'alpha', old: 1, flag: 0, ans: 0 },
    { ch: 4, name: 1, alt: '', old: 0, flag: 1, ans: 3 },
    { ch: 6, name: 1, alt: 'gamma', old: 1, flag: 1, ans: 0 },
  ];
  function setupCh(variant) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    heap(out, 0x200, variant === 'oom' ? { failMask: 0xffffffff } : {});
    swapGlobal(0xb49c0, 0x49c0abcd, out, 'gdef');
    // `tail`: no NULL payload and no `delta`, so row 8's walk ends on a NULL cursor (0x100210fd taken)
    if (variant === 'empty') descList(out, []);
    else if (variant === 'tail') descList(out, [['alpha', 1], ['beta', 3], ['', 2], ['gamma', 0x7fffffff]]);
    else descList(out, [['alpha', 1], ['beta', 3], ['', 2], ['gamma', 0x7fffffff], [null, 0], ['delta', 1]]);
    SU.forEach(function (r, i) {
      const q = block(i, 0x20f00000, 0xa00);
      for (let k = 0; k < 0x80; k++) u32(q, 0x744 + 4 * k, 0);
      if (r.old && (r.ch >>> 0) <= 0x7f) { const o = oldRec(i, 0x0ed00000); u32(q, 0x744 + 4 * r.ch, o); out['old' + i] = o; }
      else out['old' + i] = block(i, 0x0ee00000, 0x40);
      const d = block(i, 0xde500000, 0x40);
      u32(d, 0, r.ch);
      u32(d, 0x1c, r.name ? cstr('name' + i) : 0);
      u32(d, 0x24, r.alt === null ? 0 : cstr(r.alt));
      u32(d, 0x3c, r.ans);
      out['q' + i] = q;
      out['d' + i] = d;
    });
    stubOn(out.log, 0x1604, 1, true, function () { return 0x1604; });
    stubOn(out.log, 0x1d52, 1, true, function () { return 0x1d52; });
    stubOn(out.log, 0x10f5, 1, true, function () { return 0x10f5; });
    stubOn(out.log, 0x15d2, 1, true, function (a) { return a[1].add(0x3c).readU32(); });
    return out;
  }
  F.c3fk_setup = function () { return setupCh(''); };
  F.c3fk_setup_empty = function () { return setupCh('empty'); };
  F.c3fk_setup_oom = function () { return setupCh('oom'); };
  F.c3fk_setup_tail = function () { return setupCh('tail'); };

  // Seq_updateChannel(desc): sequencers whose slot desc[0] holds a channel record (0x40 bytes, +0x30 the shared object) or NULL.
  // The shared list (`main`): alpha (use 1, head), beta (5), gamma (1, middle), delta (2), omega (1, tail); `hole` puts a NULL
  // payload in the middle (the walk stops there, 0x1002208e), `nolist` leaves the head NULL. zeta is a shared object outside the
  // list. Seq_mergeChannelDesc 0x10001d52, Sound_setAltName 0x10001604, Seq_freeChannelObjects 0x10001d66 and Seq_setupChannel
  // 0x1000118b are recording stubs. Rows: {slot used, rec+0x30 (list index, 'z' = zeta, -1 = NULL), alt name}.
  const UP = [
    { rec: 1, cur: -1, alt: 'beta', nul: 1 },
    { rec: 0, cur: -1, alt: 'beta' },
    { rec: 1, cur: 1, alt: null },
    { rec: 1, cur: 0, alt: 'alpha' },
    { rec: 1, cur: -1, alt: 'beta' },
    { rec: 1, cur: 3, alt: 'beta' },
    { rec: 1, cur: 2, alt: 'beta' },
    { rec: 1, cur: 0, alt: 'gamma' },
    { rec: 1, cur: 4, alt: 'delta' },
    { rec: 1, cur: 'z', alt: 'beta' },
    { rec: 1, cur: -1, alt: 'nomatch' },
    { rec: 1, cur: 1, alt: 'omega' },
  ];
  function updateCh(variant) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    heap(out, 0x100);
    let spec = [['alpha', 1], ['beta', 5], ['gamma', 1], ['delta', 2], ['omega', 1]];
    if (variant === 'hole') spec = [['alpha', 1], ['beta', 5], [null, 0], ['delta', 2], ['omega', 1]];
    if (variant === 'nolist') spec = [];
    descList(out, spec);
    const z = block(0, 0x2e7a0000, 0x220);
    u32(z, 0, cstr('zeta'));
    u32(z, 0x210, 1);
    out.z = z;
    UP.forEach(function (r, i) {
      const q = block(i, 0x21f00000, 0xa00);
      const ch = (i * 7 + 3) & 0x7f;
      const rec = block(i, 0x2ec00000, 0x40);
      let cur = 0;
      if (r.cur === 'z') cur = z;
      else if (r.cur !== -1 && out['p' + r.cur]) cur = out['p' + r.cur];
      u32(rec, 0x30, cur);
      u32(q, 0x744 + 4 * ch, r.rec ? rec : 0);
      const d = block(i, 0xde600000, 0x40);
      u32(d, 0, ch);
      u32(d, 0x24, r.alt === null ? 0 : cstr(r.alt));
      out['q' + i] = q;
      out['r' + i] = rec;
      out['d' + i] = r.nul ? ptr(0) : d;
    });
    stubOn(out.log, 0x1d52, 1, true, function () { return 0x1d52; });
    stubOn(out.log, 0x1604, 1, true, function () { return 0x1604; });
    stubOn(out.log, 0x1d66, 0, true, function () { return 0x1d66; });
    stubOn(out.log, 0x118b, 2, true, function () { return 0x118b; });
    return out;
  }
  F.c3fk_update = function () { return updateCh(''); };
  F.c3fk_update_hole = function () { return updateCh('hole'); };
  F.c3fk_update_nolist = function () { return updateCh('nolist'); };

  // a fixture node list {prev, next, payload} of payload ids; header at obj + off = {head, tail, cursor (marker), count}.
  // payload id null = a NULL payload. Returns the nodes.
  function nodeList(out, obj, off, ids, payloads, name, count) {
    const nodes = ids.map(function (id, k) { return block(k, 0x0a0e0000 + off, 0x10); });
    ids.forEach(function (id, k) {
      u32(nodes[k], 0, k ? nodes[k - 1] : 0);
      u32(nodes[k], 4, k + 1 < nodes.length ? nodes[k + 1] : 0);
      u32(nodes[k], 8, id === null ? 0 : payloads[id]);
      out[name + k] = nodes[k];
    });
    u32(obj, off, nodes.length ? nodes[0] : 0);
    u32(obj, off + 4, nodes.length ? nodes[nodes.length - 1] : 0);
    u32(obj, off + 8, 0xc0c0c0c0);
    u32(obj, off + 0xc, count === undefined ? nodes.length : count);
    return nodes;
  }

  // Sound5b6a8_dtor: 0xa00-byte objects. +0xac bit 0 (Midi_Device::closeFile through 0x10001014, a recording stub), channel
  // records in some of the 0x7f slots +0x744.. (ChanRecord_freeBuffers runs for real), list A +0x218 (nodes and payloads
  // freed), the 16 x 0x4c array at +0x250 (??_M 0x1004280e runs for real; the element destructor thunk 0x1000166d records),
  // list B +0x228 (nodes freed). Seq_dtor 0x100014d8 and Ch5be88_dtorA 0x10001cc1 are recording stubs; operator delete records.
  const D6A8 = [
    { f: 0, s: [], a: [], b: [] },
    { f: 1, s: [0], a: [1], b: [1] },
    { f: 0xfe, s: [0x7e], a: [1, 1], b: [] },
    { f: 0xff, s: [3, 4, 0x40], a: [1, 0, 1], b: [1, 1] },
    { f: 0, s: [], a: [0, 1, 1, 1], b: [1, 0, 1] },
    { f: 1, s: [1, 2, 3, 4, 5, 6], a: [1, 1, 1, 1], b: [0] },
    { f: 0x80, s: [0x7f], a: [], b: [1] },
    { f: 0, s: [0, 0x7e], a: [1, 1, 0], b: [1, 1, 1] },
    { f: 3, s: [10], a: [0], b: [0, 1] },
    { f: 0, s: [0x20, 0x21], a: [1, 0, 0, 1], b: [] },
    { f: 1, s: [], a: [1], b: [1, 1, 1, 1] },
    { f: 2, s: [0x7d], a: [1, 1, 1], b: [1, 0] },
  ];
  F.c3fk_d6a8 = function () {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    heap(out, 0x100);
    D6A8.forEach(function (r, i) {
      const o = block(i, 0x1ff00000, 0xa00);
      o.add(0xac).writeU8(r.f);
      for (let k = 0; k < 0x80; k++) u32(o, 0x744 + 4 * k, 0);
      r.s.forEach(function (k, j) { const rec = oldRec(j, 0x0ec00000 + 0x10000 * i); u32(o, 0x744 + 4 * k, rec); out['c' + i + '_' + j] = rec; });
      const pa = r.a.map(function (h, k) { return h ? (0x9a000000 + 0x100 * i + k) : null; });
      nodeList(out, o, 0x218, pa.map(function (v, k) { return v === null ? null : k; }), pa, 'na' + i + '_', r.a.length + 0x10);
      const pb = r.b.map(function (h, k) { return h ? (0x9b000000 + 0x100 * i + k) : null; });
      nodeList(out, o, 0x228, pb.map(function (v, k) { return v === null ? null : k; }), pb, 'nb' + i + '_', r.b.length + 0x20);
      out['o' + i] = o;
    });
    stubOn(out.log, 0x1014, 0, true, function () { return 0x1014; });
    stubOn(out.log, 0x166d, 0, true, function () { return 0x166d; });
    stubOn(out.log, 0x14d8, 0, true, function () { return 0x14d8; });
    stubOn(out.log, 0x1cc1, 0, true, function () { return 0x1cc1; });
    return out;
  };

  // Sound5c450_dtor: 0x100-byte objects with the buffer list +0x5c / +0x60 / count +0x68; buffers of 0x60 bytes with the mmio
  // handle at +0x44 (0 = none). The open-mmio list 0x100b49c8 / 0x100b49cc / cursor 0x100b49d0 / count 0x100b49d4 and the open
  // count 0x100b4a08 are swapped; its nodes point at buffers of several rows (head, middle, tail) and one unrelated buffer.
  // WINMM!mmioClose is stood in at sound.dll's IAT (calls and arguments compared); Ch5c234_dtor 0x10001cc6 is a recording stub;
  // the thunk 0x10001b45 (VoiceSlot_dtorEmpty, a bare `ret`) runs for real. Rows: buffer specs {h: handle, m: in the mmio list}
  // or 0 for a NULL payload.
  const D450 = [
    [], [{ h: 0x11, m: 1 }], [{ h: 0x21, m: 0 }, 0, 0], [{ h: 0, m: 0 }], [{ h: 0x41, m: 1 }, { h: 0x42, m: 1 }],
    [{ h: 0x51, m: 1 }, 0, { h: 0x53, m: 1 }], [0, { h: 0x62, m: 0 }], [{ h: 0x71, m: 0 }, { h: 0, m: 0 }, { h: 0x73, m: 1 }],
    [{ h: 0x81, m: 1 }, { h: 0x82, m: 0 }, { h: 0x83, m: 1 }, { h: 0x84, m: 1 }],
    [{ h: 0x91, m: 1 }, 0, { h: 0x93, m: 0 }, { h: 0x94, m: 0 }, { h: 0x95, m: 0 }],
    [{ h: 0xa1, m: 0 }, 0, { h: 0xa3, m: 0 }, 0], [{ h: 0xb1, m: 1 }],
  ];
  F.c3fk_d450 = function () {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    heap(out, 0x100);
    const inList = [];
    const stray = block(0, 0x5a5a0000, 0x60);
    D450.forEach(function (r, i) {
      const o = block(i, 0x30100000, 0x100);
      const bufs = r.map(function (b, k) {
        if (!b) return null;
        const p = block(k, 0x30200000 + 0x10000 * i, 0x60);
        u32(p, 0x44, b.h);
        out['b' + i + '_' + k] = p;
        if (b.m) inList.push(p);
        return p;
      });
      nodeList(out, o, 0x5c, bufs.map(function (v, k) { return v === null ? null : k; }), bufs, 'nd' + i + '_', r.length + 0x30);
      out['o' + i] = o;
    });
    // the open-mmio list: the first listed buffer, the stray buffer, then every other listed buffer (row order), so rows
    // unlink the head (0x1003021a taken), middle nodes and the tail (0x10030232 taken)
    const ml = inList.slice(0, 1).concat([stray], inList.slice(1));
    const mn = ml.map(function (p, k) { return block(k, 0x49c80000, 0x10); });
    ml.forEach(function (p, k) {
      u32(mn[k], 0, k ? mn[k - 1] : 0);
      u32(mn[k], 4, k + 1 < mn.length ? mn[k + 1] : 0);
      u32(mn[k], 8, p);
      out['mn' + k] = mn[k];
    });
    out.mcount = mn.length;
    swapGlobal(0xb49c8, mn[0], out, 'gmh');
    swapGlobal(0xb49cc, mn[mn.length - 1], out, 'gmt');
    swapGlobal(0xb49d0, 0x49d049d0, out, 'gmc');
    swapGlobal(0xb49d4, mn.length + 0x100, out, 'gmn');
    swapGlobal(0xb4a08, 0x4a080040, out, 'gopen');
    out.implog = __importStandIn({ module: 'sound.dll', imports: { 'WINMM.dll!mmioClose': { args: 2, ret: 0 } } }).log;
    stubOn(out.log, 0x1cc6, 0, true, function () { return 0x1cc6; });
    return out;
  };

  // Seq_buildTrack9 / Seq_buildTrack14: 0x500-byte sequencers with list A at +0x218 and list B at +0x228 holding the same tracks
  // (0x20-byte payloads, sort key at +4); desc blocks of 0x40 bytes {+4 key, +8 first index, +0xc last index, the rest markers}.
  // NodeB000_ctor (thunk 0x10001410) and the NodeList helpers pushBack 0x100025bd, pushFront 0x10001073, insert 0x10001ef1,
  // initNode 0x1000106e run for real (their nodes come from the arena); Seq_clearTrackFlag8All 0x100019e7 is a recording stub;
  // Seq_listGetAt's thunk 0x100015fa is a stub (Seq_buildTrack9 only): step i < 16 writes the dword at seq+0x300+4i into the out
  // slot unless it is 0xeeeeeeee (no write), returns the dword at seq+0x340+4i; i >= 16 writes nothing and returns 0x24; the out
  // slot is the caller's stack and is logged as 0x5ac0. Tracks the out slot names are 0x460-byte blocks (+0x458 is written).
  // Rows: {keys of the existing tracks (A; B holds the same tracks unless b is given), count A (default = length), key, first,
  // last, the getAt table}. `bad` builds the inconsistent list whose stop node's +0 points at a stray node with +4 NULL.
  const BT = [
    { a: [], key: 5, lo: 1, hi: 0 },
    { a: [7], cnt: 0, key: 5, lo: 0, hi: 0, get: [1] },
    { a: [10], key: 5, lo: 2, hi: 4, get: [1, 2, 0xe] },
    { a: [10], key: 20, lo: 0, hi: 2, get: [0, 0xe, 3] },
    { a: [10, 20, 30], key: 25, lo: 3, hi: 3, get: [1, 1, 1, 2] },
    { a: [10, 20, 30], key: 20, lo: 0, hi: 5, get: [0xe, 1, 0, 0xe, 2, 3] },
    { a: [10, 20], key: 15, lo: 15, hi: 17, get: [] },
    { a: [10], key: 10, lo: 4, hi: 1 },
    { a: [10, 20], key: 0xffffffff, lo: 0, hi: 0, get: [0] },
    { a: [10, null, 30], key: 40, lo: 1, hi: 1, get: [0, 2] },
    { a: [10, 20, 30], b: [10], key: 15, lo: 0, hi: 1, get: [3, 0xe] },
    { a: [], cnt: 1, key: 3, lo: 0, hi: 0, get: [2] },
    { a: [10, 20], bad: 1, key: 15, lo: 0, hi: 0, get: [1] },
  ];
  function buildTrack(nine) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    heap(out, 0x200);
    BT.forEach(function (r, i) {
      const q = block(i, 0x22900000, 0x500);
      const tracks = r.a.map(function (k, j) {
        if (k === null) return null;
        const t = block(j, 0x7a000000 + 0x100000 * i, 0x20);
        u32(t, 4, k);
        out['tk' + i + '_' + j] = t;
        return t;
      });
      const idsA = r.a.map(function (k, j) { return k === null ? null : j; });
      const na = nodeList(out, q, 0x218, idsA, tracks, 'na' + i + '_', r.cnt === undefined ? r.a.length : r.cnt);
      const bk = r.b || r.a;
      nodeList(out, q, 0x228, bk.map(function (k, j) { return k === null ? null : j; }), tracks, 'nb' + i + '_', bk.length);
      if (r.bad) {
        const stray = block(9, 0x5f000000 + 0x10000 * i, 0x10);
        u32(stray, 4, 0);
        u32(na[1], 0, stray);
        out['stray' + i] = stray;
      }
      // the getAt table: 1..3 = a 0x460-byte target track, 0 = write 0, 0xe = no write
      for (let k = 0; k < 16; k++) { u32(q, 0x300 + 4 * k, 0xeeeeeeee); u32(q, 0x340 + 4 * k, (k * 3) & 0x7); }
      (r.get || []).forEach(function (g, k) {
        if (g === 0xe) return;
        if (g === 0) { u32(q, 0x300 + 4 * k, 0); return; }
        const tg = block(g, 0x45800000 + 0x10000 * i, 0x460);
        out['tg' + i + '_' + k] = tg;
        u32(q, 0x300 + 4 * k, tg);
      });
      const d = block(i, 0xde700000, 0x40);
      u32(d, 4, r.key);
      u32(d, 8, r.lo);
      u32(d, 0xc, r.hi);
      out['q' + i] = q;
      out['d' + i] = d;
    });
    if (nine) {
      stubOn(out.log, 0x19e7, 0, true, function () { return 0x19e7; });
      stubOn(out.log, 0x15fa, 2, true, function (a) {
        const self = a[0].sub(0x60), i = a[2].toUInt32();
        if (i >= 16) return 0x24;
        const v = self.add(0x300 + 4 * i).readU32();
        if (v !== 0xeeeeeeee) a[1].writeU32(v);
        return self.add(0x340 + 4 * i).readU32();
      }, [1]);
    }
    return out;
  }
  // Sound_lookupOrCreate(desc): 0xa00-byte sequencers with channel records (0x40 bytes: +0x24 alt name, +0x30 shared object) in
  // some slots; desc blocks of 0x220 bytes {+4 + 4i sub-descriptors, +0x204 name, +0x208 flags, +0x20c / +0x210 range, +0x218
  // value}. The shared list (main): alpha (use 1, head), beta (4), gamma (2), gamma2 (1, middle; its string is `gamma`), delta
  // (1, tail); `hole` puts a NULL payload after beta; `nolist` leaves the head NULL. getField204 (thunk 0x100025f4),
  // Sound_initEntry (0x10001929), StrPtr_assign (0x10001334) and ChannelTable_callSlot4 (0x10002455) run for real (three are
  // c3fk hooks: the callees-off pass switches them off); ChannelTable_loadRange 0x10001fa0 is a stub that stores the sub-
  // descriptor's +0x10 (a recording object or 0) in the entry's table slot sub[0] + 1 when sub[0] < 0x7f, so callSlot4 then
  // calls it; Seq_setPlayRange 0x10001f32 and Seq_freeChannelObjects 0x10001d66 are recording stubs. Rows: {name, flags,
  // sub-descriptors [index, first, has object], records [slot, alt (null = NULL), +0x30 (list index, -1 NULL)]}.
  const LK = [
    { nul: 1 },
    { name: 'beta', recs: [[3, 'beta', -1]] },
    { name: 'alpha', recs: [[0, 'alpha', 0], [1, 'beta', 0]] },
    { name: 'beta', recs: [[5, 'beta', 2], [6, 'beta', 4]] },
    { name: 'beta', recs: [[6, 'beta', 3]] },
    { name: 'newname', subs: [[0, 0, 1], [5, 7, 1], [0x7f, 0x7f, 0]], recs: [[2, 'newname', -1], [4, 'beta', -1]] },
    { name: 'other', flags: 9, subs: [[2, 9, 1], [10, 10, 0]], recs: [[7, 'other', 1]] },
    { name: 'third', flags: 1, recs: [[1, null, -1], [0x7f, 'third', -1]] },
    { name: 'gamma', recs: [[1, null, 0], [2, 'gamma', 3], [3, 'delta', -1], [4, 'gamma', -1], [9, 'gamma', 2]] },
    { name: 'fourth', flags: 8, subs: [[0x40, 0x41, 1]], recs: [[0, 'fourth', 'z'], [1, 'fourth', -1]] },
    { name: 'delta', recs: [[8, 'delta', 0]] },
    { name: '', flags: 0xfe, recs: [[2, '', -1]] },
  ];
  function lookup(variant) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    heap(out, 0x600);
    const vt = recVtable(out.log, { 4: 1 });
    let spec = [['alpha', 1], ['beta', 4], ['gamma', 2], ['gamma', 1], ['delta', 1]];
    if (variant === 'hole') spec = [['alpha', 1], ['beta', 4], [null, 0], ['gamma', 1], ['delta', 1]];
    if (variant === 'nolist') spec = [];
    descList(out, spec);
    // zeta: a shared object (use count 1) that is not in the list, so its node is not found (0x100225b1 taken)
    const z = block(0, 0x2e7b0000, 0x220);
    u32(z, 0, cstr('zeta'));
    u32(z, 0x210, 1);
    out.z = z;
    LK.forEach(function (r, i) {
      const q = block(i, 0x22300000, 0xa00);
      for (let k = 0; k < 0x80; k++) u32(q, 0x744 + 4 * k, 0);
      (r.recs || []).forEach(function (e, j) {
        const rec = block(j, 0x2e300000 + 0x10000 * i, 0x40);
        u32(rec, 0x24, e[1] === null ? 0 : cstr(e[1]));
        u32(rec, 0x30, e[2] === 'z' ? z : ((e[2] >= 0 && out['p' + e[2]]) ? out['p' + e[2]] : 0));
        u32(q, 0x744 + 4 * e[0], rec);
        out['r' + i + '_' + j] = rec;
      });
      const d = block(i, 0xde800000, 0x220);
      for (let k = 0; k < 0x80; k++) u32(d, 4 + 4 * k, 0);
      (r.subs || []).forEach(function (s, j) {
        const sb = block(j, 0x5b000000 + 0x10000 * i, 0x20);
        u32(sb, 0, s[1]);
        if (s[2]) {
          // callSlot4 reads the member count +0x1438 (1 = no members) of every object it calls
          const o = recObj(j, 0x5c000000 + 0x100000 * i, vt, { 0x104: 0x77000 + j, 0x1438: 1 }, 0x1500);
          u32(sb, 0x10, o);
          out['so' + i + '_' + j] = o;
        }
        else u32(sb, 0x10, 0);
        u32(d, 4 + 4 * s[0], sb);
      });
      u32(d, 0x204, cstr(r.name === undefined ? 'x' : r.name));
      u32(d, 0x208, r.flags || 0);
      u32(d, 0x20c, 0x20c0000 + i);
      u32(d, 0x210, 0x2100000 + i);
      u32(d, 0x218, 0x2180000 + i);
      out['q' + i] = q;
      out['d' + i] = r.nul ? ptr(0) : d;
    });
    stubOn(out.log, 0x1fa0, 1, true, function (a) {
      const ent = a[0], s = a[1], f = s.readU32();
      if (f < 0x7f) ent.add(4 + 4 * (f + 1)).writeU32(s.add(0x10).readU32());
      return 0;
    });
    stubOn(out.log, 0x1f32, 2, true, function () { return 0x1f32; });
    stubOn(out.log, 0x1d66, 0, true, function () { return 0x1d66; });
    return out;
  }
  F.c3fk_lookup = function () { return lookup(''); };
  F.c3fk_lookup_hole = function () { return lookup('hole'); };
  F.c3fk_lookup_nolist = function () { return lookup('nolist'); };

  F.c3fk_build9 = function () { return buildTrack(true); };
  F.c3fk_build14 = function () { return buildTrack(false); };

  // ===================================================================================================================
  // C. Seq_buildPlayOrder, Midi_Device::handleMetaEvent
  // ===================================================================================================================
  // Seq_buildPlayOrder(out): 0x260-byte sequencers {+0x214 flags, +0x23c count, +0x248 lo, +0x24c hi, +0x244 marker}, 0x40-byte
  // marker-filled out buffers. The seed 0x100b4fe0 is swapped (state); WINMM!timeGetTime is stood in at sound.dll's IAT (its log
  // compared). nextRandomFloat (thunk 0x1000114f) and __ftol 0x10042550 run for real. Rows: [flags, count, lo, hi].
  const PO = [[0x100, 4, 2, 4], [0x100, 0, 5, 9], [0x100, -1, 7, 8], [0x180, 3, 0, 9], [0, 5, 3, 3], [0x80, 2, 10, 20],
              [0, 0, 1, 2], [0, -3, 1, 2], [0x100, 6, 0, 0], [0x1100, 3, 1, 5], [0x100, 5, 3, 1], [0xffffffff, 4, 2, 0xffffffff]];
  F.c3fk_playorder = function () {
    const out = {};
    quietTimer(out);
    PO.forEach(function (r, i) {
      const q = block(i, 0x23d00000, 0x260);
      u32(q, 0x214, r[0]);
      u32(q, 0x23c, r[1]);
      u32(q, 0x248, r[2]);
      u32(q, 0x24c, r[3]);
      out['q' + i] = q;
      out['b' + i] = block(i, 0x0b0b0000, 0x40);
    });
    swapGlobal(0xb4fe0, 0x4fe04fe0, out, 'gseed');
    out.implog = __importStandIn({ module: 'sound.dll', imports: { 'WINMM.dll!timeGetTime': { ret: [0x00012345, 0x0abcdef1] } } }).log;
    return out;
  };

  // Midi_Device::handleMetaEvent(p, track): 0x800-byte devices on a recording vtable (slots +0x84 and +0x9c one argument, +0x94
  // two; +0x9c answers the dword at this+0x19c, the row's span object or 0), event bytes, 0x460-byte tracks {+0x34, +0x38 flags,
  // +0x50, +0x458 a second track or 0}, span objects {+8 first, +0xc last (getSpanLength = last - first + 1), +0x14 loops,
  // +0x18 next id, +0x24 position, +0x28 loop count}, and the payload list at this+0x218 (head) / +0x220 (cursor) / +0x224
  // (count) with payloads {+4 id, +8, +0xc, +0x1c}. Midi_readVarLen, byteSwap16 and getSpanLength run for real (pure leaves);
  // Seq_rewindTracks 0x10001ca3, Seq_stepTrackMeta 0x100011cc and Seq_allocTrackSlot 0x100024cd are recording stubs.
  // Rows: {ev: event bytes, tf: track flags +0x38, t2: a second track, r: span object, cnt: +0x224, list: payload ids (null =
  // NULL payload), want: this+0x738 (the id looked for)}.
  const ME = [
    { ev: [0x51, 3, 0x07, 0xa1, 0x20] },                         // tempo 500000: 120
    { ev: [0x51, 3, 0x00, 0x00, 0x07] },                         // 60000000 / 7
    { ev: [0x51, 3, 0x00, 0x00, 0x00] },                         // tempo 0: the division gives infinity
    { ev: [0x51, 0x80, 0x03, 0xff, 0xff, 0xff] },                // two-byte length, tempo 0xffffff
    { ev: [0x51, 3, 0x0f, 0x42, 0x40] },                         // 1000000: 60
    { ev: [0x59, 2, 0x12, 0x34] },                               // key signature word
    { ev: [0x58, 4, 4, 2, 0x18, 8] },                            // time signature: nothing
    { ev: [0x01, 5, 0x41, 0x42, 0x43, 0x44, 0x45] },             // text (below 0x2f): nothing
    { ev: [0x7f, 0x81, 0x00].concat(new Array(0x80).fill(0x55)) }, // above 0x59: nothing, length 0x80
    { ev: [0x2f, 0], tf: 0 },                                    // end: flag only
    { ev: [0x2f, 0], tf: 2, t2: 1 },                             // rewind, second track decremented
    { ev: [0x2f, 0], tf: 6, t2: 1 },                             // alloc + rewind
    { ev: [0x2f, 0], tf: 4 },                                    // bit 2 without bit 1: nothing more
    { ev: [0x2f, 0], tf: 0x10 },                                 // loop, span object NULL
    { ev: [0x2f, 0], tf: 0x10, r: { n14: 0, lo: 1, hi: 5 } },    // span object with no loops
    { ev: [0x2f, 0], tf: 0x10, r: { n14: 3, n24: 0, lo: 2, hi: 6 } },              // position 0 -> 1 of 5
    { ev: [0x2f, 0], tf: 0x12, r: { n14: 3, n24: 4, n28: 0, lo: 2, hi: 6 } },      // position wraps, loop 0 -> 1 of 3
    { ev: [0x2f, 0], tf: 0x10, r: { n14: 3, n24: 4, n28: 2, lo: 2, hi: 6, v18: 7 }, cnt: 0 },   // loops done, count 0
    { ev: [0x2f, 0], tf: 0x10, r: { n14: 1, n24: 0, n28: 0, lo: 9, hi: 9, v18: 7 }, cnt: 1 },   // head NULL
    { ev: [0x2f, 0], tf: 0x10, r: { n14: 1, n24: 0, lo: 9, hi: 9, v18: 7 }, cnt: 1, list: [[7, 0x1c1c]] },     // head matches
    { ev: [0x2f, 0], tf: 0x10, r: { n14: 1, lo: 0, hi: 0, v18: 7 }, cnt: 2, list: [[3, 5], [4, 0], [7, 0]] }, // third, +0x1c 0
    { ev: [0x2f, 0], tf: 0x10, r: { n14: 1, lo: 0, hi: 0, v18: 8 }, cnt: 3, list: [[3, 5], [4, 6]] },         // no match: end
    { ev: [0x2f, 0], tf: 0x10, r: { n14: 1, lo: 0, hi: 0, v18: 7 }, cnt: 3, list: [[3, 5], null, [7, 1]] },   // NULL payload
    { ev: [0x2f, 0], tf: 0x10, r: { n14: 1, lo: 0, hi: 0, v18: 7 }, cnt: 1, list: [null] },                   // head payload NULL
    { ev: [0x2f, 0], tf: 0x10, r: { n14: 2, n24: 0, n28: 1, lo: 0, hi: 0, v18: 0xffffffff }, cnt: 1, list: [[0xffffffff, 2]] },
  ];
  F.c3fk_meta = function () {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    const vt = recVtable(out.log, { 0x84: 1, 0x9c: 1, 0x94: 2 });
    ME.forEach(function (r, i) {
      const o = recObj(i, 0x21b00000, vt, {}, 0x800);
      const e = keep(Memory.alloc(0x100));
      fill(e, 0x100, 0xee);
      e.writeByteArray(r.ev);
      const t = block(i, 0x7c000000, 0x460);
      t.add(0x38).writeU8(r.tf || 0);
      u32(t, 0x458, 0);
      if (r.t2) { const t2 = block(i, 0x7d000000, 0x40); u32(t, 0x458, t2); out['t2_' + i] = t2; }
      else out['t2_' + i] = block(i, 0x7e000000, 0x40);
      const sp = block(i, 0x5a000000, 0x40);
      if (r.r) {
        u32(sp, 8, r.r.lo); u32(sp, 0xc, r.r.hi); u32(sp, 0x14, r.r.n14);
        u32(sp, 0x18, r.r.v18 === undefined ? 0x18181818 : r.r.v18);
        u32(sp, 0x24, r.r.n24 || 0); u32(sp, 0x28, r.r.n28 || 0);
        u32(o, 0x19c, sp);
      }
      out['r' + i] = sp;
      u32(o, 0x224, r.cnt === undefined ? 0x22400000 + i : r.cnt);
      u32(o, 0x738, 0x73800000 + i);
      const ids = r.list || [];
      const nodes = ids.map(function (x, k) { return block(k, 0x0d000000 + 0x10000 * i, 0x10); });
      ids.forEach(function (x, k) {
        u32(nodes[k], 0, k ? nodes[k - 1] : 0);
        u32(nodes[k], 4, k + 1 < nodes.length ? nodes[k + 1] : 0);
        if (x === null) { u32(nodes[k], 8, 0); return; }
        const pl = block(k, 0x0e000000 + 0x10000 * i, 0x40);
        u32(pl, 4, x[0]);
        u32(pl, 0x1c, x[1]);
        u32(nodes[k], 8, pl);
        out['pl' + i + '_' + k] = pl;
      });
      ids.forEach(function (x, k) { out['nd' + i + '_' + k] = nodes[k]; });
      u32(o, 0x218, nodes.length ? nodes[0] : 0);
      u32(o, 0x220, 0x22022022);
      out['o' + i] = o;
      out['e' + i] = e;
      out['t' + i] = t;
    });
    stubOn(out.log, 0x1ca3, 0, true, function () { return 0x1ca3; });
    stubOn(out.log, 0x11cc, 1, true, function () { return 0x11cc; });
    stubOn(out.log, 0x24cd, 0, true, function () { return 0x24cd; });
    return out;
  };

  // ===================================================================================================================
  // D. DirectSoundDevice bodies
  // ===================================================================================================================
  // recCom (copied from fixtures.d/c3fd.js): a COM vtable whose slots are __stdcall NativeCallbacks (interface first) that log
  // {0x5200 + slot, arguments}, write the object's +0x140 + slot through argument `out` when the slot has one and return the
  // object's +0x100 + slot.
  function recCom(log, slots) {
    const vt = fill(keep(Memory.alloc(0x100)), 0x100, 0);
    Object.keys(slots).forEach(function (k) {
      const off = parseInt(k, 10), n = slots[k][0], outIdx = slots[k][1];
      vt.add(off).writePointer(keep(new NativeCallback(function () {
        const a = Array.prototype.slice.call(arguments);
        logCall(log, 0x5200 + off, a);
        if (outIdx !== undefined && !a[outIdx].isNull()) a[outIdx].writeU32(a[0].add(0x140 + off).readU32());
        return a[0].add(0x100 + off).readU32();
      }, 'uint32', Array(n + 1).fill('pointer'), 'stdcall')));
    });
    return vt;
  }
  // a chain of n 0x20-byte entries linked through +0x10 (the enumerated-device list); entry k's +0 / +4 / +8 are the string
  // pointers `strs(k)` (fixture values: operator delete only records them)
  function entryChain(out, name, n, strs) {
    const es = [];
    for (let k = 0; k < n; k++) {
      const e = block(k, 0x0e700000 + 0x1000 * name.length, 0x20);
      const s = strs ? strs(k) : [0, 0, 0];
      u32(e, 0, s[0]); u32(e, 4, s[1]); u32(e, 8, s[2]);
      es.push(e);
      out[name + k] = e;
    }
    es.forEach(function (e, k) { u32(e, 0x10, k + 1 < n ? es[k + 1] : 0); });
    return es;
  }
  // a node list {prev, next, payload} at dev + off (head, tail, +0xc count), payloads 1 = a fixture value, 0 = NULL
  function devList(out, dev, off, spec, name) {
    const ids = spec.map(function (h, k) { return h ? k : null; });
    const pl = spec.map(function (h, k) { return h ? (0x9c000000 + 0x100 * off + k) : 0; });
    return nodeList(out, dev, off, ids, pl, name, spec.length + 0x40);
  }

  // DirectSoundDevice::enumerate(flags, ctx): 0x200-byte devices. DirectSoundEnumerateA's import thunk 0x1003a978 is a recording
  // stub that does what the enumeration callback leaves behind, read from the device's own spare words: +4 = the entry chain at
  // +0x1f4, +0x148 = the count at +0x1f0, a non-zero +0x1fc into +8, and returns the HRESULT at +0x1f8. mapDsError runs for real
  // (thunk 0x1000268a, a pure C3 leaf). The device index 0x100b4a40 is swapped per key (_i0, _i2, _i9).
  const DE = [
    { pre: [1, 0, 0] }, { pre: [0, 2, 0] }, { pre: [0, 0, 3] }, { ctx: 0 }, { f: 0x20, hr: 0x80004001 }, { f: 0x60, n: 0 },
    { f: 0x40, n: 3 }, { f: 0xff, n: 5, set8: 0x88 }, { f: 0x21, n: 1 }, { f: 0, n: 10 }, { f: 0x60, hr: 0x88780078, n: 2 },
    { f: 0x9f, n: 0x10 },
  ];
  function dsEnum(idx) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    DE.forEach(function (r, i) {
      const d = block(i, 0x14e00000, 0x200);
      u32(d, 0, r.pre ? r.pre[0] : 0); u32(d, 4, r.pre ? r.pre[1] : 0); u32(d, 8, r.pre ? r.pre[2] : 0);
      d.add(0x6c).writeU8(0x80 | i);
      u32(d, 0x148, 0x14800000 + i);
      const n = r.n || 0;
      const es = entryChain(out, 'en' + i + '_', n);
      u32(d, 0x1f0, n);
      u32(d, 0x1f4, n ? es[0] : 0);
      u32(d, 0x1f8, r.hr || 0);
      u32(d, 0x1fc, r.set8 || 0);
      out['d' + i] = d;
      out['c' + i] = r.ctx === 0 ? ptr(0) : block(i, 0xc7c00000, 0x10);
    });
    swapGlobal(0xb4a40, idx, out, 'gidx');
    stubOn(out.log, 0x3a978, 2, false, function (a) {
      const d = a[1];
      u32(d, 4, d.add(0x1f4).readU32());
      u32(d, 0x148, d.add(0x1f0).readU32());
      if (d.add(0x1fc).readU32()) u32(d, 8, d.add(0x1fc).readU32());
      return d.add(0x1f8).readU32();
    });
    return out;
  }
  F.c3fk_dsenum_i0 = function () { return dsEnum(0); };
  F.c3fk_dsenum_i2 = function () { return dsEnum(2); };
  F.c3fk_dsenum_i9 = function () { return dsEnum(9); };

  // The DsBuffer list 0x100b4a34 (head) / 0x100b4a38 (tail), swapped for a list of 0x200-byte buffers (prev +0xbc, next +0xc0,
  // channel +0x20: 0 or a 0x200-byte channel block whose +0x1f0 names its buffer).
  function dsBufList(out, chans) {
    const bufs = chans.map(function (c, j) {
      const b = block(j, 0x0b100000, 0x200);
      u32(b, 0x20, 0);
      if (c) { const ch = block(j, 0x0c400000, 0x200); u32(ch, 0x1f0, b); u32(b, 0x20, ch); out['ch' + j] = ch; }
      out['b' + j] = b;
      return b;
    });
    bufs.forEach(function (b, j) {
      u32(b, 0xbc, j > 0 ? bufs[j - 1] : 0);
      u32(b, 0xc0, j + 1 < bufs.length ? bufs[j + 1] : 0);
    });
    swapGlobal(0xb4a34, bufs.length ? bufs[0] : 0, out, 'ghead');
    swapGlobal(0xb4a38, bufs.length ? bufs[bufs.length - 1] : 0, out, 'gtail');
    return bufs;
  }
  function jsDsUnlink(b) {
    const prev = b.add(0xbc).readPointer(), next = b.add(0xc0).readPointer();
    if (!prev.isNull()) prev.add(0xc0).writePointer(next); else snd(0xb4a34).writePointer(next);
    if (!next.isNull()) next.add(0xbc).writePointer(prev); else snd(0xb4a38).writePointer(prev);
    b.add(0xbc).writeU32(0); b.add(0xc0).writeU32(0);
  }

  // DirectSoundDevice::stopAll: 0x200-byte devices {+0 interface (a recCom object, slot +8 Release) or 0, +0x6c flags, +0x168
  // list, +0x18c counter, +0x198 extra buffer (a 0x200-byte block; its releaseIfOwned may clear the device's +0x198 through a
  // poke)}. Stubs: appendNode 0x10001681 (records), Channel_releaseBuffers 0x10001d11 (records and unlinks the buffer named by the
  // channel's +0x1f0, as the real one takes it out of the list), DsBuffer::release 0x1000144c, DsBuffer::destroy 0x10001910 and
  // DsBuffer::releaseIfOwned 0x100012d5 (record; releaseIfOwned applies the object's poke). Keys by buffer list: _l0 (empty),
  // _l3 (no channel, channel, no channel), _l2c (two with channels).
  const DSS = [
    { f: 0 }, { f: 2 }, { f: 0xfd, x: 1 }, { f: 2, x: 2 }, { f: 0, itf: 1 }, { f: 0xff, x: 1, itf: 1, l: [1, 1] },
    { f: 0, l: [1, 0, 1] }, { f: 2, l: [0, 1] }, { f: 0x42, x: 2, l: [1, 1, 1, 1] }, { f: 0, itf: 1, l: [1] },
    { f: 3, x: 1, itf: 1 }, { f: 0x80, l: [0] },
  ];
  function dsStop(chans) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    heap(out, 0x100);
    dsBufList(out, chans);
    const ivt = recCom(out.log, { 8: [0] });
    DSS.forEach(function (r, i) {
      // 0x280 bytes: releaseIfOwned's stub reads the poke words of this+0x70 at this+0x250 .. +0x26f (zero: no poke)
      const d = block(i, 0x1a800000, 0x280);
      fill(d.add(0x1e0), 0x20, 0);
      fill(d.add(0x250), 0x20, 0);
      d.add(0x6c).writeU8(r.f);
      u32(d, 0x18c, [0, 1, 0xffffffff, 7, 0x100, 2, 3, 4, 5, 6, 0x7fffffff, 9][i]);
      u32(d, 0, 0);
      if (r.itf) { const it = recObj(i, 0x1f100000, ivt, { 0x108: 0x108 + i }); u32(d, 0, it); out['it' + i] = it; }
      else out['it' + i] = block(i, 0x1f200000, 0x200);
      u32(d, 0x198, 0);
      const x = block(i, 0x1a900000, 0x200);
      fill(x.add(0x1e0), 0x20, 0);
      if (r.x) {
        u32(d, 0x198, x);
        if (r.x === 2) { const c = pokeCfg(d, 0x12d5, 0, 0x198); Object.keys(c).forEach(function (k) { u32(x, parseInt(k, 10), c[k]); }); }
      }
      out['x' + i] = x;
      devList(out, d, 0x168, r.l || [], 'nl' + i + '_');
      out['d' + i] = d;
    });
    stubOn(out.log, 0x1681, 1, true, function () { return 0x1681; });
    stubOn(out.log, 0x1d11, 0, true, function (a) { jsDsUnlink(a[0].add(0x1f0).readPointer()); return 0x1d11; });
    stubOn(out.log, 0x144c, 0, true, function () { return 0x144c; });
    stubOn(out.log, 0x1910, 0, true, function () { return 0x1910; });
    stubOn(out.log, 0x12d5, 0, true, function (a) { poke(a[0], 0x12d5); return 0x12d5; });
    return out;
  }
  F.c3fk_dsstop_l0 = function () { return dsStop([]); };
  // _l3's head carries a non-NULL +0xbc (a stray block), so the unlink at 0x10011ac6 writes stray+0xc0 (0x10011ac4 fall)
  F.c3fk_dsstop_l3 = function () {
    const out = dsStop([0, 1, 0]);
    out.stray = block(0, 0x57a70000, 0x200);
    u32(out.b0, 0xbc, out.stray);
    return out;
  };
  F.c3fk_dsstop_l2c = function () { return dsStop([1, 1]); };

  // DirectSoundDevice::close: 0x200-byte devices {+4 the enumerated-device chain (entries' strings are fixture values), +8, the
  // lists +0x178 and +0x168, +0xf0 the answer of the DsBuffer::destroy stub (called on this+0x70)}. Stubs: releaseAll 0x1000104b
  // and DsBuffer::destroy 0x10001910 (record); ole32!CoUninitialize is stood in at sound.dll's IAT; operator delete records.
  const DSC = [
    { e: 0 }, { e: 1, s: [1, 1, 1] }, { e: 3, s: [1, 0, 1] }, { e: 2, s: [0, 0, 0] }, { e: 0, a: [1, 1], b: [1] },
    { e: 1, s: [0, 1, 0], a: [0, 1] }, { e: 4, s: [1, 1, 0], a: [1, 0, 1], b: [0, 1] }, { e: 0, b: [1, 1, 1] },
    { e: 2, s: [1, 1, 1], a: [1], b: [1, 1, 0, 1] }, { e: 1, s: [1, 0, 0], a: [1, 1, 1] }, { e: 5, s: [0, 1, 1] },
    { e: 0, a: [0], b: [0] },
  ];
  F.c3fk_dsclose = function () {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    heap(out, 0x100);
    DSC.forEach(function (r, i) {
      const d = block(i, 0x12900000, 0x200);
      const es = entryChain(out, 'ce' + i + '_', r.e, function (k) {
        return (r.s || [0, 0, 0]).map(function (h, j) { return h ? (0x57000000 + 0x10000 * i + 0x100 * k + j) : 0; });
      });
      u32(d, 4, r.e ? es[0] : 0);
      u32(d, 0xf0, 0xf0f00000 + i);
      devList(out, d, 0x178, r.a || [], 'na' + i + '_');
      devList(out, d, 0x168, r.b || [], 'nb' + i + '_');
      out['d' + i] = d;
    });
    out.implog = __importStandIn({ module: 'sound.dll', imports: { 'ole32.dll!CoUninitialize': { args: 0, ret: 0 } } }).log;
    stubOn(out.log, 0x104b, 0, true, function () { return 0x104b; });
    stubOn(out.log, 0x1910, 0, true, function (a) { return a[0].add(0x80).readU32(); });
    return out;
  };

  // DirectSoundDevice::duplicateBuffer(chan, src, out): 0x200-byte devices {+0 an IDirectSound recCom object (slot +0x14
  // DuplicateSoundBuffer: logs, writes the object's +0x154 through its third argument, returns +0x114) or 0, +0x6c flags, +0x150
  // limit, +0x15c count, +0x168 list}, 0x200-byte channels and sources {+0x60 interface value, +0x1f0 the hasAuxInterfaces
  // stub's answer}, 0x10-byte out slots. The new buffer and node come from the arena (heapstandin.js). Stubs: DsBuffer::ctor
  // 0x10001c1c (records, returns its this), DsBuffer::destroy 0x10001910, DsDevice_registerBuffer 0x10001320, hasAuxInterfaces
  // 0x10002081, query3D 0x10001c30, queryNotify 0x10001762 (record). mapDsError runs for real. Keys: the DsBuffer list holds two
  // fixture buffers (main), is empty (_empty), or main with the second allocation (the node) failing (_oom).
  const DD = [
    { f: 0, itf: 1 }, { f: 2, itf: 0 }, { f: 2, itf: 1, c15c: 5, c150: 5 }, { f: 2, itf: 1, hr: 0x88780078 },
    { f: 2, itf: 1 }, { f: 2, itf: 1, l: [1, 1], aux: 1 }, { f: 3, itf: 1, hr: 0x80070057, l: [1] }, { f: 2, itf: 1, aux: 0x100 },
    { f: 2, itf: 1, c15c: 0xffffffff, c150: 0 }, { f: 2, itf: 1, aux: 0xff }, { f: 0xfe, itf: 1, l: [] }, { f: 2, itf: 1, hr: 1 },
  ];
  function dsDup(variant) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    heap(out, 0x200, variant === 'oom' ? { failMask: 2 } : {});
    dsBufList(out, variant === 'empty' ? [] : [0, 0]);
    const dvt = recCom(out.log, { 0x14: [2, 2] });   // DuplicateSoundBuffer(this, original, &duplicate)
    DD.forEach(function (r, i) {
      const d = block(i, 0x22600000, 0x200);
      d.add(0x6c).writeU8(r.f);
      u32(d, 0x150, r.c150 === undefined ? 8 : r.c150);
      u32(d, 0x15c, r.c15c === undefined ? i : r.c15c);
      const ds = recObj(i, 0x2d500000, dvt, { 0x114: r.hr || 0, 0x154: 0x15400000 + i });
      u32(d, 0, r.itf ? ds : 0);
      devList(out, d, 0x168, r.l || [], 'dl' + i + '_');
      const s = block(i, 0x5c000000, 0x200);
      u32(s, 0x60, 0x60600000 + i);
      u32(s, 0x1f0, r.aux || 0);
      out['d' + i] = d;
      out['ds' + i] = ds;
      out['s' + i] = s;
      out['c' + i] = block(i, 0xca000000, 0x200);
      out['o' + i] = block(i, 0x0a000000, 0x10);
    });
    stubOn(out.log, 0x1c1c, 0, true, function (a) { return a[0]; });
    stubOn(out.log, 0x1910, 0, true, function () { return 0x1910; });
    stubOn(out.log, 0x1320, 1, true, function () { return 0x1320; });
    stubOn(out.log, 0x2081, 0, true, function (a) { return a[0].add(0x1f0).readU32(); });
    stubOn(out.log, 0x1c30, 0, true, function () { return 0x1c30; });
    stubOn(out.log, 0x1762, 0, true, function () { return 0x1762; });
    return out;
  }
  F.c3fk_dsdup = function () { return dsDup(''); };
  F.c3fk_dsdup_empty = function () { return dsDup('empty'); };
  F.c3fk_dsdup_oom = function () { return dsDup('oom'); };

  // DirectSoundDevice::freeBufferById(chan, out): 0x200-byte devices {+0x6c flags, +0x158, +0x15c counters, +0x168 node list of
  // 0x200-byte buffers {+0x20 channel, +0x1f0 the isBusy stub's answer, +0x1f4 releaseIfOwned's answer}}, channels {+0x54 the
  // source passed on, +0x1f8 the duplicateBuffer stub's answer, +0x1fc the buffer it writes to *out when that answer is 0}. The
  // DsBuffer list holds row 6's idle buffer (head), row 4's (middle), a stray buffer and row 7's (tail). Stubs: isBusy
  // 0x10001b9f, voiceListRemove 0x10001ec4 (records; row 10's channel pokes the device's list head to NULL), releaseIfOwned
  // 0x100012d5, DsBuffer::destroy 0x10001910, duplicateBuffer's thunk 0x10002572. Rows: {busy flags of the nodes, flags, release
  // answer, dup answer}.
  const DF = [
    { nul: 1, busy: [0] }, { busy: [] }, { busy: [1, 1] }, { busy: [0, 1], f: 0 }, { busy: [1, 0], f: 2, rel: 0 },
    { busy: [0], f: 2, rel: 1, dup: 0x1d }, { busy: [0, 0], f: 2, rel: 0 }, { busy: [1, 1, 0], f: 0x42, rel: 0 },
    { busy: [1, 1, 0], f: 2, rel: 5 }, { busy: [0], f: 0xfd, dup: 0x25 }, { busy: [0, 1], f: 2, pokeHead: 1 }, { busy: [1, 0, 1], f: 3, rel: 0 },
  ];
  // DirectSoundDevice::createBuffer(out, flags, size) / createBufferFmt(out, fmt, flags, size): 0x200-byte devices {+0 interface
  // value, +0x6c flags, +0x14c, +0x150, +0x154, +0x158, +0x15c counters, +0x164, +0x168 node list of 0x200-byte buffers {+0x20
  // channel, +0x1f0 isBusy's answer, +0x1f4 releaseIfOwned's answer, +0x1f8 findAndUnlinkNode's answer (the buffer or 0)},
  // +0x1fc the DsBuffer::create stub's answer}. The new buffer comes from the arena; the DsBuffer list holds two fixture buffers
  // (main) or none (_empty). Stubs (record): isBusy 0x10001b9f, voiceListRemove 0x10001ec4, findAndUnlinkNode 0x100015a0,
  // unlinkDsBuffer 0x10001ebf (also unlinks its argument), releaseIfOwned 0x100012d5, scalarDeletingDtor 0x10002252, DsBuffer::ctor
  // 0x10001c1c (returns its this), setBufferSize 0x10001c35, setPrimaryFormat 0x10001d75, DsBuffer::create 0x10001bfe (answers the
  // device's +0x1fc through the new buffer's +0x5c), DsBuffer::destroy 0x10001910. Row 6's evicted buffer is in the DsBuffer list.
  const DCR = [
    { f6c: 0 }, { fl: 0 }, { fl: 0, hr: 0x88780078 }, { fl: 4, c158: 5, c154: 5 }, { fl: 4, c158: 1, c154: 8, c14c: 10, c15c: 3 },
    { fl: 4, c158: 1, c154: 8, c14c: 4, c15c: 5, busy: [1, 1] },
    { fl: 4, c158: 1, c154: 8, c14c: 4, c15c: 5, busy: [1, 0], rel: 0, find: 1, inList: 1 },
    { fl: 4, c158: 1, c154: 8, c14c: 4, c15c: 5, busy: [0], find: 0 },
    { fl: 4, c158: 1, c154: 8, c14c: 4, c15c: 5, busy: [0, 1], rel: 1, find: 1, hr: 0x80004005 },
    { fl: 0xff, c158: 0, c154: 2, c14c: 9, c15c: 0, nofmt: 1 }, { fl: 4, c158: 2, c154: 8, c14c: 3, c15c: 9 },
    { fl: 7, c158: 0, c154: 4, c14c: 6, c15c: 1, hr: 1 },
  ];
  function dsCreate(variant) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    heap(out, 0x200);
    const gl = dsBufList(out, variant === 'empty' ? [] : [0, 0]);
    DCR.forEach(function (r, i) {
      const d = block(i, 0x2d100000, 0x200);
      u32(d, 0, 0xd5000000 + i);
      d.add(0x6c).writeU8(r.f6c === undefined ? 2 : r.f6c);
      u32(d, 0x14c, r.c14c || 0); u32(d, 0x150, 0x150150 + i); u32(d, 0x154, r.c154 || 0);
      u32(d, 0x158, r.c158 || 0); u32(d, 0x15c, r.c15c || 0); u32(d, 0x164, 0x16400000 + i);
      u32(d, 0x1fc, r.hr || 0);
      const bufs = (r.busy || []).map(function (bz, k) {
        const b = block(k, 0x0b500000 + 0x10000 * i, 0x200);
        u32(b, 0x20, 0xc4000000 + 0x100 * i + k);
        u32(b, 0x1f0, bz); u32(b, 0x1f4, r.rel || 0); u32(b, 0x1f8, (!bz && r.find) ? b : 0);
        u32(b, 0xbc, 0); u32(b, 0xc0, 0);
        out['cb' + i + '_' + k] = b;
        return b;
      });
      nodeList(out, d, 0x168, bufs.map(function (b, k) { return k; }), bufs, 'cn' + i + '_', bufs.length + 0x20);
      if (r.inList && gl.length) {
        // put the idle buffer between the two list buffers
        const e = bufs.filter(function (b, k) { return !r.busy[k]; })[0];
        u32(gl[0], 0xc0, e); u32(e, 0xbc, gl[0]); u32(e, 0xc0, gl[1]); u32(gl[1], 0xbc, e);
      }
      out['d' + i] = d;
      out['o' + i] = block(i, 0x0a200000, 0x10);
      out['f' + i] = r.nofmt ? ptr(0) : block(i, 0xf0f00000, 0x20);
    });
    stubOn(out.log, 0x1b9f, 0, true, function (a) { return a[0].add(0x1f0).readU32(); });
    stubOn(out.log, 0x1ec4, 1, true, function () { return 0x1ec4; });
    stubOn(out.log, 0x15a0, 1, true, function (a) { return a[1].add(0x1f8).readU32(); });
    stubOn(out.log, 0x1ebf, 1, true, function (a) { jsDsUnlink(a[1]); return 0x1ebf; });
    stubOn(out.log, 0x12d5, 0, true, function (a) { return a[0].add(0x1f4).readU32(); });
    stubOn(out.log, 0x2252, 1, true, function () { return 0x2252; });
    stubOn(out.log, 0x1c1c, 0, true, function (a) { return a[0]; });
    stubOn(out.log, 0x1c35, 1, true, function () { return 0x1c35; });
    stubOn(out.log, 0x1d75, 1, true, function () { return 0x1d75; });
    stubOn(out.log, 0x1bfe, 2, true, function (a) { return a[0].add(0x5c).readPointer().add(0x1fc).readU32(); });
    stubOn(out.log, 0x1910, 0, true, function () { return 0x1910; });
    return out;
  }
  F.c3fk_dscreate = function () { return dsCreate(''); };
  F.c3fk_dscreate_empty = function () { return dsCreate('empty'); };

  // DirectSoundDevice::create(hwnd, level): 0x200-byte devices {+0 interface (written by the CoCreateInstance stand-in from the
  // device's +0x1f4, 0 to leave it NULL), +8 the selected entry (0 = none), +0xc caps, +0x10 caps flag, +0x18c counter, +0x30,
  // +0x40, +0x6c flags, +0x164, +0x178 list, +0x1f4 the interface to install, +0x1fc the CoCreateInstance HRESULT, +0x1e8
  // configureDesc's answer, +0x1ec create's answer}. The IDirectSound interface is a recCom object (slot +0x10 GetCaps writes its
  // +0x150 through the caps pointer and returns +0x110, +0x18 SetCooperativeLevel returns +0x118, +0x28 Initialize returns +0x128).
  // ole32!CoInitialize and CoCreateInstance are stood in at the IAT (CoInitialize returns 0; CoCreateInstance writes *ppv from the
  // device's +0x1f4 and returns +0x1fc). Stubs (record): setPrimaryFormat 0x10001d75, configureDesc 0x10001b4a (returns the
  // device's +0x1e8), DsBuffer::create 0x10001bfe (returns the device's +0x1ec), releaseAll 0x1000104b, parseRiffWave 0x10001e51;
  // mapDsError runs for real; operator delete records. The hwnd global 0x100b4a3c is swapped.
  // (a CoCreateInstance HRESULT != 0 is not exercised: the original then dereferences the NULL interface +0 at the
  //  SetCooperativeLevel block 0x10011d29, a fault both arms take; see the Path C note)
  const DV = [
    { hwnd: 0 }, { pre0: 1 }, { f6c: 0 }, { noent: 1, lvl: 0 }, {}, { init: 0x80004005 },
    { coop: 0x8007000e }, { f6c: 0x0d, caps: 0x20, caps2: 0x88780078 }, { f6c: 0x09, c40: 1 }, { cfg: 0x80004001 }, { crt: 1 },
    { f6c: 0x09, list: [1, 0, 1], c14c: 1 },
  ];
  F.c3fk_dscreate_dev = function () {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    heap(out, 0x100);
    const ivt = recCom(out.log, { 0x10: [1, 1], 0x18: [2], 0x28: [1] });  // GetCaps(this, pCaps), SetCoop(this, hwnd, level), Initialize(this, guid)
    swapGlobal(0xb4a3c, 0x4a3c0000, out, 'ghwnd');
    DV.forEach(function (r, i) {
      const d = block(i, 0x11c00000, 0x200);
      u32(d, 0, r.pre0 ? (0x5e000000 + i) : 0);
      // the selected entry: a 0x20-byte block whose +8 is the GUID the device's Initialize is called with
      const ent = block(i, 0xe8000000, 0x20);
      u32(ent, 8, 0xe8e80000 + i);
      u32(d, 8, r.noent ? 0 : ent);
      out['ent' + i] = ent;
      u32(d, 0x10, r.caps || 0);
      u32(d, 0x18c, 0x18c00000 + i);
      u32(d, 0x30, 0x3030 + i);
      u32(d, 0x40, r.c40 || 5);
      d.add(0x6c).writeU8(r.f6c === undefined ? 3 : r.f6c);
      u32(d, 0x164, 0x16400000 + i);
      u32(d, 0x1fc, r.cohr || 0);
      u32(d, 0x1e8, r.cfg || 0);
      u32(d, 0x1ec, r.crt || 0);
      const itf = recObj(i, 0x5e100000, ivt, { 0x110: r.caps2 || 0, 0x118: r.coop || 0, 0x128: r.init || 0, 0x150: 0x15000000 + i });
      u32(d, 0x1f4, r.cohr ? 0 : itf);
      out['itf' + i] = itf;
      devList(out, d, 0x178, r.list || [], 'vn' + i + '_');
      u32(d, 0x14c, r.c14c || 0);
      out['d' + i] = d;
      out['h' + i] = r.hwnd === 0 ? ptr(0) : block(i, 0xca010000, 0x10);
    });
    out.implog = __importStandIn({ module: 'sound.dll', imports: {
      'ole32.dll!CoInitialize': { args: 1, ret: 0 },
      'ole32.dll!CoCreateInstance': { args: 5, impl: function (a) {
        const ppv = a[4];
        ppv.writePointer(ppv.add(0x1f4).readPointer());
        return ppv.add(0x1fc).readU32();
      } } } }).log;
    // setPrimaryFormat's format pointer is the caller's frame (differs between arms); log the WAVEFORMATEX's defined bytes
    // (the 16 header bytes plus the 2-byte cbSize; the 2 bytes after it are stack padding that differs between arms)
    stubOn(out.log, 0x1d75, 1, true, function (a) {
      for (let k = 0; k < 4; k++) logCall(out.log, 0x1d750000 + k, [a[1].add(4 * k).readU32()]);
      logCall(out.log, 0x1d750004, [a[1].add(16).readU16()]);
      return 0x1d75;
    }, [1]);
    stubOn(out.log, 0x1b4a, 1, true, function (a) { return a[0].sub(0x70).add(0x1e8).readU32(); });
    stubOn(out.log, 0x1bfe, 2, true, function (a) { return a[0].sub(0x70).add(0x1ec).readU32(); });
    stubOn(out.log, 0x104b, 0, true, function () { return 0x104b; });
    stubOn(out.log, 0x1e51, 1, true, function () { return 0x1e51; });
    return out;
  };

  F.c3fk_dsfree = function () { return dsFree(false); };
  F.c3fk_dsfree_nolist = function () { return dsFree(true); };   // the DsBuffer list empty (0x10012772 taken)
  function dsFree(nolist) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    heap(out, 0x100);
    const idle = {};
    DF.forEach(function (r, i) {
      const d = block(i, 0x26900000, 0x200);
      d.add(0x6c).writeU8(r.f || 0);
      u32(d, 0x158, 0x158 + i);
      u32(d, 0x15c, 0x15c0 + i);
      const ch = block(i, 0xcb000000, 0x200);
      fill(ch.add(0x1e0), 0x10, 0);
      u32(ch, 0x54, 0x54540000 + i);
      u32(ch, 0x1f8, r.dup || 0);
      const nb = block(i, 0x0b200000, 0x200);
      u32(ch, 0x1fc, nb);
      out['nb' + i] = nb;
      const bufs = r.busy.map(function (bz, k) {
        const b = block(k, 0x0b300000 + 0x10000 * i, 0x200);
        u32(b, 0x20, ch);
        u32(b, 0x1f0, bz);
        u32(b, 0x1f4, r.rel || 0);
        u32(b, 0xbc, 0); u32(b, 0xc0, 0);
        out['fb' + i + '_' + k] = b;
        if (!bz && idle[i] === undefined) idle[i] = b;
        return b;
      });
      nodeList(out, d, 0x168, bufs.map(function (b, k) { return k; }), bufs, 'fn' + i + '_', bufs.length + 0x10);
      if (r.pokeHead) Object.keys(pokeCfg(d, 0x1ec4, 0, 0x168)).forEach(function (k) { u32(ch, parseInt(k, 10), pokeCfg(d, 0x1ec4, 0, 0x168)[k]); });
      out['d' + i] = d;
      out['ch' + i] = r.nul ? ptr(0) : ch;
      out['chb' + i] = ch;
      out['o' + i] = block(i, 0x0a100000, 0x10);
    });
    // the DsBuffer list: row 6's idle buffer, row 4's, a stray buffer, row 7's
    out.stray = block(0, 0x57a80000, 0x200);
    const gl = [idle[6], idle[4], out.stray, idle[7]];
    gl.forEach(function (b, j) {
      u32(b, 0xbc, j > 0 ? gl[j - 1] : 0);
      u32(b, 0xc0, j + 1 < gl.length ? gl[j + 1] : 0);
    });
    swapGlobal(0xb4a34, nolist ? 0 : gl[0], out, 'ghead');
    swapGlobal(0xb4a38, nolist ? 0 : gl[gl.length - 1], out, 'gtail');
    stubOn(out.log, 0x1b9f, 0, true, function (a) { return a[0].add(0x1f0).readU32(); });
    stubOn(out.log, 0x1ec4, 1, true, function (a) { poke(a[0], 0x1ec4); return 0x1ec4; });
    stubOn(out.log, 0x12d5, 0, true, function (a) { return a[0].add(0x1f4).readU32(); });
    stubOn(out.log, 0x1910, 0, true, function () { return 0x1910; });
    stubOn(out.log, 0x2572, 3, true, function (a) {
      const ch = a[1], r = ch.add(0x1f8).readU32();
      if (!r) a[3].writePointer(ch.add(0x1fc).readPointer());
      return r;
    });
    return out;
  };
})();
