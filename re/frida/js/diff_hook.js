// Path-1 A/B: call the ORIGINAL (MinHook trampoline) and the REIMPLEMENTATION (detour) with identical
// inputs inside the live game process and report both results. Requires diff_fixtures.js loaded first.
rpc.exports = {
  // 1 when the SG_HOOK of a DLL spec is installed. jgld.dll and sound.dll are LoadLibrary'd after the window appears
  // (sound.dll ~1-10 s later) and their hooks install then, so diff_hook.py polls this before running DLL keys.
  installed(spec) {
    const shim = Process.getModuleByName('winmm.dll');
    const pd = Memory.alloc(4), po = Memory.alloc(4), pi = Memory.alloc(4);
    const f = new NativeFunction(shim.getExportByName('SimGolfShim_FindHookM'), 'int',
      ['pointer', 'uint32', 'pointer', 'pointer', 'pointer'], 'stdcall');
    return f(Memory.allocUtf8String(spec.module), spec.addr, pd, po, pi) ? pi.readS32() : -1;
  },
  run(spec) {
    const shim = Process.getModuleByName('winmm.dll');
    const pd = Memory.alloc(4), po = Memory.alloc(4), pi = Memory.alloc(4);
    const isExe = (spec.module || 'golf_clean.exe').toLowerCase() === 'golf_clean.exe';
    // DLL RVAs collide across jgld.dll / sound.dll / Terrain.dll: look DLL hooks up by (module, rva)
    const found = isExe
      ? new NativeFunction(shim.getExportByName('SimGolfShim_FindHook'), 'int',
          ['uint32', 'pointer', 'pointer', 'pointer'], 'stdcall')(spec.addr, pd, po, pi)
      : new NativeFunction(shim.getExportByName('SimGolfShim_FindHookM'), 'int',
          ['pointer', 'uint32', 'pointer', 'pointer', 'pointer'], 'stdcall')(Memory.allocUtf8String(spec.module), spec.addr, pd, po, pi);
    if (!found) throw new Error('no SG_HOOK registered at ' + (spec.module || 'golf_clean.exe') + '+0x' + spec.addr.toString(16));
    const installed = pi.readS32() === 1;
    if (!installed) throw new Error('hook registered but not installed (toggled OFF?)');
    const detour = pd.readPointer(), original = po.readPointer();
    // Install witness: the hooked address must start with a JMP (0xE9) written by MinHook.
    const mod = spec.module || 'golf_clean.exe';
    const target = mod.toLowerCase() === 'golf_clean.exe' ? ptr(spec.addr) : Process.getModuleByName(mod).base.add(spec.addr);
    const witness = target.readU8();
    const fx = spec.fixture ? globalThis.DIFF_FIXTURES[spec.fixture]() : {};
    const mk = (p) => new NativeFunction(p, spec.ret, spec.args, spec.abi);
    // override_re: negative control for the test suite only (proves a wrong body reads RED).
    const wrong = spec.override_re === 'null'
      ? new NativeCallback(() => (spec.ret === 'pointer' ? ptr(0) : 0), spec.ret, spec.args, spec.abi)
      : null;
    // State: memory regions [base, offset, size] (base = an address, or "$name" for a fixture pointer) that the
    // function reads or writes. Each vector runs both arms from the same snapshot; the regions are compared
    // after each arm and restored at the end, so stateful functions (RNG, writers) can be A/B'd and the game
    // is left as it was. The game keeps running meanwhile: regions it writes concurrently make a run flaky.
    const regs = (spec.state || []).map((s) => ({
      p: (typeof s[0] === 'string' ? fx[s[0].slice(1)] : ptr(s[0])).add(s[1]), n: s[2] }));
    const snap = () => regs.map((r) => r.p.readByteArray(r.n));
    const restore = (s) => regs.forEach((r, i) => r.p.writeByteArray(s[i]));
    const hex = (bufs) => bufs.map((b) => Array.from(new Uint8Array(b), (x) => x.toString(16).padStart(2, '0')).join('')).join('|');
    const fnv = (s) => { let h = 0x811c9dc5; for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 0x01000193) >>> 0; } return h.toString(16).padStart(8, '0'); };
    // orig_restore_state (test-only negative control): the original's return value with its state change undone.
    const fo0 = mk(original);
    const restoring = spec.override_re === 'orig_restore_state'
      ? new NativeCallback(function (...a) { const s = snap(); const r = fo0(...a); restore(s); return r; }, spec.ret, spec.args, spec.abi)
      : null;
    const fo = fo0, fr = mk(wrong || restoring || (spec.override_re ? ptr(spec.override_re) : detour));
    const resolve = (v) => (typeof v === 'string' && v.startsWith('$')) ? fx[v.slice(1)] : v;
    const norm = (r) => {
      if (spec.ret === 'void') return 'void';
      if (spec.ret !== 'pointer') return r.toString();
      if (r.isNull()) return 'NULL';
      return fx.obj ? 'obj+0x' + r.sub(fx.obj).toString(16) : r.toString();
    };
    const rows = [];
    for (const vec of spec.vectors) {
      // a plain number for a pointer argument (e.g. 0 = NULL) becomes a NativePointer
      const args = vec.map((v, i) => { const x = resolve(v); return spec.args[i] === 'pointer' && typeof x === 'number' ? ptr(x) : x; });
      if (!regs.length) {
        rows.push({ vector: vec, orig: norm(fo(...args)), re: norm(fr(...args)) });
        continue;
      }
      const before = snap();
      const ro = norm(fo(...args)); const so = hex(snap());
      restore(before);
      const rr = norm(fr(...args)); const sr = hex(snap());
      restore(before);
      rows.push({ vector: vec, orig: ro, re: rr, state_orig: fnv(so), state_re: fnv(sr), state_match: so === sr,
                  state_changed: so !== hex(before) });
    }
    return { witness, detour: detour.toString(), original: original.toString(), rows };
  },
};
