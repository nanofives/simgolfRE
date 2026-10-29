// S4.8 fixtures for the emu-only rows that have no registry key and no instruction-identical twin (port/test/sound_replay.py
// CUSTOM). Evaluated into each key's fixture-VM context after diff_hook.py's scripts (sound_replay's KeyRun.setup); they use the
// same mock Frida API as re/frida/js/fixtures.d/*.js. The COM methods are machine-code stubs run by the emulator (no
// NativeCallback), so both arms reach the same code.
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

  let code = null;
  function stubs() {
    if (code) return code;
    const page = Memory.alloc(Process.pageSize);
    // +0x10 SetVolume (stdcall this, vol): log vol at [this+8+4*k] (k = [this+4], 0..7), k = (k+1) & 7, return [this+0x28]
    page.add(0x10).writeByteArray([0x8b, 0x44, 0x24, 0x04, 0x8b, 0x48, 0x04, 0x8b, 0x54, 0x24, 0x08, 0x89, 0x54, 0x88, 0x08,
                                   0x41, 0x83, 0xe1, 0x07, 0x89, 0x48, 0x04, 0x8b, 0x40, 0x28, 0xc2, 0x08, 0x00]);
    Memory.protect(page, Process.pageSize, 'rwx');
    const comVt = fill(Memory.alloc(0x100), 0x100, 0);
    comVt.add(0x3c).writePointer(page.add(0x10));
    code = { comVt: comVt };
    return code;
  }
  function comBuffer(ret) {
    const c = fill(Memory.alloc(0x40), 0x40, 0);
    c.writePointer(stubs().comVt);
    c.add(0x28).writeU32(ret >>> 0);
    return c;
  }

  // DirectSoundDevice::setMasterVolume (rva 0x13560): a device and four DsBuffers on the module's list (head rva 0xb4a34,
  // tail 0xb4a38; prev +0xbc, next +0xc0). Buffer k: master +0x5c = the device except b2 (NULL: setVolume uses 0x7f),
  // interface +0x60 = a recording SetVolume stub except b3 (NULL: setVolume returns the volume). The list globals are
  // saved and restored at the flush.
  function masterVol(empty) {
    __freeze.thread();
    const fx = { dev: block(0x200, 0x5348), gl: snd(0xb4a34) };
    fx.dev.add(0x188).writeU32(0x33);
    const bs = [];
    for (let k = 0; k < 4; k++) {
      const b = block(0xd4, 0x5350 + k);
      const c = comBuffer([0, 0x88780096, 7, 0][k]);
      b.add(0x5c).writePointer(k === 2 ? ptr(0) : fx.dev);
      b.add(0x60).writePointer(k === 3 ? ptr(0) : c);
      fx['b' + k] = b;
      fx['c' + k] = c;
      bs.push(b);
    }
    for (let k = 0; k < 4; k++) {
      bs[k].add(0xbc).writePointer(k ? bs[k - 1] : ptr(0));
      bs[k].add(0xc0).writePointer(k < 3 ? bs[k + 1] : ptr(0));
    }
    __freeze.save(snd(0xb4a34), 8);
    __freeze.setU32(snd(0xb4a34), empty ? 0 : bs[0].toUInt32());
    __freeze.setU32(snd(0xb4a38), empty ? 0 : bs[3].toUInt32());
    return fx;
  }
  F.s48_mastervol = function () { return masterVol(false); };
  F.s48_mastervol_empty = function () { return masterVol(true); };
  // postMidiNotification (rva 0x20b60): the c3dh_postmidi objects and import stand-ins without c3dh_waitengine (which waits
  // for the live game's sound engine): g_soundEngine (rva 0xb4a20) is set by the fixture instead, to an engine object whose
  // flag word +0x1a4 has bit 5 (`eng`), lacks it (`eng0`), or to NULL (`noeng`; the sound-thread post then runs, and the
  // thread state's flag byte at rva 0xb5038 decides it: 0 after DllMain, set to 1 by `noeng1` so PostThreadMessageA runs).
  function postMidi(engine, threadFlag) {
    const D = globalThis.DIFF_FIXTURES;
    const out = D.c3dh_postmidi_objects();
    __freeze.thread();
    __freeze.save(snd(0xb4a20), 4);
    __freeze.save(snd(0xb5038), 0x30);
    let eng = ptr(0);
    if (engine !== null) {
      eng = block(0x200, 0x5360);
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
  F.c3dh_postmidi_objects = function () {
    const out = {};
    [[0, 0], [0x100, 0], [0xff, 0x10], [0x1ff, 0], [0xfffffeff, 0xffff0000], [0x100, 0xffffffff], [0, 0xffffffef],
     [0x80, 0x5]].forEach(function (r, i) {
      const o = Memory.alloc(0x220);
      for (let k = 0; k < 0x220; k += 4) o.add(k).writeU32((0x4e000000 + 0x1000 * i + k) >>> 0);
      o.add(0xac).writeU32(r[0] >>> 0);
      o.add(0x210).writeU32((0x6000 + i) >>> 0);
      o.add(0x214).writeU32(r[1] >>> 0);
      out['m' + i] = o;
    });
    return out;
  };
  F.s48_postmidi_eng = function () { return postMidi(0x20, 0); };
  F.s48_postmidi_eng0 = function () { return postMidi(0xffffffdf, 0); };
  F.s48_postmidi_noeng = function () { return postMidi(null, 0); };
  F.s48_postmidi_noeng1 = function () { return postMidi(null, 1); };
  // delete_sound (rva 0x97d0, export; emu-only, no registry key): sound objects s0..s8 (flag dword +0x44, a recording vtable
  // whose slot 0 logs the scalar-delete call and slot +0x78 returns the object's answer at +0x100, the sub-object at +0xf8,
  // the 0x80 record pointers at +0x744), the globals g_soundEngine (rva 0xb4a20), g_soundTimer (0xb49ec, byte +0x3c) and
  // g_midiDevice (0xb4a1c) per key variant, and recording stand-ins (Interceptor.replace on the bodies) for the callees other
  // S4 steps own: WaveCmdQueue::pushNode 0xc2f0 / pushValue 0xc3d0, removeSoundNode 0xe050, Voice_setLoopFlag 0x37760,
  // pairStore 0x8810. CommandQueue::push 0xa240 (this step's) runs; its node comes from a heap stand-in arena.
  const DLOGSZ = 0x800;
  function dlog(log, tag, args) {
    const n = log.readU32();
    if (4 + 0x20 * (n + 1) <= DLOGSZ) {
      const p = log.add(4 + 0x20 * n);
      p.writeU32(tag >>> 0);
      for (let i = 0; i < args.length && i < 7; i++) p.add(4 + 4 * i).writeU32(ptr(args[i]).toUInt32());
    }
    log.writeU32(n + 1);
  }
  function stub(log, rva, nargs, ret) {
    const base = snd(rva);
    const argt = ['pointer'].concat(Array(nargs).fill('pointer'));
    const cb = new NativeCallback(function () {
      dlog(log, rva, Array.prototype.slice.call(arguments));
      return ret >>> 0;
    }, 'uint32', argt, 'thiscall');
    Interceptor.replace(base, cb);
    Interceptor.flush();
    __freeze.atFlush(function () { Interceptor.revert(base); Interceptor.flush(); });
  }
  function recObj(log, size, tag, ans78) {
    const o = block(size, tag);
    const vtb = fill(Memory.alloc(0x100), 0x100, 0);
    vtb.writePointer(new NativeCallback(function (self, x) { dlog(log, 0x5000, [self, x]); return 0; },
                                        'uint32', ['pointer', 'uint32'], 'thiscall'));
    vtb.add(0x78).writePointer(new NativeCallback(function (self) { dlog(log, 0x5078, [self]); return self.add(0x100).readU32(); },
                                                  'uint32', ['pointer'], 'thiscall'));
    o.writePointer(vtb);
    o.add(0x100).writeU32(ans78 >>> 0);
    return o;
  }
  function delSound(engine, timerBit, midi) {
    __freeze.thread();
    const fx = { log: fill(Memory.alloc(DLOGSZ), DLOGSZ, 0) };
    const log = fx.log;
    stub(log, 0xc2f0, 2, 0);
    stub(log, 0xc3d0, 1, 0);
    stub(log, 0xe050, 1, 0);
    stub(log, 0x37760, 1, 0);
    stub(log, 0x8810, 2, 0x5e1000);
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x200 });
    fx.arena = h.arena;
    __freeze.save(snd(0xb4a20), 4);
    __freeze.save(snd(0xb49ec), 4);
    __freeze.save(snd(0xb4a1c), 4);
    fx.tmr = block(0x60, 0x5370);
    fx.tmr.add(0x3c).writeU8(timerBit ? 4 : 0x0b);
    fx.eng = block(0x200, 0x5371);
    fx.midi = block(0x4060, 0x5372);
    fx.midi.add(0x50 + 0x4000).writeU32(7);          // CommandQueue at +0x50: read index +0x4050, write index +0x4054
    fx.midi.add(0x50 + 0x4004).writeU32(3);
    __freeze.setU32(snd(0xb4a20), engine ? fx.eng.toUInt32() : 0);
    __freeze.setU32(snd(0xb49ec), fx.tmr.toUInt32());
    __freeze.setU32(snd(0xb4a1c), midi ? fx.midi.toUInt32() : 0);
    // s0: bit 2; s1: bit 5 with records; s2: neither, slot 0x78 -> 8; s3: neither, 0x78 -> 0; s4: neither, 0x78 -> 8, sub with
    // bit 6; s5: as s4, sub without bit 6; s6: as s4, no sub; s7: bits 2 and 5 (bit 2 wins); s8: bit 5 with no records
    const SPEC = [[4, 0, 0], [0x20, 0, 0], [0, 8, 0], [0, 0, 0], [0x10, 0xf8, 0x40], [0, 8, 1], [0, 8, 2], [0x24, 0, 0],
                  [0x20, 0, 0]];
    SPEC.forEach(function (sp, i) {
      const s = recObj(log, 0x960, 0x5380 + i, sp[1]);
      s.add(0x44).writeU32(sp[0] >>> 0);
      for (let k = 0; k < 0x80; k++) s.add(0x744 + 4 * k).writeU32(0);
      if (i === 1) {
        for (let k = 0; k < 0x80; k += 9) {
          const rec = block(0x40, 0x5390 + k);
          if (k % 2 === 0) {
            const o = block(0x220, 0x53a0 + k);
            o.add(0x210).writeU32(k + 2);
            rec.add(0x30).writePointer(o);
            fx['o' + k] = o;
          } else {
            rec.add(0x30).writeU32(0);
          }
          s.add(0x744 + 4 * k).writePointer(rec);
        }
        // the last record (index 0x7f) too
        const rec = block(0x40, 0x53f0);
        const o = block(0x220, 0x53f1);
        o.add(0x210).writeU32(0x55);
        rec.add(0x30).writePointer(o);
        s.add(0x744 + 4 * 0x7f).writePointer(rec);
        fx.olast = o;
      }
      if (i === 4 || i === 5) {
        const sub = recObj(log, 0x200, 0x53c0 + i, 0);
        sub.add(0xc8).writeU32(i === 4 ? 0x45 : 0x5);
        s.add(0xf8).writePointer(sub);
        fx['sub' + i] = sub;
      } else {
        s.add(0xf8).writeU32(0);
      }
      fx['s' + i] = s;
    });
    return fx;
  }
  F.s48_delsound_ready = function () { return delSound(true, true, true); };
  F.s48_delsound_notimer = function () { return delSound(true, false, true); };
  F.s48_delsound_none = function () { return delSound(false, false, false); };
})();
