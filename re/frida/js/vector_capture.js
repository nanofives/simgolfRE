// Phase 2.5 CAPTURE mode (re/tools/vector_capture.py): runs one registry key's vectors against the ORIGINAL only and
// reports, per key, the data-section image after the fixture, the fixture's memory blocks and, per vector, the
// arguments as resolved, the return value and the exact data-section / block changes. Loaded in place of diff_hook.js
// after the same files (keepalive, the stand-ins, freeze, the fixtures). The game runs with SIMGOLF_HOOKS_OFF=all, so
// the function's address IS the original code and its callees are original too.
//
// Binary payloads go through send(): one 'key' message carrying the image + blocks + pages, sent before the rpc reply.
(function () {
  const DATA = ptr('0x004c1000'), DSIZE = 0x381174, NDW = DSIZE >>> 2;
  const cap = { on: false, blocks: [], uses: new Set() };

  // Fixture blocks: diff_hook.py rewrites `Memory.alloc(` to `__keepAlloc(` in fixture sources; the fixtures look the
  // global up at call time, so this wrapper sees every block a fixture allocates and its size.
  const keep = globalThis.__keepAlloc;
  globalThis.__keepAlloc = function (size, options) {
    const p = keep(size, options);
    if (cap.on) cap.blocks.push([p, Number(size)]);
    return p;
  };
  // Stand-in / freeze use is recorded per key: those keys depend on stood-in heap, imports, files, UI trees or a frozen
  // thread, which a vector of plain memory cannot express (the port's segment corpus covers them).
  const wrap = (obj, name, tag) => {
    if (!obj || typeof obj[name] !== 'function') return;
    const f = obj[name];
    try { obj[name] = function (...a) { if (cap.on) cap.uses.add(tag); return f.apply(this, a); }; } catch (e) { /* read-only */ }
  };
  wrap(globalThis, '__heapStandIn', 'heap_standin');
  wrap(globalThis, '__importStandIn', 'import_standin');
  wrap(globalThis, '__fileStandIn', 'file_standin');
  wrap(globalThis, '__mmioStandIn', 'mmio_standin');
  wrap(globalThis, '__uiTree', 'uitree');
  wrap(globalThis, '__modal', 'modal');
  wrap(globalThis.__freeze, 'thread', 'freeze');
  wrap(globalThis.__recDisplay, 'guard', 'recdisplay');
  // Interceptor's methods are read-only and a Proxy breaks its invariants (a non-configurable property must read the
  // same value): put a plain forwarding object in front of the global the fixtures resolve at call time
  try {
    const I = Interceptor, W = {};
    const names = new Set(Object.getOwnPropertyNames(I));
    const proto = Object.getPrototypeOf(I);
    if (proto) Object.getOwnPropertyNames(proto).forEach((n) => names.add(n));
    names.forEach((k) => {
      if (k === 'constructor') return;
      const v = I[k];
      if (typeof v === 'function') {
        W[k] = function (...a) {
          if (cap.on && (k === 'replace' || k === 'attach' || k === 'replaceFast')) cap.uses.add('interceptor');
          return v.apply(I, a);
        };
      } else {
        Object.defineProperty(W, k, { get: () => I[k] });
      }
    });
    globalThis.Interceptor = W;
  } catch (e) { /* the code-patch scan below still finds every stood-in function */ }
  // Function entries of golf_clean.exe with their first bytes in the file (rpc init): after the fixture, every entry
  // whose bytes differ is a function the fixture stood in (Interceptor.replace / attach write a jump there; with
  // SIMGOLF_HOOKS_OFF=all nothing else patches the exe)
  let entries = [];
  if (globalThis.__frameMask) for (const k of Object.keys(globalThis.__frameMask)) wrap(globalThis.__frameMask, k, 'framemask');

  // Snapshots and dword diffs run natively (a CModule): the data section is 921,693 dwords
  const cm = new CModule(`
#include <stdint.h>
int dwdiff(const uint32_t *a, const uint32_t *b, int n, uint32_t *out, int cap) {
  int k = 0;
  for (int i = 0; i < n; i++)
    if (a[i] != b[i]) { if (k < cap) { out[2 * k] = (uint32_t)i; out[2 * k + 1] = b[i]; } k++; }
  return k;
}
int bdiff(const uint8_t *a, const uint8_t *b, int n, uint32_t *out, int cap) {
  int k = 0;
  for (int i = 0; i < n; i++)
    if (a[i] != b[i]) { if (k < cap) { out[2 * k] = (uint32_t)i; out[2 * k + 1] = b[i]; } k++; }
  return k;
}
unsigned int getcw(void) { unsigned short cw; __asm__ __volatile__("fnstcw %0" : "=m"(cw)); return cw; }
`);
  globalThis.__vectorCaptureCm = cm;   // keep the module referenced: Frida frees an unreferenced CModule's code
  // the FPU control word of this (the A/B) thread, the one the vectors run on
  const readCw = new NativeFunction(cm.getcw, 'uint', []);
  const dwdiffN = new NativeFunction(cm.dwdiff, 'int', ['pointer', 'pointer', 'int', 'pointer', 'int']);
  const bdiffN = new NativeFunction(cm.bdiff, 'int', ['pointer', 'pointer', 'int', 'pointer', 'int']);
  const CAP = 1 << 20;
  const outBuf = Memory.alloc(CAP * 8);
  const snaps = [Memory.alloc(DSIZE), Memory.alloc(DSIZE), Memory.alloc(DSIZE)];
  // flat [index, value, ...] of the dwords that differ between two snapshots (value from b)
  function diffDw(a, b) {
    const k = dwdiffN(a, b, NDW, outBuf, CAP);
    if (k > CAP) throw new Error('more than ' + CAP + ' dwords changed');
    return k ? Array.from(new Uint32Array(outBuf.readByteArray(k * 8))) : [];
  }
  // flat [offset, value, ...] of the bytes that differ between two native buffers (value from b)
  function diffBytes(a, b, n) {
    const k = bdiffN(a, b, n, outBuf, CAP);
    if (k > CAP) throw new Error('more than ' + CAP + ' block bytes changed');
    return k ? Array.from(new Uint32Array(outBuf.readByteArray(k * 8))) : [];
  }
  const isData = (p) => p.compare(DATA) >= 0 && p.compare(DATA.add(DSIZE)) < 0;
  const exe = Process.getModuleByName('golf_clean.exe');
  const inModule = (p) => Process.findModuleByAddress(p) !== null;

  const hexOf = (ab) => Array.from(new Uint8Array(ab), (x) => x.toString(16).padStart(2, '0')).join('');
  rpc.exports = {
    init(list) { entries = list; return entries.length; },
    // pages of the live process the emulation found missing (heap objects reached through data-section pointers)
    pages(list) {
      const out = [];
      for (const a of list) {
        const p = ptr(a);
        const r = Process.findRangeByAddress(p);
        if (!r || r.protection[0] !== 'r') continue;
        const m = Process.findModuleByAddress(p);
        if (m !== null && m.name.toLowerCase() === 'golf_clean.exe') continue;   // the emulation maps the exe itself
        try { out.push({ base: a, hex: hexOf(p.readByteArray(Process.pageSize)), module: m ? m.name : null }); } catch (e) { /* guard page */ }
      }
      return out;
    },
    capture(spec) {
      if (globalThis.__freeze) globalThis.__freeze.begin();
      const target = ptr(spec.addr);
      const firstByte = target.readU8();
      const cw = readCw();
      cap.blocks = []; cap.uses = new Set(); cap.on = true;
      let fx = {};
      let fixtureError = null;
      try {
        fx = spec.fixture ? globalThis.DIFF_FIXTURES[spec.fixture]() : {};
      } catch (e) { fixtureError = String(e); }
      cap.on = false;
      // The game's first thread keeps running the main menu while the vectors run: it changed up to ~1,750 data dwords
      // between two vectors of one key and some of them during a call (computeRatingTrend vector 15, 2026-10-18). Freeze
      // it for the key's vectors unless the fixture already did (__freeze.thread() is not re-entrant); the shared flush
      // resumes it after the rpc returns.
      let frozen = cap.uses.has('freeze') ? 'fixture' : null, freezeError = null;
      if (!frozen && spec.freeze !== false && globalThis.__freeze) {
        try { globalThis.__freeze.thread(); frozen = 'capture'; } catch (e) { freezeError = String(e); }
      }
      let patched = [];
      const scanPatched = () => {
        patched = [];
        for (const [va, h] of entries) if (hexOf(ptr(va).readByteArray(h.length >>> 1)) !== h) patched.push(va);
      };
      scanPatched();
      // UNSTUB variant (vector_capture.py capture --unstub): take the fixture's stand-ins off the listed functions, so
      // the original runs its real callees there; the fixture's own revert at the flush then finds nothing to undo
      const unstubbed = [];
      if (spec.unstub && spec.unstub.length) {
        for (const va of patched) {
          if (spec.unstub.indexOf(va) < 0) continue;
          try { globalThis.Interceptor.revert(ptr(va)); unstubbed.push(va); } catch (e) { /* not an Interceptor patch */ }
        }
        globalThis.Interceptor.flush();
        scanPatched();
      }
      const fxNames = {};
      for (const [k, v] of Object.entries(fx || {})) if (v instanceof NativePointer) fxNames[k] = v.toUInt32();
      const blocks = cap.blocks.map(([p, n]) => ({ base: p.toUInt32(), size: n }));
      const resolve = (v) => (typeof v === 'string' && v.startsWith('$')) ? fx[v.slice(1)] : v;
      // pages behind pointers that are neither the data section, a module nor a fixture block: strings made with
      // Memory.allocUtf8String and similar (Memory.alloc* other than alloc is not rewritten), 2 pages each
      const inBlock = (p) => blocks.some((b) => p.compare(ptr(b.base)) >= 0 && p.compare(ptr(b.base).add(b.size)) < 0);
      const pageSet = new Map();
      const pageSeen = new Set();            // page bases already judged: arenas repeat the same values many times
      const notePtr = (p) => {
        if (!(p instanceof NativePointer) || p.compare(ptr(0x10000)) < 0) return;
        const pk = p.toUInt32() >>> 12;
        if (pageSeen.has(pk)) return;
        pageSeen.add(pk);
        if (isData(p) || inBlock(p) || inModule(p)) return;
        const base = p.and(ptr(Process.pageSize - 1).not());
        for (const a of [base, base.add(Process.pageSize)]) {
          if (pageSet.has(a.toUInt32())) continue;
          const r = Process.findRangeByAddress(a);
          if (r && r.protection[0] === 'r') { try { pageSet.set(a.toUInt32(), a.readByteArray(Process.pageSize)); } catch (e) { } }
        }
      };
      for (const vec of spec.vectors) vec.forEach((v, i) => { const x = resolve(v); if (spec.args[i] === 'pointer') notePtr(typeof x === 'number' ? ptr(x) : x); });
      for (const b of blocks) {
        const n = b.size >>> 2;
        for (let i = 0; i < n; i++) notePtr(ptr(ptr(b.base).add(4 * i).readU32()));
      }
      Memory.copy(snaps[0], DATA, DSIZE);                  // the key image (after the fixture)
      const img = snaps[0].readByteArray(DSIZE);
      const blockBytes = blocks.map((b) => ptr(b.base).readByteArray(b.size));
      const pages = Array.from(pageSet.entries()).map(([a, buf]) => ({ base: a, size: buf.byteLength, buf }));
      // one binary message: image, then blocks, then pages
      const total = DSIZE + blockBytes.reduce((n, b) => n + b.byteLength, 0) + pages.reduce((n, p) => n + p.size, 0);
      const blob = new Uint8Array(total);
      let o = 0;
      blob.set(new Uint8Array(img), o); o += DSIZE;
      for (const b of blockBytes) { blob.set(new Uint8Array(b), o); o += b.byteLength; }
      for (const p of pages) { blob.set(new Uint8Array(p.buf), o); o += p.size; }
      send({ type: 'key', key: spec.key, blocks, pages: pages.map((p) => ({ base: p.base, size: p.size })) }, blob.buffer);
      if (fixtureError) return { firstByte, cw, uses: Array.from(cap.uses), fxNames, fixtureError, patched, unstubbed, frozen, freezeError, rows: [] };

      const fo = new NativeFunction(target, spec.ret, spec.args, spec.abi);
      // restore skip mask: dwords of pointer-class objects (the game's own heap pointers are never reverted)
      const skip = new Uint8Array(NDW);
      for (const [off, n] of spec.noRestore || []) for (let i = off >>> 2; i < (off + n + 3) >>> 2 && i < NDW; i++) skip[i] = 1;
      const rows = [];
      const blkSnap = blocks.map((b) => Memory.alloc(Math.max(b.size, 1)));   // released with this rpc's scope
      let prev = snaps[0], pre = snaps[1], post = snaps[2];
      for (let vi = 0; vi < spec.vectors.length; vi++) {
        const vec = spec.vectors[vi];
        const args = vec.map((v, i) => { const x = resolve(v); return spec.args[i] === 'pointer' && typeof x === 'number' ? ptr(x) : x; });
        const argOut = args.map((a) => (a instanceof NativePointer) ? { p: a.toUInt32() } : (typeof a === 'number' ? { n: a } : { s: String(a) }));
        Memory.copy(pre, DATA, DSIZE);
        const drift = diffDw(prev, pre);
        blocks.forEach((b, i) => Memory.copy(blkSnap[i], ptr(b.base), b.size));
        let ret = null, err = null;
        try {
          const r = fo(...args);
          if (spec.ret === 'void') ret = null;
          else if (r instanceof NativePointer) ret = r.toUInt32();
          else if (typeof r === 'number' || typeof r === 'boolean') ret = (typeof r === 'boolean') ? (r ? 1 : 0) : r;
          else ret = r.toString();                       // Int64 / UInt64
          if (typeof ret === 'number' && !Number.isFinite(ret)) ret = String(ret);   // NaN / Infinity in JSON
        } catch (e) { err = String(e); }
        Memory.copy(post, DATA, DSIZE);
        const dd = diffDw(pre, post);
        const bd = blocks.map((b, i) => diffBytes(blkSnap[i], ptr(b.base), b.size));
        // restore: data dwords the call changed (except pointer-class objects), then every block
        for (let k = 0; k < dd.length; k += 2) {
          const i = dd[k];
          if (!skip[i]) DATA.add(4 * i).writeU32(pre.add(4 * i).readU32());
        }
        blocks.forEach((b, i) => { if (bd[i].length) Memory.copy(ptr(b.base), blkSnap[i], b.size); });
        const t = prev; prev = pre; pre = t;
        rows.push({ i: vi, args: argOut, ret, err, drift, dataDiff: dd, blockDiff: bd });
      }
      const modules = Process.enumerateModules().map((m) => ({ name: m.name, base: m.base.toUInt32(), size: m.size }));
      return { firstByte, cw, uses: Array.from(cap.uses), fxNames, fixtureError: null, patched, unstubbed, frozen, freezeError, modules, rows };
    },
  };
})();
