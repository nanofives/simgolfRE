// S4.10 fixtures for the exe sound layer's rows with no C3 key (the C2 Snd / VoiceRx forwarders, the group helpers,
// HashTable::remove, the device holders): evaluated into each custom key's VM context (port/test/audio_stream.py CUSTOM).
// Only synthetic values; the exe globals written here (g_soundReady / g_soundFns / g_soundHead / g_soundTail and the
// group table of g_musicDevice) are saved through __freeze and restored at its flush.
(function () {
  function keep(x) { globalThis.__diffKeepAlive.push(x); return x; }
  function fill(p, n, v) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(v >>> 0); }
  function alloc(n, v) { const p = Memory.alloc(n); fill(p, n, v === undefined ? 0 : v); return p; }
  function push(rec, v) {
    let h = rec.readU32();
    h = (Math.imul(h, 33) + (v >>> 0)) >>> 0;
    rec.writeU32(h);
    rec.add(4).writeU32((rec.add(4).readU32() + 1) >>> 0);
    rec.add(8).writeU32(v >>> 0);
  }
  // stack arguments of every device slot the forwarders call (thiscall, `this` = the device)
  const SLOT_ARGS = { 0x04: 1, 0x10: 2, 0x14: 0, 0x18: 1, 0x1c: 0, 0x20: 0, 0x28: 0, 0x30: 0, 0x34: 3, 0x38: 0, 0x3c: 0,
    0x40: 1, 0x44: 1, 0x48: 1, 0x4c: 1, 0x54: 1, 0x58: 0, 0x5c: 0, 0x60: 0, 0x6c: 1, 0x70: 0, 0x74: 0, 0x84: 3, 0x88: 0,
    0x8c: 1, 0x90: 0, 0x94: 1, 0x98: 1, 0x9c: 1, 0xb8: 1, 0xbc: 0, 0xc0: 0, 0xc4: 3, 0xc8: 0, 0xcc: 0, 0xd0: 1, 0xd4: 1,
    0xd8: 3, 0xdc: 1, 0xe0: 1, 0xe4: 1, 0xe8: 0, 0xec: 1, 0xf0: 2 };

  // a device object of `id` whose every slot logs (slot, id, arguments) and returns a value made of them
  function devEnv() {
    const rec = alloc(0x40);
    const vt = alloc(0x100);
    Object.keys(SLOT_ARGS).forEach(function (k) {
      const slot = Number(k), n = SLOT_ARGS[k];
      const args = ['pointer'];
      for (let i = 0; i < n; i++) args.push('uint32');
      vt.add(slot).writePointer(keep(new NativeCallback(function (t) {
        const id = t.add(4).readU32();
        push(rec, ((slot << 16) | (id & 0xffff)) >>> 0);
        let r = (slot * 0x101 + id) >>> 0;
        for (let i = 1; i < arguments.length; i++) { push(rec, arguments[i] >>> 0); r = (r * 3 + (arguments[i] >>> 0)) >>> 0; }
        return r;
      }, 'uint32', args, 'thiscall')));
    });
    return { rec: rec, vt: vt, dev: function (id) { const d = alloc(0x20); d.writePointer(vt); d.add(4).writeU32(id); return d; } };
  }

  // the exe globals the forwarders and VoiceRx::start read: saved and restored by __freeze
  function globals(e, opts) {
    const ready = ptr(0x0083afc4), fns = ptr(0x0083af6c);
    __freeze.save(ptr(0x0083af64), 0x64);
    __freeze.save(ptr(0x0083ad80), 0x1e4);
    ready.writeU32(opts.ready ? 1 : 0);
    // g_soundFns[0] (createSound4840e0): writes a new device into *a and answers by the third argument's low bit;
    // g_soundFns[1] (releaseSoundObj): logs its argument, answers 0
    // (the next device's index lives in the compared log block, +0x10, so both arms hand out the same device)
    const made = [];
    for (let i = 0; i < 16; i++) made.push(e.dev(900 + i));
    fns.writePointer(keep(new NativeCallback(function (out, b, c) {
      push(e.rec, 0xc0de0000 | (c & 0xffff));
      const k = e.rec.add(0x10).readU32();
      e.rec.add(0x10).writeU32((k + 1) >>> 0);
      const d = made[k % made.length];
      out.writePointer(d);
      return (b & 1) ? 7 : 0;
    }, 'uint32', ['pointer', 'uint32', 'uint32'])));
    fns.add(4).writePointer(keep(new NativeCallback(function (p) { push(e.rec, 0xde1e0000); push(e.rec, p.isNull() ? 0 : p.add(4).readU32()); return 0; },
      'uint32', ['pointer'])));
    for (let g = 0; g < 16; g++) {
      const r = ptr(0x0083ad80).add(0x24 + g * 0x1c);
      r.writeU8(g % 3 === 0 ? 0 : 1);          // enabled
      r.add(1).writeU8(g % 5 === 0 ? 1 : 0);   // muted
      r.add(4).writeU32(g * 17 + 3);           // gain
    }
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    // twelve Snd records (0x80 each, pre-filled 0x4d): device at +0x40 null in records 3, 7, 11; own table the exe's Snd
    // or VoiceRx table (records with an odd index); group +0x3c below 0x10 in records 0..5, 0x10 after; +0x38 zero in 2, 6
    s410_snd(ready) {
      __freeze.thread();
      const e = devEnv();
      globals(e, { ready: ready !== false });
      const fx = { rec: e.rec };
      fx.sblk = alloc(12 * 0x80, 0x4d4d4d4d);
      for (let i = 0; i < 12; i++) {
        const s = fx.sblk.add(i * 0x80);
        s.writePointer(ptr(i % 2 ? 0x004badf8 : 0x004bacf8));
        s.add(0x40).writePointer(i % 4 === 3 ? ptr(0) : e.dev(i + 1));
        s.add(0x3c).writeU32(i < 6 ? i * 3 : 0x10);
        s.add(0x38).writeU32(i % 4 === 2 ? 0 : 1000 + i);
        s.add(0x44).writeU32(i % 3 === 0 ? 0x202 : 0x10);
        s.add(0x48).writeU32(0);
        s.add(0x4c).writeU32(0);
        s.add(0x50).writeU32(0);
        fx['s' + i] = s;
      }
      fx.scr = alloc(0x40, 0x5a5a5a5a);
      fx.gl = ptr(0x0083af64);
      fx.md = ptr(0x0083ad80);
      return fx;
    },
    s410_snd_ready() { return this.s410_snd(true); },
    s410_snd_noready() { return this.s410_snd(false); },
    // NodeList::playChildSuspendingSounds: the twelve records linked from g_soundHead (prev +0x48 / next +0x4c), a list
    // object whose device (+0x14) logs its slot 0x1c call (one stack argument), and one with no device
    s410_nodelist() {
      const fx = this.s410_snd(true);
      for (let i = 0; i < 12; i++) {
        const s = fx['s' + i];
        s.add(0x48).writePointer(i ? fx['s' + (i - 1)] : ptr(0));
        s.add(0x4c).writePointer(i < 11 ? fx['s' + (i + 1)] : ptr(0));
        s.add(0x50).writeU32(i % 2 ? 0 : 0x80 + i);
      }
      ptr(0x0083af68).writePointer(fx.s0);
      ptr(0x0083af64).writePointer(fx.s11);
      const rec = fx.rec;
      const vt = alloc(0x40);
      vt.add(0x1c).writePointer(keep(new NativeCallback(function (t, a) {
        push(rec, 0x1c1c0000 | (t.add(4).readU32() & 0xffff));
        push(rec, a >>> 0);
        return (a * 7 + 1) >>> 0;
      }, 'uint32', ['pointer', 'uint32'], 'thiscall')));
      const dev = alloc(0x20);
      dev.writePointer(vt);
      dev.add(4).writeU32(0x77);
      fx.nl = alloc(0x20, 0x31313131);
      fx.nl.add(0x14).writePointer(dev);
      fx.nl0 = alloc(0x20, 0x32323232);
      fx.nl0.add(0x14).writeU32(0);
      return fx;
    },
    // the exit-time destructors over the exe's own globals: `plain` leaves the file image's zeroed g_sounds and device
    // lists; otherwise g_sounds records 0..11 are made live like s410_snd's (devices from the logging environment)
    s410_exit(plain) {
      __freeze.thread();
      const e = devEnv();
      if (!plain) globals(e, { ready: true });   // the group table it writes into g_musicDevice stays out of the plain image
      __freeze.save(ptr(0x0080d840), 0x7e90);
      __freeze.save(ptr(0x0083ad58), 0x20);
      __freeze.save(ptr(0x0083af98), 0x20);
      if (!plain) {
        for (let i = 0; i < 12; i++) {
          const s = ptr(0x0080d840).add(i * 0x6c);
          s.writePointer(ptr(0x004bacf8));
          s.add(0x40).writePointer(i % 4 === 3 ? ptr(0) : e.dev(i + 1));
          s.add(0x3c).writeU32(i < 6 ? i * 3 : 0x10);
          s.add(0x38).writeU32(1000 + i);
          s.add(0x44).writeU32(i % 3 === 0 ? 0x202 : 0x10);
        }
      }
      return { rec: e.rec, gl: ptr(0x0083af64), md: ptr(0x0083ad80) };
    },
    s410_exit_plain() { return this.s410_exit(true); },
    s410_exit_live() { return this.s410_exit(false); },
  });
})();
