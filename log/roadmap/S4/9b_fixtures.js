// S4.9b fixtures: loadSoundFile (rva 0x41d0, slot 4 of the Sound vtable 0x5b000). Evaluated into each key's fixture-VM context after
// 8_fixtures.js and 9_fixtures.js (9b_replay.KeyRun9b.setup); the same mock Frida API as re/frida/js/fixtures.d/*.js and the helpers of
// c3fn.js (F.c3fn_h). Addresses in comments are rvas of the user's sound.dll; the instruction citations are log/roadmap/S4/9b_notes.md's.
//
// Every key: the timer is held quiet (c3fn's quietTimer), operator new / malloc carve from a heap stand-in arena and operator delete /
// free only record (the arena and the free log are state), the twelve WINMM mmio imports are served by the mmio stand-in from the key's
// files (its block and its import log are state), g_openMmioCount (rva 0xb4a08) and the shared sound list g_cmdTableHead .. +0xc (rva
// 0xb4fe8, the list Sound_findByName / Sound_lookupOrCreate / Sound_reapIdle walk) are swapped for the key. The Sound objects are built by
// the real Sound::ctor (rva 0x3f30) under the stand-ins; their vtable pointer is then a fixture copy of the Sound vtable whose slot +0x14
// (rva 0x20480 in the original table, called when bit 0 of +0xac is set, 0x4264) is a recording stub returning the key's `busy` value.
//
// FILES. Synthetic bank files built here from the chunk layouts loadSoundFile reads (notes section 2), per layout: a `trak` chunk's
// payload is read over the whole new Ctrl5b668 object, vtable slot included (0x4594), and the success path calls that slot
// (0x4f49), so a payload's first dword is the address of a fixture vtable (built here, so it follows the layout's base). The user's own
// files (keys `user*`) come from 9b_replay.py, which reads them from original\ and evaluates `globalThis.__s49bUser = {name: hex}` before
// the fixture runs; no file bytes are in the repo.
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const HN = F.c3fn_h;
  const snd = HN.snd, u32 = HN.u32;

  // ---- file builder -----------------------------------------------------------------------------------------------------------------
  function Enc() { this.a = []; }
  Enc.prototype.u32 = function (v) { v = (v instanceof NativePointer) ? v.toUInt32() : (v >>> 0); this.a.push(v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff); return this; };
  Enc.prototype.tag = function (s) { for (let i = 0; i < 4; i++) this.a.push(s.charCodeAt(i) & 0xff); return this; };
  Enc.prototype.str = function (s, nul) { for (let i = 0; i < s.length; i++) this.a.push(s.charCodeAt(i) & 0xff); if (nul !== false) this.a.push(0); return this; };
  Enc.prototype.raw = function (b) { for (let i = 0; i < b.length; i++) this.a.push(b[i] & 0xff); return this; };
  Enc.prototype.hex = function (h) { for (let i = 0; i < h.length; i += 2) this.a.push(parseInt(h.substr(i, 2), 16)); return this; };
  Enc.prototype.dw = function (list) { const self = this; list.forEach(function (v) { self.u32(v); }); return this; };
  Enc.prototype.bytes = function () { return new Uint8Array(this.a); };
  function dwBytes(list, n) {      // the first n bytes of a dword list
    const e = new Enc(); e.dw(list); const b = e.a.slice(0, n); while (b.length < n) b.push(0); return b;
  }

  // 'ctrl' (Wave_Device::readFmtChunk 0x5440): a dword it reads first, n, n bytes into the CtrlObject's +0x10 (the dword at payload +0xc is
  // the type, 3 adds three (size, bytes) pairs read into a Ctrl5b5c0 at +0x20, +8, +0x58)
  function ctrl(e, n, payload, pairs) {
    e.tag('ctrl').u32(0x0c0ffee0).u32(n).raw(dwBytes(payload, n));
    (pairs || []).forEach(function (p) { e.u32(p.length).raw(p); });
  }
  // 'trak' (0x4551): size, then size bytes over the new 0x34-byte Ctrl5b668 (+0 its vtable pointer)
  function trak(e, dws) { e.tag('trak').u32(4 * dws.length).dw(dws); }
  // 'glbl' (0x4479): size (decremented, never tested), n, n bytes into the 12-byte local {record size, record count, mode} the
  // 0x2e3c0 initializer set to {0x24, 0, 1}, then `count` records of `record size` bytes
  function glbl(e, n, local, recs) {
    e.tag('glbl').u32(0x1000).u32(n).raw(dwBytes(local, n));
    recs.forEach(function (r) { e.raw(r); });
  }
  // 'chnl' (0x4428): size, size bytes into the CtrlObject3 local
  function chnl(e, dws) { e.tag('chnl').u32(4 * dws.length).dw(dws); }
  // 'kmap' (0x460c): size, +0x208, +0x210, +0x20c, a NUL-ended name, a record count, the record size when +0x208 & 6, the records
  // ({d1, d2} or record-size bytes, then a NUL-ended name each), and a trailing dword read when size - consumed >= 4. The size the
  // chunk carries only steers the record loop (0x475b) and the trailing read (0x4999): consumed starts at 0xd (0x46a2) and the count
  // dword is not counted.
  function kmap(e, k) {
    let used = 12 + k.name.length + 1 + ((k.f208 & 6) ? 4 : 0);
    k.recs.forEach(function (r) { used += ((k.f208 & 6) ? k.recsize : 8) + r.name.length + 1; });
    const size = (k.size !== undefined) ? k.size : used + ((k.trail !== undefined) ? 4 : 0);
    e.tag('kmap').u32(size).u32(k.f208).u32(k.f210).u32(k.f20c).str(k.name).u32(k.count === undefined ? k.recs.length : k.count);
    if (k.f208 & 6) e.u32(k.recsize);
    k.recs.forEach(function (r) {
      if (k.f208 & 6) e.raw(dwBytes(r.dws, k.recsize)); else e.u32(r.d1).u32(r.d2);
      e.str(r.name);
    });
    if (k.trail !== undefined) e.u32(k.trail);
  }
  // 'prgm' (0x49e1): size; size 0x1c: n, 0x18 bytes into the ChanRecord local, n name bytes; any other size: 0x1c bytes, a NUL-ended name;
  // then a NUL-ended alternate name when consumed < size. p.size absent: the size that reads exactly this layout (consumed, or
  // consumed + 1 when an alternate name is given)
  function prgm(e, p) {
    if (p.size === undefined) p.size = 0x1c + p.name.length + 1 + (p.alt !== undefined ? 1 : 0);
    if (p.size === 0x1c) {
      e.tag('prgm').u32(0x1c).u32(p.name.length).raw(dwBytes(p.rec, 0x18)).str(p.name, false);
    } else {
      e.tag('prgm').u32(p.size).raw(dwBytes(p.rec, 0x1c)).str(p.name);
    }
    if (p.alt !== undefined) e.str(p.alt);
  }
  // MIDI images (synthetic): MThd + MTrk chunks with a few note events
  const MIDI = {
    m2: '4d546864000000060001000200604d54726b0000000c0090354000803500' + '00ff2f00' + '4d54726b00000010019039400a8039000a90' + '3c40' + '00ff2f00',
    m1: '4d54686400000006000000010060' + '4d54726b0000000c0090304005803000' + '00ff2f00',
    m3: '4d546864000000060001000300784d54726b000000080590354000ff2f004d54726b00000014099039400a913c410b923f420c93424300ff2f00' +
        '4d54726b0000000c0790374008913a4100ff2f00',
    bad: '4d546864000000060001000200',                                  // the header ends inside its own length
    nomtrk: '4d546864000000060001000100604d5472',                       // a track header cut short
  };
  function midi(e, k) { e.hex(MIDI[k]); }

  // ---- objects ----------------------------------------------------------------------------------------------------------------------------
  const MMSIZE = 0x20000;          // the mmio block of every key (9b_keys.py's $mmblock region)
  const SOUND_SIZE = 0xb00;           // create_sound allocates 0xa60 for this class; the rest is slack the fixture can see
  function soundVtable(log, busy) {
    const vt = HN.zero(0x200);
    for (let off = 0; off < 0x200; off += 4) vt.add(off).writeU32(snd(0x5b000).add(off).readU32());
    const stub = HN.recVtable(log, { 0x14: [0, function () { return busy; }] });
    vt.add(0x14).writePointer(stub.add(0x14).readPointer());
    return vt;
  }
  // a fixture vtable for the trak objects: slot 0 (the deleting destructor the success path calls with 1, 0x4f49) records and returns 0
  function trakVtable(log) { return HN.recVtable(log, { 0: [1, function () { return 0x7a0; }] }); }
  // a list {head, tail, cursor, count} of fixture nodes {prev, next, value} in block nb
  function nodeList(list, nb, values) { HN.nodeList(list, nb, 0, values); }

  // the shared sound list at rva 0xb4fe8: entries {+0 name pointer, +0x210 use count} (Sound_findByName 0x21240 compares +0's string;
  // Sound_reapIdle 0x204e0 keeps a signed +0x210 > 0)
  function soundList(out, names) {
    const nb = HN.zero(0x400);
    const vals = names.map(function (nm, i) {
      const ent = HN.block(0x220, 0x5e100000 + (i << 12));
      const s = HN.zero(0x40);
      s.writeUtf8String(nm);
      ent.writePointer(s);
      u32(ent, 0x210, 1);
      out['lent' + i] = ent;
      return ent;
    });
    out.lnodes = nb;
    const tmp = HN.zero(0x10);
    nodeList(tmp, nb, vals);
    HN.swapGlobal(0xb4fe8, tmp.readU32(), out, 'glist');
    HN.swapGlobal(0xb4fec, tmp.add(4).readU32(), out);
    HN.swapGlobal(0xb4ff0, 0, out);
    HN.swapGlobal(0xb4ff4, vals.length, out);
  }

  // ---- one key --------------------------------------------------------------------------------------------------------------------------
  // o: { files: (ctx) -> {name: bytes}, objs: n, busy, f214, fac, f58, gstr, list: [names], heap: {failMask, ...}, seek: value,
  //      ranges: [[first, last], ...] the nodes of the +0x218 list }
  function lsf(o) {
    return function () {
      const out = {};
      HN.quietTimer();
      out.log = HN.stubLog();
      // the Sound objects first: Sound::ctor's allocations (if any) come from the module's own heap, so the stand-in's call
      // counter (failMask) counts loadSoundFile's own allocations from 0
      out.vt = soundVtable(out.log, o.busy || 0);
      const ctor = new NativeFunction(snd(0x3f30), 'pointer', ['pointer'], 'thiscall');
      const n = o.objs || 1;
      out.objs = HN.block(n * SOUND_SIZE, 0x50f00000);
      out.rnodes = HN.zero(0x40 * n * 8);
      out.recs = HN.zero(0x20 * n * 8);
      for (let i = 0; i < n; i++) {
        const s = out.objs.add(SOUND_SIZE * i);
        ctor(s);
        s.writePointer(out.vt);
        if (o.f214) s.add(0x214).writeU32(s.add(0x214).readU32() | 0x40);
        if (o.fac) s.add(0xac).writeU32(s.add(0xac).readU32() | 1);
        // bit 0 of +0x58 (the path-prefix test, 0x480f / 0x4ac5): Sound::ctor leaves it set; f58 0 clears it, 1 sets it
        if (o.f58 === 1) s.add(0x58).writeU32(s.add(0x58).readU32() | 1);
        if (o.f58 === 0) s.add(0x58).writeU32(s.add(0x58).readU32() & ~1);
        const rg = o.ranges || [];
        const vals = rg.map(function (r, j) {
          if (r === null) return 0;      // a node whose record is NULL ends the range loop (0x4fc1)
          const rec = out.recs.add(0x20 * (i * 8 + j));
          u32(rec, 0, 0x7ec00000 + j); u32(rec, 4, 0x7ec10000 + j); u32(rec, 8, r[0]); u32(rec, 0xc, r[1]);
          return rec;
        });
        if (rg.length) HN.nodeList(s.add(0x218), out.rnodes, 0x40 * 8 * i, vals.concat([]));
        out['s' + i] = s;
      }
      const h = HN.heap(out, o.arena || 0x40000, o.heap || {});
      out.trakvt = trakVtable(out.log);
      const ctx = { trakvt: out.trakvt };
      const files = o.files(ctx);
      const mmOpts = { module: 'sound.dll', files: files, size: MMSIZE, opLogSize: 0x4000, implogSize: 0x4000, bufSlots: 2,
                       bufCap: 0x1000 };
      if (o.basenameOpen) mmOpts.basename = true;
      if (o.seek !== undefined) mmOpts.imports = { 'WINMM.dll!mmioSeek': { args: 3, ret: o.seek } };
      const m = __mmioStandIn(mmOpts);
      out.mmblock = m.block;
      out.implog = m.implog;
      out.mmsize = m.size;
      HN.swapGlobal(0xb4a08, 0x20, out, 'gmmio');
      soundList(out, o.list || []);
      if (o.gstr !== undefined) {
        __freeze.save(snd(0x69988), 0x40);
        snd(0x69988).writeByteArray(new Array(0x40).fill(0));
        snd(0x69988).writeUtf8String(o.gstr);
      }
      out.gstr = snd(0x69988);
      // file names: one block, a NUL-ended string each at 0x80 steps
      out.names = HN.zero(0x80 * 32);
      (o.names || Object.keys(files)).forEach(function (nm, j) { out.names.add(0x80 * j).writeUtf8String(nm); out['n' + j] = out.names.add(0x80 * j); });
      if (o.f214) HN.stubOn(out.log, 0x28810, 0, 'thiscall', function () { return 0x28810; });
      return out;
    };
  }
  F.s49b_lsf = lsf;
  F.s49b_enc = function () { return new Enc(); };
  F.s49b_parts = { ctrl: ctrl, trak: trak, glbl: glbl, chnl: chnl, kmap: kmap, prgm: prgm, midi: midi, MIDI: MIDI, dwBytes: dwBytes };

  // ---- the key files ----------------------------------------------------------------------------------------------------------------------
  // a trak payload: 13 dwords over the Ctrl5b668 {+0 vtable, ..., +0x28 track index, +0x2c flag byte, +0x30 value} (Seq_applyTrackFlag
  // 0x23e10 reads +0x28 / +0x2c / +0x30)
  function trakDws(vt, idx, flag, val) { return [vt, 0x6c727463, 0x34, 0x18, 0x44444444, 0xffffffff, 0, 1, 0xffffffff, 0xffffffff, idx, flag, val]; }
  // a NodeB000-sized glbl record (14 dwords, Seq_buildTrack14 0x22d00 copies them over its new node)
  function rec14(i) { const d = []; for (let k = 0; k < 14; k++) d.push(k === 0 ? 0 : ((0x100 * i + k) >>> 0)); d[9] = 0x10 * i; d[10] = 0x20 + i; return dwBytes(d, 0x38); }
  // a chnl payload: 14 dwords, the slot index at +0x28 (Seq_copyChannelParams 0x22280 has no bound: 0..15 are the 16 slots of +0x250)
  function chnlDws(idx, base) { const d = []; for (let k = 0; k < 14; k++) d.push((base + 0x11 * k) >>> 0); d[10] = idx; return d; }
  function prgmRec(ch, extra) { const r = [ch, 0x64, 0x7f, 0x40, 0x3c00, 0x11, 0, 0]; (extra || []).forEach(function (v, i) { r[1 + i] = v; }); return r; }

  function filesAll(ctx) {
    const P = F.s49b_parts;
    const fs = {};
    let e;
    e = new Enc(); midi(e, 'm2'); fs['mid2.mid'] = e.bytes();
    // every chunk kind, then a MIDI image
    e = new Enc();
    ctrl(e, 0x18, [0x11, 0x22, 0x33, 1, 0x55, 0x66]);
    // no type-3 ctrl (readFmtChunk's +0x2c record copies an uninitialised dword of its own frame, 0x555b: U-1491)
    ctrl(e, 0x18, [0x101, 0x102, 0x103, 2, 0x105, 0x106]);
    ctrl(e, 0x10, [0x201, 0x202, 0x203, 0]);
    trak(e, trakDws(ctx.trakvt, 0, 2, 0x1234));
    trak(e, trakDws(ctx.trakvt, 1, 0, 0x5678));
    glbl(e, 12, [0x38, 2, 1], [rec14(1), rec14(2)]);
    glbl(e, 12, [0x38, 0, 2], []);
    glbl(e, 12, [0x20, 1, 4], [rec14(3).slice(0, 0x20)]);
    chnl(e, chnlDws(3, 0x1000));
    chnl(e, chnlDws(0, 0x2000).slice(0, 8));
    kmap(e, { f208: 0, f210: 0x40, f20c: 0x10, name: 'kmA', recs: [{ d1: 5, d2: 9, name: 'vA0' }, { d1: 12, d2: 3, name: 'vA1' }, { d1: 7, d2: 7, name: '' }], trail: 0x99 });
    kmap(e, { f208: 2, f210: 0x41, f20c: 0x11, name: 'kmB', recsize: 0x10, recs: [{ dws: [20, 30, 0x777, 0x888], name: 'vB0' }] });
    kmap(e, { f208: 4, f210: 0x42, f20c: 0x12, name: '', recsize: 8, recs: [{ dws: [1, 2], name: 'vC0' }, { dws: [3, 4], name: 'vC1' }], trail: 0x55 });
    kmap(e, { f208: 0, f210: 0x43, f20c: 0x13, name: 'kmA', recs: [{ d1: 1, d2: 2, name: 'vD0' }] });
    kmap(e, { f208: 0, f210: 0x44, f20c: 0x14, name: 'kmE', recs: [], count: 0 });
    prgm(e, { size: 0x1c, rec: prgmRec(5), name: 'pgAA' });
    prgm(e, { size: 0x1c, rec: prgmRec(6), name: 'pg', alt: 'altB' });
    prgm(e, { size: 0x30, rec: prgmRec(7).concat([0x1d]), name: 'pgC', alt: 'altC' });
    prgm(e, { rec: prgmRec(8).concat([0x1e]), name: 'pgD' });
    prgm(e, { rec: prgmRec(0x90), name: 'pgE' });
    prgm(e, { rec: prgmRec(9), name: '' });
    prgm(e, { rec: prgmRec(10), name: 'pgG', alt: '' });
    midi(e, 'm3');
    fs['all.bnk'] = e.bytes();
    // a known chunk, then an unknown tag
    e = new Enc(); ctrl(e, 0x18, [1, 2, 3, 1, 5, 6]); trak(e, trakDws(ctx.trakvt, 0, 2, 1)); e.tag('junk').u32(0); fs['unk.bnk'] = e.bytes();
    // a kmap whose size ends its record loop after one record (0x475b): the second record is read as the next tag (unknown: 8)
    e = new Enc(); kmap(e, { f208: 0, f210: 0x45, f20c: 0x15, name: 'kmF', recs: [{ d1: 1, d2: 2, name: 'vF0' }, { d1: 3, d2: 4, name: 'vF1' }], size: 12 + 4 + 8 + 4 }); midi(e, 'm1'); fs['kcut.bnk'] = e.bytes();
    // tracks and a glbl, then a MIDI image (key seeknomove: the stood-in seek reports success without moving, so
    // Midi_Device::loadFromMemory reads the dword after 'MThd' and returns 0x12)
    e = new Enc(); trak(e, trakDws(ctx.trakvt, 0, 2, 1)); trak(e, trakDws(ctx.trakvt, 1, 2, 2)); glbl(e, 12, [0x38, 1, 1], [rec14(4)]); midi(e, 'm1'); fs['bad.bnk'] = e.bytes();
    // tracks then a MIDI image: the success path's track loop (index 5 is above the track count: Seq_applyTrackFlag returns 0xa)
    e = new Enc(); trak(e, trakDws(ctx.trakvt, 0, 2, 0x11)); trak(e, trakDws(ctx.trakvt, 1, 0, 0x22)); trak(e, trakDws(ctx.trakvt, 5, 2, 0x33)); midi(e, 'm2'); fs['trak.bnk'] = e.bytes();
    // a trak with a 0-byte payload keeps the constructed Ctrl5b668 (its own vtable, rva 0x5b668)
    e = new Enc(); e.tag('trak').u32(0); midi(e, 'm1'); fs['trak0.bnk'] = e.bytes();
    e = new Enc(); e.tag('RIFF').u32(4).tag('WAVE'); fs['riff.wav'] = e.bytes();
    // an unknown tag above 'ctrl' (the compare chain's other half, 0x43ff -> 0x45f6 -> 0x4606)
    e = new Enc(); ctrl(e, 0x18, [1, 2, 3, 1, 5, 6]); e.tag('zzzz').u32(0); fs['unkz.bnk'] = e.bytes();
    // three tracks before an unknown tag (the inline drain 0x4d35 loops twice)
    e = new Enc(); trak(e, trakDws(ctx.trakvt, 0, 2, 1)); trak(e, trakDws(ctx.trakvt, 1, 2, 2)); trak(e, trakDws(ctx.trakvt, 2, 0, 3));
    e.tag('junk').u32(0); fs['unk3.bnk'] = e.bytes();
    // one track then a MIDI image (a one-node list in the drains)
    e = new Enc(); trak(e, trakDws(ctx.trakvt, 0, 2, 0x44)); midi(e, 'm1'); fs['trak1.bnk'] = e.bytes();
    return fs;
  }
  // NULL track objects in the drains: a trak chunk of size 0 whose Ctrl5b668 allocation the key's failMask refuses leaves a {0, 0, NULL}
  // node (allocations per trak: the object, then its node, so trak k's object is call 2k). Tracks: 'x' full, '0' empty.
  function filesNull(ctx) {
    const fs = {};
    const mk = function (pattern, tail) {
      const e = new Enc();
      for (let k = 0; k < pattern.length; k++) {
        if (pattern[k] === 'x') trak(e, trakDws(ctx.trakvt, k & 1, 2, 0x100 + k)); else e.tag('trak').u32(0);
      }
      if (tail === 'junk') e.tag('junk').u32(0); else midi(e, 'm1');
      return e.bytes();
    };
    fs['n0.bnk'] = mk('0xx');      // mask 1:  [NULL, A, B]: the success loop stops at NULL, the final drain pops A and B
    fs['n1.bnk'] = mk('0x');       // mask 1:  [NULL, A]
    fs['n4.bnk'] = mk('0xxx');     // mask 1:  [NULL, A, B, C]: the final drain's loop pops a node with a successor (0x5012)
    fs['u0.bnk'] = mk('0', 'junk');   // mask 1:  [NULL] before an unknown tag
    fs['s0.bnk'] = mk('0');        // mask 1:  [NULL] (seek keys)
    fs['n2.bnk'] = mk('00');       // mask 5:  [NULL, NULL]
    fs['n3.bnk'] = mk('0x0');      // mask 0x11: [NULL, A, NULL]
    fs['u2.bnk'] = mk('x0', 'junk');  // mask 4:  [A, NULL] before an unknown tag
    fs['s2.bnk'] = mk('x0');       // mask 4:  [A, NULL] (seek keys)
    return fs;
  }
  const NAMES_BASIC = ['mid2.mid', 'all.bnk', 'unk.bnk', 'kcut.bnk', 'trak.bnk', 'trak0.bnk', 'riff.wav', 'missing.bnk', 'bad.bnk', 'unkz.bnk',
                       'unk3.bnk'];
  F.s49b_basic = lsf({ files: filesAll, objs: 11, names: NAMES_BASIC, list: ['kmZ'], ranges: [[0, 1], [3, 2], [1, 7], null, [0, 0]] });
  F.s49b_busy7 = lsf({ files: filesAll, objs: 2, names: ['all.bnk', 'mid2.mid'], fac: 1, busy: 7 });
  F.s49b_busy0 = lsf({ files: filesAll, objs: 2, names: ['all.bnk', 'mid2.mid'], fac: 1, busy: 0, f214: 1 });
  F.s49b_seekfail = lsf({ files: filesAll, objs: 4, names: ['trak.bnk', 'mid2.mid', 'all.bnk', 'trak1.bnk'], seek: 0xffffffff });
  F.s49b_nopre = lsf({ files: filesAll, objs: 2, names: ['all.bnk', 'trak.bnk'], f58: 0 });
  F.s49b_seeknomove = lsf({ files: filesAll, objs: 3, names: ['bad.bnk', 'mid2.mid', 'trak.bnk'], seek: 4 });
  F.s49b_list = lsf({ files: filesAll, objs: 2, names: ['all.bnk', 'trak.bnk'], list: ['kmA', 'kmB', 'kmE'] });

  // the path-prefix arms of kmap / prgm (bit 0 of +0x58, 0x480f / 0x4ac5): the Sound's own name (+0x948) with or without a backslash,
  // names in the file with or without one, and the module string at rva 0x69988 equal to the prefix or not
  function filesPrefix(ctx) {
    const fs = {};
    const e = new Enc();
    kmap(e, { f208: 0, f210: 1, f20c: 2, name: 'kp', recs: [{ d1: 1, d2: 2, name: 'plain' }, { d1: 3, d2: 4, name: 'sub\\slot' }, { d1: 5, d2: 6, name: '' }] });
    prgm(e, { size: 0x1c, rec: prgmRec(1), name: 'pplain' });
    prgm(e, { rec: prgmRec(2), name: 'x\\pslash' });
    prgm(e, { rec: prgmRec(3), name: '' });
    midi(e, 'm1');
    fs['pre.bnk'] = e.bytes();
    return fs;
  }
  const PRE_NAMES = ['pre.bnk', 'dir\\pre.bnk', 'a\\b\\pre.bnk', '\\pre.bnk', 'ab\\pre.bnk'];
  function prefixKey(gstr) {  // five Sound objects, one per PRE_NAMES entry
    return lsf({ files: filesPrefix, objs: PRE_NAMES.length, names: PRE_NAMES, f58: 1, gstr: gstr, basenameOpen: true });
  }
  F.s49b_pre_g0 = prefixKey(undefined);
  F.s49b_pre_g1 = prefixKey('dir\\');
  F.s49b_pre_g2 = prefixKey('a\\b\\');
  F.s49b_pre_g3 = prefixKey('a\\c\\');
  F.s49b_pre_g4 = prefixKey('dXr\\');     // differs at an odd index (0x48a1 / 0x4b57)
  F.s49b_pre_g5 = prefixKey('ab\\');      // equal over an odd length (0x48ab / 0x4b61 fall through)

  // allocation failures (heap stand-in failMask: call k returns NULL when bit k is set). Each file starts with the chunk whose object
  // must come back NULL, so its allocation is call 0 (or call 1 / 2 after the objects named).
  function filesOom(ctx) {
    const fs = {};
    let e;
    e = new Enc(); e.tag('trak').u32(0); midi(e, 'm1'); fs['t0.bnk'] = e.bytes();                       // call 0: Ctrl5b668 (0x34)
    e = new Enc(); e.tag('trak').u32(0); midi(e, 'm1'); fs['t1.bnk'] = e.bytes();                       // call 1: its list node (0xc)
    e = new Enc(); glbl(e, 12, [0x38, 0, 1], []); midi(e, 'm1'); fs['g0.bnk'] = e.bytes();               // call 0: NodeB000 (0x38)
    e = new Enc(); kmap(e, { f208: 0, f210: 1, f20c: 2, name: 'ko', recs: [], count: 0 }); midi(e, 'm1'); fs['k2.bnk'] = e.bytes();   // call 2: VoiceSlot (0x110)
    return fs;
  }
  F.s49b_oom0 = lsf({ files: filesOom, objs: 2, names: ['t0.bnk', 'g0.bnk'], heap: { failMask: 1 } });
  F.s49b_oom1 = lsf({ files: filesOom, objs: 1, names: ['t1.bnk'], heap: { failMask: 2 } });
  F.s49b_oom2 = lsf({ files: filesOom, objs: 1, names: ['k2.bnk'], heap: { failMask: 4 } });
  // NULL track objects in the list drains (filesNull)
  F.s49b_nullA = lsf({ files: filesNull, objs: 4, names: ['n0.bnk', 'n1.bnk', 'u0.bnk', 'n4.bnk'], heap: { failMask: 1 } });
  F.s49b_nullB = lsf({ files: filesNull, objs: 1, names: ['n2.bnk'], heap: { failMask: 5 } });
  F.s49b_nullC = lsf({ files: filesNull, objs: 1, names: ['n3.bnk'], heap: { failMask: 0x11 } });
  F.s49b_nullU = lsf({ files: filesNull, objs: 1, names: ['u2.bnk'], heap: { failMask: 4 } });
  F.s49b_sfnull = lsf({ files: filesNull, objs: 1, names: ['s0.bnk'], heap: { failMask: 1 }, seek: 0xffffffff });
  F.s49b_sfnullU = lsf({ files: filesNull, objs: 1, names: ['s2.bnk'], heap: { failMask: 4 }, seek: 0xffffffff });
  F.s49b_snmnull = lsf({ files: filesNull, objs: 1, names: ['s0.bnk'], heap: { failMask: 1 }, seek: 4 });
  F.s49b_snmnullU = lsf({ files: filesNull, objs: 1, names: ['s2.bnk'], heap: { failMask: 4 }, seek: 4 });

  // development: one chunk of each layout, then a MIDI image (a vector that returns 8 instead of 0 desynchronised the stream)
  function filesDbg(ctx) {
    const fs = {};
    const add = function (nm, fn) { const e = new Enc(); fn(e); midi(e, 'm1'); fs[nm] = e.bytes(); };
    add('c1', function (e) { ctrl(e, 0x18, [0x11, 0x22, 0x33, 1, 0x55, 0x66]); });
    add('c3', function (e) { ctrl(e, 0x18, [0x101, 0x102, 0x103, 3, 0x105, 0x106], [dwBytes([1, 2, 3, 4], 0x10), dwBytes([5, 6], 8), dwBytes([7, 8, 9], 0xc)]); });
    add('t', function (e) { trak(e, trakDws(ctx.trakvt, 0, 2, 0x1234)); });
    add('g', function (e) { glbl(e, 12, [0x38, 2, 1], [rec14(1), rec14(2)]); });
    add('ch', function (e) { chnl(e, chnlDws(3, 0x1000)); });
    add('kA', function (e) { kmap(e, { f208: 0, f210: 0x40, f20c: 0x10, name: 'kmA', recs: [{ d1: 5, d2: 9, name: 'vA0' }], trail: 0x99 }); });
    add('kB', function (e) { kmap(e, { f208: 2, f210: 0x41, f20c: 0x11, name: 'kmB', recsize: 0x10, recs: [{ dws: [20, 30, 0x777, 0x888], name: 'vB0' }] }); });
    add('kF', function (e) { kmap(e, { f208: 0, f210: 0x45, f20c: 0x15, name: 'kmF', recs: [{ d1: 1, d2: 2, name: 'vF0' }, { d1: 3, d2: 4, name: 'vF1' }], size: 12 + 4 + 8 + 4 }); });
    add('pA', function (e) { prgm(e, { size: 0x1c, rec: prgmRec(5), name: 'pgAA' }); });
    add('pB', function (e) { prgm(e, { size: 0x1c, rec: prgmRec(6), name: 'pg', alt: 'altB' }); });
    add('pC', function (e) { prgm(e, { size: 0x30, rec: prgmRec(7).concat([0x1d]), name: 'pgC', alt: 'altC' }); });
    add('pD', function (e) { prgm(e, { size: 0x20, rec: prgmRec(8).concat([0x1e]), name: 'pgD' }); });
    return fs;
  }
  const DBG = ['c1', 'c3', 't', 'g', 'ch', 'kA', 'kB', 'kF', 'pA', 'pB', 'pC', 'pD'];
  F.s49b_dbg = lsf({ files: filesDbg, objs: DBG.length, names: DBG });

  // the user's own files (9b_replay.py evaluates globalThis.__s49bUser = {name: hex} from original\ first)
  function filesUser() {
    const fs = {};
    const U = globalThis.__s49bUser || {};
    Object.keys(U).forEach(function (k) { const e = new Enc(); e.hex(U[k]); fs[k] = e.bytes(); });
    return fs;
  }
  F.s49b_user = function () {
    const names = Object.keys(globalThis.__s49bUser || {});
    return lsf({ files: filesUser, objs: Math.max(1, names.length), names: names, f58: 1 })();
  };
})();
