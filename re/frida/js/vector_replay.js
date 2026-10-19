// Phase 2.5 validation (re/tools/vector_validate.py live): feeds captured corpus vectors to the REIMPLEMENTATION (the
// SG_HOOK detour in the shim) inside the live game and reports its return value, the bytes of every expected output
// region and the data-section dwords it changed outside them. Loaded after keepalive.js and freeze.js. Every vector's
// changes are undone before the next one (pointer-class objects excepted, as in capture).
(function () {
  const DATA = ptr('0x004c1000'), DSIZE = 0x381174, NDW = DSIZE >>> 2;
  const cm = new CModule(`
#include <stdint.h>
int dwdiff(const uint32_t *a, const uint32_t *b, int n, uint32_t *out, int cap) {
  int k = 0;
  for (int i = 0; i < n; i++)
    if (a[i] != b[i]) { if (k < cap) { out[2 * k] = (uint32_t)i; out[2 * k + 1] = b[i]; } k++; }
  return k;
}
`);
  globalThis.__vectorReplayCm = cm;   // keep the module referenced: Frida frees an unreferenced CModule's code
  const dwdiffN = new NativeFunction(cm.dwdiff, 'int', ['pointer', 'pointer', 'int', 'pointer', 'int']);
  const CAP = 1 << 20;
  const outBuf = Memory.alloc(CAP * 8);
  const snapA = Memory.alloc(DSIZE), snapB = Memory.alloc(DSIZE), snapK = Memory.alloc(DSIZE);
  function diffDw(a, b) {
    const k = dwdiffN(a, b, NDW, outBuf, CAP);
    if (k > CAP) throw new Error('more than ' + CAP + ' dwords changed');
    return k ? Array.from(new Uint32Array(outBuf.readByteArray(k * 8))) : [];
  }
  const hexToBytes = (h) => { const a = new Uint8Array(h.length >>> 1); for (let i = 0; i < a.length; i++) a[i] = parseInt(h.substr(2 * i, 2), 16); return a.buffer; };
  const toHex = (ab) => Array.from(new Uint8Array(ab), (x) => x.toString(16).padStart(2, '0')).join('');

  rpc.exports = {
    replay(spec) {
      if (globalThis.__freeze) globalThis.__freeze.begin();
      const shim = Process.getModuleByName('winmm.dll');
      const pd = Memory.alloc(4), po = Memory.alloc(4), pi = Memory.alloc(4);
      const found = new NativeFunction(shim.getExportByName('SimGolfShim_FindHook'), 'int',
        ['uint32', 'pointer', 'pointer', 'pointer'], 'stdcall')(spec.addr, pd, po, pi);
      if (!found) throw new Error('no SG_HOOK at 0x' + spec.addr.toString(16));
      if (pi.readS32() !== 1) throw new Error('hook not installed');
      const witness = ptr(spec.addr).readU8();
      const target = spec.arm === 'original' ? po.readPointer() : pd.readPointer();
      let release = null;
      if (spec.freeze && globalThis.__freeze) release = globalThis.__freeze.thread();
      // blocks, relocated to where this process put them
      const blocks = spec.blocks.map((h) => { const n = Math.max(h.length >>> 1, 1); const p = __keepAlloc(n + 16); p.writeByteArray(hexToBytes(h)); return p; });
      const addrOf = (l) => ('block' in l) ? blocks[l.block].add(l.off) : ptr(l.va);
      for (const r of spec.relocs) addrOf(r.at).writeU32(blocks[r.to.block].add(r.to.off).toUInt32());
      const blockCopies = blocks.map((p, i) => p.readByteArray(Math.max(spec.blocks[i].length >>> 1, 1)));
      const writeRegions = (regs) => { for (const [va, h] of regs) ptr(va).writeByteArray(hexToBytes(h)); };
      Memory.copy(snapK, DATA, DSIZE);
      const skip = new Uint8Array(NDW);
      for (const [off, n] of spec.noRestore || []) for (let i = off >>> 2; i < (off + n + 3) >>> 2 && i < NDW; i++) skip[i] = 1;
      const fr = new NativeFunction(target, spec.ret, spec.args, spec.abi);
      const rows = [];
      try {
        for (const v of spec.vectors) {
          writeRegions(spec.seed);
          writeRegions(v.in);
          // relocations that sit in data seed bytes are rewritten after the seed
          for (const r of spec.relocs) if (!('block' in r.at)) addrOf(r.at).writeU32(blocks[r.to.block].add(r.to.off).toUInt32());
          Memory.copy(snapA, DATA, DSIZE);
          const args = v.args.map((a, i) => {
            if ('block' in a) return blocks[a.block].add(a.off);
            if ('va' in a) return ptr(a.va);
            if ('int64' in a) return spec.args[i] === 'uint64' ? uint64(a.int64) : int64(a.int64);
            return spec.args[i] === 'pointer' ? ptr(a.n) : a.n;
          });
          let ret = null, err = null;
          try {
            const r = fr(...args);
            if (spec.ret === 'void') ret = null;
            else if (r instanceof NativePointer) {
              const u = r.toUInt32();
              const bi = blocks.findIndex((p, i) => u >= p.toUInt32() && u < p.toUInt32() + Math.max(spec.blocks[i].length >>> 1, 1));
              ret = bi >= 0 ? { block: bi, off: u - blocks[bi].toUInt32() } : { va: u };
            } else if (typeof r === 'number' || typeof r === 'boolean') ret = typeof r === 'boolean' ? (r ? 1 : 0) : r;
            else ret = r.toString();
            if (typeof ret === 'number' && !Number.isFinite(ret)) ret = String(ret);
          } catch (e) { err = String(e); }
          Memory.copy(snapB, DATA, DSIZE);
          const out = v.out.map((o) => toHex(('block' in o ? blocks[o.block].add(o.off) : ptr(o.va)).readByteArray(o.n)));
          const dd = diffDw(snapA, snapB);
          const changed = [];
          for (let k = 0; k < dd.length; k += 2) changed.push(0x004c1000 + 4 * dd[k]);
          // undo: the call's changes and the seed / in bytes, back to the key's state before this vector
          for (let k = 0; k < dd.length; k += 2) { const i = dd[k]; if (!skip[i]) DATA.add(4 * i).writeU32(snapA.add(4 * i).readU32()); }
          const back = diffDw(snapA, snapK);
          for (let k = 0; k < back.length; k += 2) { const i = back[k]; if (!skip[i]) DATA.add(4 * i).writeU32(back[k + 1]); }
          blocks.forEach((p, i) => p.writeByteArray(blockCopies[i]));
          rows.push({ ret, err, out, changed: changed.slice(0, 4096), nchanged: changed.length });
        }
      } finally {
        if (release) release();
      }
      return { witness, rows, bases: blocks.map((p) => p.toUInt32()) };
    },
  };
})();
