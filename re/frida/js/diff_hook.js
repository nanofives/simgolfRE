// Path-1 A/B: call the ORIGINAL (MinHook trampoline) and the REIMPLEMENTATION (detour) with identical
// inputs inside the live game process and report both results. Requires diff_fixtures.js loaded first.
rpc.exports = {
  run(spec) {
    const shim = Process.getModuleByName('winmm.dll');
    const find = new NativeFunction(shim.getExportByName('SimGolfShim_FindHook'), 'int',
      ['uint32', 'pointer', 'pointer', 'pointer'], 'stdcall');
    const pd = Memory.alloc(4), po = Memory.alloc(4), pi = Memory.alloc(4);
    if (!find(spec.addr, pd, po, pi)) throw new Error('no SG_HOOK registered at 0x' + spec.addr.toString(16));
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
    const fo = mk(original), fr = mk(wrong || (spec.override_re ? ptr(spec.override_re) : detour));
    const resolve = (v) => (typeof v === 'string' && v.startsWith('$')) ? fx[v.slice(1)] : v;
    const norm = (r) => {
      if (spec.ret !== 'pointer') return r.toString();
      if (r.isNull()) return 'NULL';
      return fx.obj ? 'obj+0x' + r.sub(fx.obj).toString(16) : r.toString();
    };
    const rows = [];
    for (const vec of spec.vectors) {
      const args = vec.map(resolve);
      rows.push({ vector: vec, orig: norm(fo(...args)), re: norm(fr(...args)) });
    }
    return { witness, detour: detour.toString(), original: original.toString(), rows };
  },
};
