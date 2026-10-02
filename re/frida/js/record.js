// Record real calls of one function for offline replay (re/frida/replay.py).
// For each call: ecx/edx, N stack args, the live return value, and a copy of the memory pages around
// every pointer-valued input (ecx and args that land in mapped memory). Pages are deduplicated by
// content on the Python side. The hook under test must be OFF so the ORIGINAL runs live.
let cfg = null, calls = 0, sent = 0;
const PAGE = 0x1000;

function pagesFor(p) {
  // The page holding p and the next one (objects that straddle a boundary).
  const out = [];
  if (p.isNull()) return out;
  const base = p.and(ptr(PAGE - 1).not());
  for (const a of [base, base.add(PAGE)]) {
    const r = Process.findRangeByAddress(a);
    if (r && r.protection.indexOf('r') === 0) {
      try { out.push([a.toUInt32(), a.readByteArray(PAGE)]); } catch (e) { /* guard page */ }
    }
  }
  return out;
}

function attach(target) {
  Interceptor.attach(target, {
    onEnter(args) {
      calls++;
      if (sent >= cfg.max || (calls - 1) % cfg.every !== 0) { this.skip = true; return; }
      const ctx = this.context;
      const sp = ctx.esp;
      const stack = [];
      for (let i = 0; i < cfg.nargs; i++) stack.push(sp.add(4 + 4 * i).readU32());
      const ptrs = [ctx.ecx, ctx.edx].concat(stack.map(v => ptr(v)));
      const pages = new Map();
      for (const p of ptrs) for (const [a, buf] of pagesFor(p)) pages.set(a, buf);
      this.rec = { ecx: ctx.ecx.toUInt32(), edx: ctx.edx.toUInt32(), args: stack, pageAddrs: Array.from(pages.keys()) };
      this.bufs = Array.from(pages.values());
    },
    onLeave(ret) {
      if (this.skip) return;
      this.rec.live = ret.toUInt32();
      sent++;
      const total = this.bufs.reduce((n, b) => n + b.byteLength, 0);
      const blob = new Uint8Array(total);
      let off = 0;
      for (const b of this.bufs) { blob.set(new Uint8Array(b), off); off += b.byteLength; }
      send({ type: 'call', rec: this.rec }, blob.buffer);
    },
  });
}

rpc.exports = {
  start(c) {
    cfg = c;
    const m = Process.getModuleByName(c.module);
    const target = c.module.toLowerCase() === 'golf_clean.exe' ? ptr(c.addr) : m.base.add(c.addr);
    attach(target);
    return { base: m.base.toUInt32(), size: m.size, target: target.toUInt32(), firstByte: target.readU8() };
  },
  dumpModule(name) {
    const m = Process.getModuleByName(name);
    return { base: m.base.toUInt32(), size: m.size };
  },
  readRange(base, size) { return ptr(base).readByteArray(size); },
  stats() { return { calls, sent }; },
};
