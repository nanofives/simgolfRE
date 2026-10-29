// S4.9 fixtures for the rows whose registry key the fixture VM cannot run, or that have no key (log/roadmap/S4/emuonly9.py CUSTOM8).
// Evaluated into each key's VM context after 8_fixtures.js (9_replay.KeyRun8.setup), so F.s48_mastervol exists. The same mock Frida
// API as re/frida/js/fixtures.d/*.js.
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const mod = Process.getModuleByName('sound.dll');
  const snd = (rva) => mod.base.add(rva);
  function fill(p, n, b) { p.writeByteArray(new Array(n).fill(b)); return p; }
  function marker(p, n, tag) {
    for (let i = 0; i < n; i += 4) p.add(i).writeU32(((tag << 16) ^ (i * 0x01010101)) >>> 0);
    return p;
  }
  function block(n, tag) { return marker(Memory.alloc(n), n, tag); }

  // onWaveInCallbackMessage (rva 0x38fd0): the registry key's c3dh_wavecb waits for the live game's sound engine (flag bit 5;
  // S4.1 census FIXTURE_ERR); here g_soundEngine (rva 0xb4a20) is set by the fixture to an engine object whose flag word +0x1a4 has
  // bit 5 (`eng`), lacks it (`eng0`), or to NULL (`noeng`; the sound-thread post then runs, and the flag byte of the thread state at
  // rva 0xb5038 decides it: 0 after DllMain, set to 1 by `noeng1` so PostThreadMessageA runs). The window handle g_soundWindow
  // (rva 0xb5018's neighbour in the engine path) is whatever the module holds.
  function waveCb(engine, threadFlag) {
    const D = globalThis.DIFF_FIXTURES;
    const out = {};
    __freeze.thread();
    __freeze.save(snd(0xb4a20), 4);
    __freeze.save(snd(0xb5038), 0x30);
    let eng = ptr(0);
    if (engine !== null) {
      eng = block(0x200, 0x5370);
      eng.add(0x1a4).writeU32(engine >>> 0);
      out.eng = eng;
    }
    __freeze.setU32(snd(0xb4a20), eng.toUInt32());
    snd(0xb5038).writeU8(threadFlag);
    snd(0xb5038).add(0x1c).writeU32(0x7a5c);
    out.implog = D.c3dh_postimports();
    out.tstate = snd(0xb5038);
    return out;
  }
  F.s49_wavecb_eng = function () { return waveCb(0x20, 0); };
  F.s49_wavecb_eng0 = function () { return waveCb(0xffffffdf, 0); };
  F.s49_wavecb_noeng = function () { return waveCb(null, 0); };
  F.s49_wavecb_noeng1 = function () { return waveCb(null, 1); };

  // setOutputMasterVolume (rva 0xe2a0): g_dsDevice (rva 0xb49f0) = the device of S4.8's masterVol fixture (`dev`), or NULL (`null`)
  F.s49_outvol_dev = function () {
    const fx = F.s48_mastervol();
    __freeze.save(snd(0xb49f0), 4);
    __freeze.setU32(snd(0xb49f0), fx.dev.toUInt32());
    return fx;
  };
  F.s49_outvol_null = function () {
    const fx = F.s48_mastervol();
    __freeze.save(snd(0xb49f0), 4);
    __freeze.setU32(snd(0xb49f0), 0);
    return fx;
  };

  // writeWaveFileB (rva 0x75e0, `ret 8`): the stereo twin of writeWaveFile. c3fl_wf's objects, engine list, import stand-ins and
  // thunk stubs (DsBuffer::getPosition 0x233d, DsBuffer::restore 0x213f, ... the stubs read the buffer's own fields), plus the two
  // thunks writeWaveFileB alone calls: DsBuffer::lockCaptureRegion (0x24b9, this = the right buffer; (bytes, &lockOut): the stub
  // writes [this+0x24] to lockOut and returns [this+0x20]) and writeWaveChunkB (0x1cb2, six arguments: recorded). The six buffers'
  // +0x20 / +0x24 hold the lock result / out value; buffer 4's getPosition result is 0x2d (the flag-and-go-on case).
  const HL = F.c3fl_h;
  const LOCKRES = [0, 0, 0, 0, 5, 0];
  F.s49_wfb = function (set) {
    const out = F.c3fl_wf(set);
    for (let j = 0; j < 6; j++) { HL.u32(out['b' + j], 0x20, LOCKRES[j]); HL.u32(out['b' + j], 0x24, 0x7000 + j); }
    HL.u32(out.b3, 0x48, 0x2d);
    HL.stubOn(out.log, 0x24b9, 2, 'thiscall', function (a) {
      a[2].writeU32(a[0].add(0x24).readU32());
      return a[0].add(0x20).readU32();
    }, { skip: [2] });
    HL.stubOn(out.log, 0x1cb2, 6, 'thiscall', function () { return 0x1cb2; }, { skip: [] });
    return out;
  };
  ['noeng', 'empty', 'nullobj', 'k6', 'k4', 'k1', 'k1b', 'mix'].forEach(function (k) { F['s49_wfb_' + k] = function () { return F.s49_wfb(k); }; });

  // writeWaveChunkB (rva 0x8070, `ret 0x18`): the stereo twin of writeWaveChunk, built like c3fn_wwc (c3fn.js): per vector a writer
  // (0x400 bytes: +0x30 output handle of the mmio stand-in's write-only file, +0x38 mix buffer freed through the heap stand-in), a
  // left and a right DsBuffer (0x300 each: block align word +0xb4; for the stubbed DsBuffer::restore (0x213f) the region info it
  // writes back at +0x200 p1 / +0x204 b1 / +0x208 p2 / +0x20c b2 / +0x210 result), 4 region buffers (0x200) and 0x80 source
  // doubles. DsBuffer::unlock (0x1ec9) is a recording stub. Rows: [alignL, b1L, b2L, alignR, b1R, b2R, restoreL, restoreR, output?,
  // mix?]; no row has a zero count (the original's `dec ecx; jne` loop runs 2^32 times for one: U-1481).
  const HN = F.c3fn_h;
  const WCB = [
    [4, 0x40, 0x20, 4, 0x40, 0x20, 0, 0, 1, 0], [4, 0x40, 0, 4, 0x40, 0, 0, 0, 1, 1], [4, 0x40, 0x20, 2, 0x20, 0x10, 0, 0, 0, 0],
    [2, 0x20, 0x10, 4, 0x40, 0x20, 0, 0, 1, 0], [4, 0x40, 0x20, 4, 0x40, 0x20, 1, 0, 1, 0], [4, 0x40, 0x20, 4, 0x40, 0x20, 0, 1, 1, 0],
    [8, 0x80, 0x40, 4, 0x40, 0, 0, 0, 1, 0], [4, 0x40, 0x20, 4, 0x40, 0x20, 0, 0, 0, 1],
  ];
  const DBL = function (i) {
    const t = [0, 100.0, 32766.5, -32767.5, 40000.0, -40000.0, 1.5, 2.5, -0.5, 16383.49, -16384.51, 32767.0, -32768.0];
    return t[i % t.length] + (i % 5) - 2;
  };
  F.s49_wcb = function () {
    const out = {};
    HN.quietTimer();
    const m = HN.mmio(out, { 'wcb.dat': HN.ramp(0x400, 0, 0, 0) }, { bufSlots: 0 });
    HN.heap(out, 0x100);
    out.log = HN.stubLog();
    const N = WCB.length;
    out.objs = HN.block(N * 0x400, 0x07100000);
    out.bufsL = HN.block(N * 0x300, 0x0a100000);
    out.bufsR = HN.block(N * 0x300, 0x0a200000);
    out.regs = HN.block(N * 4 * 0x200, 0x0b100000);
    out.srcs = HN.block(N * 0x400, 0x0c100000);
    out.mixes = HN.block(N * 0x100, 0x0e100000);
    for (let i = 0; i < N; i++) {
      const r = WCB[i];
      const o = out.objs.add(0x400 * i), aL = out.bufsL.add(0x300 * i), aR = out.bufsR.add(0x300 * i);
      const rg = [0, 1, 2, 3].map(function (k) { return out.regs.add(0x200 * (4 * i + k)); });
      const src = out.srcs.add(0x400 * i), mixb = out.mixes.add(0x100 * i);
      out['o' + i] = o; out['l' + i] = aL; out['r' + i] = aR; out['s' + i] = src;
      aL.add(0xb4).writeU16(r[0]);
      HN.u32(aL, 0x200, rg[0]); HN.u32(aL, 0x204, r[1]); HN.u32(aL, 0x208, r[2] ? rg[1] : 0); HN.u32(aL, 0x20c, r[2]); HN.u32(aL, 0x210, r[6]);
      aR.add(0xb4).writeU16(r[3]);
      HN.u32(aR, 0x200, rg[2]); HN.u32(aR, 0x204, r[4]); HN.u32(aR, 0x208, r[5] ? rg[3] : 0); HN.u32(aR, 0x20c, r[5]); HN.u32(aR, 0x210, r[7]);
      for (let k = 0; k < 0x80; k++) HN.wd(src.add(8 * k), DBL(i + k));
      HN.u32(o, 0x30, r[8] ? m.open('wcb.dat', 1, 0) : 0);
      HN.u32(o, 0x38, r[9] ? mixb : 0);
    }
    HN.swapGlobal(0xb4a08, 0x30, out, 'gmmio');
    HN.stubOn(out.log, 0x213f, 6, 'thiscall', function (a) {
      const a0 = a[0];
      a[3].writePointer(a0.add(0x200).readPointer());
      a[4].writeU32(a0.add(0x204).readU32());
      a[5].writePointer(a0.add(0x208).readPointer());
      a[6].writeU32(a0.add(0x20c).readU32());
      return a0.add(0x210).readU32();
    }, [3, 4, 5, 6]);
    HN.stubOn(out.log, 0x1ec9, 4, 'thiscall', function () { return 0; });
    return out;
  };

  // Wave_Device::open (rva 0xb420) and Wave_Device::start (rva 0xbaa0): one fixture per scenario `cfg`. The module globals the two read
  // and write (g_soundTimer 0xb49ec, the flag byte 0xb49bc, 0xb49c0, the device 0xb49f0, the stream device 0xb49f4, the primary buffer
  // 0xb49f8, the two format buffers 0xb49fc / 0xb4a00, the object 0xb4a1c, 0xb4a3c; the sound window object 0xb49b8 sits in the same
  // block) are one saved block [0xb49b8, 0xb4a40) the key sets and compares ($gl). The callee thunks are recording stubs (log `$log`):
  // DirectSoundDevice::enumerate / create / createBuffer / createBufferFmt answer from cfg (the buffer calls store a fixture buffer
  // through their slot argument), initSoundVoice / initStreamBuffer return their object after the device byte +0x6c is set from cfg;
  // `this`'s virtual slots 0x3c / 0x40 / 0x50 and the buffers' COM slot 0x4c (stdcall) are recording callbacks; operator new /
  // delete come from the heap stand-in. cfg: devflags (device byte +0x6c), enumRes, createRes, cbRes (createBuffer), fmt1, fmt2 (the
  // two createBufferFmt results), dev / stream / prim / sec (existing objects at entry), timer, f49bc (byte), selfFlags (+0x1a4), com
  // (the format buffers' COM pointer present), obj1c (+0x60 / +0x58 object answers).
  const GL = 0xb49b8, GLN = 0x88;
  function comVtable(log, slots) {
    const vt = HN.zero(0x200);
    Object.keys(slots).forEach(function (k) {
      const off = parseInt(k, 10), nargs = slots[k][0], retfn = slots[k][1];
      const argt = ['pointer'].concat(Array(nargs).fill('pointer'));
      vt.add(off).writePointer(HN.keep(new NativeCallback(function () {
        const a = Array.prototype.slice.call(arguments);
        HN.logCall(log, 0x5200 + off, a);
        const r = retfn ? retfn(a) : 0;
        return (typeof r === 'number') ? (r >>> 0) : ptr(r).toUInt32();
      }, 'uint32', argt, 'stdcall')));
    });
    return vt;
  }
  function wdev(cfg) {
    const out = {};
    HN.quietTimer();
    out.log = HN.stubLog();
    HN.heap(out, 0x400);
    const mk = function (n, tag) { return HN.block(n, tag); };
    out.self = mk(0x1c0, 0x0b100000);
    out.self.writePointer(HN.recVtable(out.log, { 0x3c: [1, function () { return 0; }], 0x40: [1, function () { return 0x1234; }], 0x50: [0, function () { return 0; }] }));
    HN.u32(out.self, 0x1a4, cfg.selfFlags >>> 0);
    HN.u32(out.self, 0x1c4, 0x5555);
    out.dev = mk(0x200, 0x0b200000); out.dev.add(0x6c).writeU8(cfg.devflags);
    HN.u32(out.dev, 0x1ac, 0x3f800000); HN.u32(out.dev, 0x1b0, 0x40000000); HN.u32(out.dev, 0x1b4, 0x40400000);
    out.stream = mk(0x40, 0x0b300000);
    out.prim = mk(0x100, 0x0b400000); out.prim.add(0xb4).writeU16(4); HN.u32(out.prim, 0x38, 0x4000);
    const comVt = comVtable(out.log, { 0x4c: [4, function () { return 0; }] });
    out.com1 = HN.zero(0x40); out.com1.writePointer(comVt);
    out.com2 = HN.zero(0x40); out.com2.writePointer(comVt);
    out.s1 = mk(0x100, 0x0b500000); out.s1.add(0xb4).writeU16(2); HN.u32(out.s1, 0x38, 0x2000);
    out.s2 = mk(0x100, 0x0b600000); out.s2.add(0xb4).writeU16(8); HN.u32(out.s2, 0x38, 0x800);
    HN.u32(out.s1, 0x64, cfg.com ? out.com1 : 0); HN.u32(out.s2, 0x64, cfg.com ? out.com2 : 0);
    out.timer = mk(0x20, 0x0b700000);
    out.obj1c = HN.zero(0x40);
    out.obj1c.writePointer(HN.recVtable(out.log, { 0x60: [0, function () { return cfg.x60; }], 0x58: [0, function () { return 0; }] }));
    __freeze.save(snd(GL), GLN);
    const g = function (rva, v) { __freeze.setU32(snd(rva), (v instanceof NativePointer) ? v.toUInt32() : v >>> 0); };
    g(0xb49ec, cfg.timer ? out.timer : 0);
    g(0xb49bc, cfg.f49bc); g(0xb49c0, 0x77c0);
    g(0xb49f0, cfg.dev ? out.dev : 0); g(0xb49f4, cfg.stream ? out.stream : 0); g(0xb49f8, cfg.prim ? out.prim : 0);
    g(0xb49fc, cfg.sec ? out.s1 : 0); g(0xb4a00, cfg.sec ? out.s2 : 0);
    g(0xb4a1c, cfg.x60 === null ? 0 : out.obj1c); g(0xb4a3c, 0x3c3c);
    out.gl = snd(GL);
    const L = out.log;
    HN.stubOn(L, 0x1177, 1, 'thiscall', function () { return 0; }, [0]);
    HN.stubOn(L, 0x21a3, 0, 'thiscall', function (a) {
      a[0].add(0x6c).writeU8(cfg.devflags);
      HN.u32(a[0], 0x1ac, 0x3f800000); HN.u32(a[0], 0x1b0, 0x40000000); HN.u32(a[0], 0x1b4, 0x40400000);
      return a[0];
    });
    HN.stubOn(L, 0x170d, 0, 'thiscall', function (a) { return a[0]; });
    HN.stubOn(L, 0x2013, 2, 'thiscall', function () { return cfg.enumRes; });
    HN.stubOn(L, 0x180c, 2, 'thiscall', function () { return cfg.createRes; });
    HN.stubOn(L, 0x1b81, 0, 'thiscall', function () { return 0; });
    HN.stubOn(L, 0x104b, 0, 'thiscall', function () { return 0; });
    HN.stubOn(L, 0x1677, 1, 'thiscall', function () { return 0; });
    HN.stubOn(L, 0x1046, 3, 'thiscall', function (a) {
      if (cfg.cbRes === 0) a[1].writePointer(out.prim);
      return cfg.cbRes;
    }, [1]);
    HN.stubOn(L, 0x11e5, 4, 'thiscall', function (a) {
      const first = a[1].toUInt32() === snd(0xb49fc).toUInt32();      // by the slot argument: a JS counter would differ between the arms
      const res = first ? cfg.fmt1 : cfg.fmt2;
      if (res === 0) a[1].writePointer(first ? out.s1 : out.s2);
      return res;
    }, [1, 2]);
    [[0x182f, 1], [0x1c5d, 0], [0x1203, 1], [0x1311, 1], [0x164a, 3], [0x227a, 1], [0x1dfc, 0]].forEach(function (e) {
      HN.stubOn(L, e[0], e[1], 'thiscall', function () { return 0; });
    });
    HN.stubOn(L, 0x16db, 0, 'thiscall', function () { return 0; }, [0]);
    return out;
  }
  const BASE = { devflags: 0x18, enumRes: 0, createRes: 0, cbRes: 0, fmt1: 0, fmt2: 0, dev: 0, stream: 0, prim: 0, sec: 0, timer: 1,
                 f49bc: 0, selfFlags: 0, com: 1, x60: 1 };
  const SC = {
    nodev_ok: {}, nodev_enumfail: { enumRes: 7 }, nodev_createfail: { createRes: 3 }, nodev_notimer: { timer: 0, selfFlags: 0x20, f49bc: 4 },
    dev_cbfail: { dev: 1, stream: 1, cbRes: 5 }, dev_nofmt: { dev: 1, stream: 1, devflags: 0x08 },
    dev_fmt1fail: { dev: 1, stream: 1, prim: 1, fmt1: 9 }, dev_fmt2fail: { dev: 1, stream: 1, prim: 1, fmt2: 9 },
    dev_fmt_nocom: { dev: 1, stream: 1, prim: 1, com: 0 }, dev_prim: { dev: 1, stream: 1, prim: 1, devflags: 0 },
    dev_secbufs: { dev: 1, stream: 1, prim: 1, sec: 1, selfFlags: 2, x60: 0 }, dev_nox60: { dev: 1, stream: 1, prim: 1, selfFlags: 2, x60: null },
  };
  Object.keys(SC).forEach(function (k) {
    const cfg = Object.assign({}, BASE, SC[k]);
    F['s49_wdev_' + k] = function () { return wdev(cfg); };
  });
  F.s49_wdev_start_noflag = function () { return wdev(Object.assign({}, BASE, { dev: 1, stream: 1, selfFlags: 0 })); };
  const SCS = {
    noflag: { selfFlags: 0 }, nodev: {}, nostream: { dev: 1 }, primexists: { dev: 1, stream: 1, prim: 1 },
    createfail: { dev: 1, stream: 1, createRes: 3 }, cbfail: { dev: 1, stream: 1, cbRes: 5 }, ok: { dev: 1, stream: 1 },
    nofmt: { dev: 1, stream: 1, devflags: 0x08 }, fmt1fail: { dev: 1, stream: 1, fmt1: 9 }, fmt2fail: { dev: 1, stream: 1, fmt2: 9 },
    nocom: { dev: 1, stream: 1, com: 0 }, nox60: { dev: 1, stream: 1, x60: null }, x60zero: { dev: 1, stream: 1, x60: 0 },
    prim_ok: { dev: 1, stream: 1, prim: 1 }, plain: { dev: 1, stream: 1, prim: 1, devflags: 0 },
  };
  Object.keys(SCS).forEach(function (k) {
    const cfg = Object.assign({}, BASE, { selfFlags: 2 }, SCS[k]);
    F['s49_wdevs_' + k] = function () { return wdev(cfg); };
  });

  // Wave_Device::dispatchCommand (rva 0xcd80), extra devices: c3fl_wd's fixture (re/frida/js/fixtures.d/c3fl.js section M) with a
  // generated list of devices instead of its eight. Each device holds 3-5 channels (flags +0x44 / +0x58 / +0xc8 / +0xcc / +0x1428,
  // decoder +0xc4, engine objects +0xf8 / +0xfc, sub-channel count +0x1438, some linked on the device list in order) and 4-6 commands;
  // the generator is a fixed LCG, so the key is deterministic. A command of kind 1-4 never targets a NULL channel (the original
  // reads +0x44 of it and faults, as c3fl's rows know).
  const CHS = function (o) {
    return Object.assign({ f44: 0, f58: 0, c8: 0, cc: 0, b1428: 0, c4: 0, f8: 0, fc: 0, cnt: 0, subs: null, pre: false, s0cc: 0, kidc8: 0, klink: false }, o);
  };
  function genDevs(n, seed) {
    let s = seed >>> 0;
    const rnd = function (m) { s = (Math.imul(s, 1103515245) + 12345) >>> 0; return (s >>> 16) % m; };
    const pick = function (a) { return a[rnd(a.length)]; };
    const KINDS = [1, 2, 3, 4, 17, 2, 3, 4, 1, 6, 13, 14, 15, 16, 5, 1, 2, 3];
    const devs = [];
    for (let d = 0; d < n; d++) {
      const nch = 3 + (d % 3), chs = [], cmds = [];
      for (let c = 0; c < nch; c++) {
        const cnt = pick([0, 2, 3, 3]);
        chs.push(CHS({ f44: pick([4, 4, 0x100, 0, 0x104]), f58: pick([0, 2, 3, 2]), c8: pick([0, 0x40, 1, 0x41]), cc: pick([0, 4]),
                       b1428: pick([0, 1]), c4: pick([0, 1]), f8: pick([0, 1]), fc: pick([0, 1]), cnt: cnt,
                       subs: [rnd(3) > 0, rnd(3) > 0], pre: rnd(3) > 0, s0cc: pick([0, 1]), kidc8: pick([0, 0x40]), klink: rnd(2) > 0 }));
      }
      const m = 4 + (d % 3);
      for (let k = 0; k < m; k++) {
        const kind = pick(KINDS);
        const ci = (kind > 4 && kind !== 17 && rnd(8) === 0) ? -1 : rnd(nch);
        cmds.push([kind, ci, rnd(3) * 0x111, rnd(3) * 0x2222]);
      }
      devs.push({ chs: chs, cmds: cmds });
    }
    return devs;
  }
  // hand-made devices for the branches the generator reaches rarely: kind 17 with engine objects that carry +0xc8 bit 6 and sit on the
  // device list, kinds 2 / 3 / 4 on channels whose sub-channels and engine objects are linked after them
  const HAND = [
    { chs: [CHS({ f8: 1, fc: 1, kidc8: 0x40, klink: true }), CHS({ f8: 1, fc: 0, kidc8: 0x40, klink: true, pre: true }),
            CHS({ f8: 0, fc: 1, kidc8: 0x40, klink: true })],
      cmds: [[17, 0, 0, 0], [17, 1, 0, 0], [17, 2, 0, 0]] },
    { chs: [CHS({ f44: 4, c8: 0x40, pre: true, cnt: 3, subs: [true, true], kidc8: 0x40, klink: true }),
            CHS({ f44: 4, f58: 2, pre: true, cnt: 3, subs: [true, false], klink: true }),
            CHS({ f44: 4, c8: 0x40, f58: 0, pre: true, c4: 1 })],
      cmds: [[3, 0, 0, 0], [2, 1, 0, 0], [4, 2, 0, 0], [4, 0, 0, 0], [3, 1, 0, 0]] },
    { chs: [CHS({ f44: 4, pre: true }), CHS({ f44: 4, pre: true }), CHS({ f44: 4, pre: true }), CHS({ f44: 4, pre: true })],
      cmds: [[2, 1, 0, 0], [2, 3, 0, 0], [2, 0, 0, 0], [2, 2, 0, 0], [2, 2, 0, 0]] },
  ];
  const GEN_DEVS = genDevs(24, 0x5eed49).concat(HAND);
  F.s49_wdgen = function (mode) {
    const out = {};
    out.log = HL.stubLog(0x1000);
    const hp = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], free: [0x4249a], arenaSize: 0x2000 });
    out.freelog = hp.log;
    out.heaparena = hp.arena;
    const s = __importStandIn({ module: 'sound.dll', imports: { 'USER32.dll!PostMessageA': { args: 4, ret: 1 } } });
    out.implog = s.log;
    const ARENA = 0x500000;
    const arena = HL.zero(ARENA);
    out.arena = arena;
    let cur = 0;
    function take(n) { const p = arena.add(cur); cur += (n + 0xf) & ~0xf; if (cur > ARENA) throw new Error('s49_wdgen: arena'); return p; }
    const chvt = HL.recVtable(out.log, {
      0: [1, function () { return 0; }],
      0xcc: [0, function (a) { return a[0].add(0x24).readU32(); }],
      0x34: [3, function () { return 0x34; }],
      0x40: [1, function () { return 0x40; }],
    });
    function mkch(sp, tag) {
      const o = take(0x1480);
      o.writePointer(chvt);
      HL.u32(o, 0x24, sp.s0cc); HL.u32(o, 0x44, sp.f44); HL.u32(o, 0x58, sp.f58); HL.u32(o, 0xc8, sp.c8);
      HL.u8(o, 0xcc, sp.cc); HL.u8(o, 0x1428, sp.b1428); HL.u32(o, 0x1438, sp.cnt);
      HL.u32(o, 0x1420, 0x14200000 + tag);
      if (sp.c4) HL.u32(o, 0xc4, take(0x80).toUInt32());
      o.kids = [];
      if (sp.f8) { const k = mkch(CHS({ c8: sp.kidc8 }), tag + 0x1000); o.add(0xf8).writePointer(k); o.kids.push(k); }
      if (sp.fc) { const k = mkch(CHS({ c8: sp.kidc8 }), tag + 0x2000); o.add(0xfc).writePointer(k); o.kids.push(k); }
      (sp.subs || []).forEach(function (on, k) {
        if (on) { const kid = mkch(CHS({ c8: sp.kidc8 }), tag + 0x3000 + k); o.add(0x1440 + 4 * (k + 1)).writePointer(kid); o.kids.push(kid); }
      });
      return o;
    }
    GEN_DEVS.forEach(function (ds, di) {
      const dev = take(0x4200);
      HL.u32(dev, 0x1a4, mode === 'post' ? 0x20 : 0);
      const chs = ds.chs.map(function (sp, ci) { const o = mkch(sp, di * 0x100 + ci); out['d' + di + 'c' + ci] = o; return o; });
      let prev = null, head = null, count = 0;
      const linkNode = function (obj) {
        const n = take(0xc);
        n.add(8).writePointer(obj);
        n.writePointer(prev || ptr(0));
        if (prev) prev.add(4).writePointer(n); else head = n;
        prev = n; count++;
      };
      ds.chs.forEach(function (sp, ci) {
        if (sp.pre) linkNode(chs[ci]);
        if (sp.klink) chs[ci].kids.forEach(linkNode);
      });
      dev.add(0x1c8).writePointer(head || ptr(0));
      dev.add(0x1cc).writePointer(prev || ptr(0));
      HL.u32(dev, 0x1d4, count);
      ds.cmds.forEach(function (cm, k) {
        const slot = dev.add(0x1d8 + 4 * ((k + 1) & 0xfff));
        if (cm === null) { slot.writePointer(ptr(0)); return; }
        const c = take(0x10);
        HL.u32(c, 0, cm[0]);
        c.add(4).writePointer(cm[1] < 0 ? ptr(0) : chs[cm[1]]);
        HL.u32(c, 8, cm[2]); HL.u32(c, 0xc, cm[3]);
        slot.writePointer(c);
      });
      HL.u32(dev, 0x41d8, 0);
      HL.u32(dev, 0x41dc, ds.cmds.length);
      out['dev' + di] = dev;
    });
    HL.quietTimer(out);
    HL.stubOn(out.log, 0x2257, 1, 'thiscall', function (a) { return a[0].add(4 * (a[1].toUInt32() & 0xfff)).readU32(); });
    HL.stubOn(out.log, 0x2031, 0, 'thiscall', function (a) { const p = a[0]; p.writeU32(0); HL.u32(p, 4, 0); HL.u32(p, 8, 0); return p; });
    HL.stubOn(out.log, 0x154b, 1, 'thiscall', function () { return 0x154b; });
    [[0x1f37, 0], [0x11f9, 0], [0x22e8, 0], [0x168b, 0], [0x1e79, 0], [0x272a, 1], [0x1b9f, 0], [0x11d6, 3]].forEach(function (e) {
      HL.stubOn(out.log, e[0], e[1], 'thiscall', function () { return e[0]; });
    });
    HL.swapGlobal(0xb4a20, 0, out, 'geng');
    return out;
  };
  F.s49_wdgen_post = function () { return F.s49_wdgen('post'); };
  F.s49_wdgen_thread = function () { return F.s49_wdgen('thread'); };
  F.s49_wdgen_n = GEN_DEVS.length;

  // fillDecodeRing (rva 0x33300), extra rows: c3fn.js's fdrFixture (section FDR) with the rows below. Row: [+0x58, write cursor offset in
  // the ring, mask, position, end, loop length, loop start, loop table?, +0x13a0, ready bytes in the file buffer, read cursor offset,
  // read end offset]. Key x1 takes "free space <= bytes left", x2 "free space > bytes left"; the rows reach the read-cursor wrap
  // (cursor behind the ring start), the empty-space case (space = ring size), a second ring part cut by the file buffer, and loops
  // with a table with and without bit 7.
  const FDR2 = {
    x1: [
      [0x00, 0x000, 0x1ff, 0x00, 0x400, 0, 0, 0, 0, 0x300, 0x000, 0x1ff],
      [0x00, 0x040, 0x1ff, 0x00, 0x400, 0, 0, 0, 0, 0x300, 0x044, 0x1ff],
      [0x00, 0x1c0, 0x1ff, 0x10, 0x400, 0, 0, 0, 0, 0x060, 0x180, 0x1ff],
      [0x84, 0x000, 0x1ff, 0x10, 0x400, 0x80, 0x20, 1, 0, 0x300, 0x040, 0x1ff],
      [0x04, 0x1c0, 0x1ff, 0x10, 0x400, 0x80, 0x20, 1, 0, 0x300, 0x040, 0x1ff],
      [0x04, 0x000, 0x1ff, 0x10, 0x400, 0x80, 0x20, 0, 0, 0x010, 0x040, 0x1ff],
    ],
    x2: [
      [0x00, 0x000, 0x1ff, 0x3f0, 0x400, 0, 0, 0, 0, 0x300, 0x000, 0x1ff],
      [0x00, 0x1f8, 0x1ff, 0x3f8, 0x400, 0, 0, 0, 0, 0x300, 0x040, 0x1ff],
      [0x00, 0x1f0, 0x1ff, 0x3e0, 0x400, 0, 0, 0, 0, 0x008, 0x040, 0x048],
      [0x00, 0x1f0, 0x1ff, 0x3e0, 0x400, 0, 0, 0, 0, 0x014, 0x040, 0x1ff],
      [0x04, 0x000, 0x1ff, 0x090, 0x400, 0x80, 0x20, 1, 0, 0x300, 0x040, 0x1ff],
      [0x04, 0x000, 0x1ff, 0x090, 0x400, 0x80, 0x20, 0, 0, 0x008, 0x040, 0x1ff],
      [0x04, 0x1f0, 0x1ff, 0x080, 0x400, 0x80, 0x20, 0, 0, 0x300, 0x040, 0x1ff],
      [0x04, 0x1f0, 0x1ff, 0x080, 0x400, 0x80, 0x20, 0, 0, 0x014, 0x040, 0x1ff],
    ],
  };
  function fdr2(rows) {
    return function () {
      const out = {};
      HN.quietTimer();
      const n = rows.length;
      const m = HN.mmio(out, { 'fdr.dat': HN.ramp(0x800, 7, 1, 3) }, { bufSlots: n, bufCap: 0x200 });
      out.objs = HN.block(n * 0x1400, 0x33100000);
      out.rings = HN.block(n * 0x100, 0x7e100000);
      out.rbufs = HN.block(n * 0x200, 0x7d100000);
      for (let j = 0; j < n; j++) {
        const r = rows[j], o = out.objs.add(0x1400 * j), ring = out.rings.add(0x100 * j), rb = out.rbufs.add(0x200 * j);
        out['o' + j] = o;
        const h = m.open('fdr.dat', 0x10000, 0x200);
        const hr = m.handle(h);
        const bufp = hr.add(0x14).readPointer();
        bufp.writeByteArray(m.data('fdr.dat').readByteArray(0x200));
        hr.add(0x24).writeU32(0); hr.add(0x28).writeU32(r[9]); hr.add(0x1c).writeU32(0); hr.add(0x20).writeU32(r[9]);
        HN.u32(o, 0, h);
        HN.u32(o, 0xc + 4, 0x20534f44); HN.u32(o, 0xc + 0x14, 0x200); HN.u32(o, 0xc + 0x18, bufp); HN.u32(o, 0xc + 0x1c, bufp);
        HN.u32(o, 0xc + 0x20, bufp.add(r[9])); HN.u32(o, 0xc + 0x24, bufp.add(0x200)); HN.u32(o, 0xc + 0x28, 0); HN.u32(o, 0xc + 0x2c, r[9]);
        HN.u32(o, 8, ring);
        HN.u32(ring, 0x14, 0x200); HN.u32(ring, 0x18, rb); HN.u32(ring, 0x1c, rb.add(r[10])); HN.u32(ring, 0x20, rb.add(r[11]));
        HN.u32(ring, 0x24, rb.add(0x200));
        HN.u32(o, 0x1378, rb.add(r[1])); HN.u32(o, 0x1374, r[2]); HN.u32(o, 0x170, r[3]); HN.u32(o, 0x178, r[4]);
        HN.u32(o, 0x1368, r[5]); HN.u32(o, 0x136c, r[6]);
        HN.u32(o, 0x1b8, r[7] ? o.add(0x1200) : 0);
        HN.u32(o, 0x13a0, r[8]); HN.u32(o, 0x58, r[0]);
      }
      HN.swapGlobal(0xb4a08, 0x30, out, 'gmmio');
      return out;
    };
  }
  F.s49_fdr_x1 = fdr2(FDR2.x1);
  F.s49_fdr_x2 = fdr2(FDR2.x2);
})();
