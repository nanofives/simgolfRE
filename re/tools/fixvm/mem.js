'use strict';
// Sparse 32-bit address space of the fixture VM (re/tools/fixvm, step S4.1). Pages of 4 KB, created by map(); every byte
// written through this object marks its page dirty, and takeDirty() hands the dirty pages to the other side of the link
// (port/test/sound_emu.py keeps the same pages in Unicorn). Pages the other side sends are applied with apply() and are NOT
// marked dirty (both sides already agree on them). An access to an unmapped page throws like Frida's access violation.
const PAGE = 0x1000;

class AccessViolation extends Error {
  constructor(addr, what) {
    super('access violation ' + what + ' 0x' + (addr >>> 0).toString(16).padStart(8, '0'));
    this.addr = addr >>> 0;
  }
}

class Space {
  constructor() {
    this.pages = new Map();          // page index -> Uint8Array(PAGE)
    this.prot = new Map();           // page index -> 'rwx' string (recorded, not enforced)
    this.dirty = new Set();
    this.writesTo = null;            // optional hook(addr, n): who wrote where (style detection)
  }
  map(base, size, fill, prot) {
    base >>>= 0;
    const lo = Math.floor(base / PAGE), hi = Math.floor((base + Math.max(size, 1) - 1) / PAGE);
    for (let p = lo; p <= hi; p++) {
      if (!this.pages.has(p)) {
        const b = new Uint8Array(PAGE);
        if (fill) b.fill(fill);
        this.pages.set(p, b);
        this.dirty.add(p);
      }
      if (prot) this.prot.set(p, prot);
    }
  }
  isMapped(addr) { return this.pages.has(Math.floor((addr >>> 0) / PAGE)); }
  page(addr, what) {
    const b = this.pages.get(Math.floor((addr >>> 0) / PAGE));
    if (!b) throw new AccessViolation(addr, what);
    return b;
  }
  read(addr, n) {
    addr >>>= 0;
    const out = new Uint8Array(n);
    let o = 0;
    while (o < n) {
      const a = (addr + o) >>> 0, b = this.page(a, 'reading'), off = a % PAGE, k = Math.min(n - o, PAGE - off);
      out.set(b.subarray(off, off + k), o);
      o += k;
    }
    return out;
  }
  write(addr, bytes) {
    addr >>>= 0;
    const n = bytes.length;
    let o = 0;
    while (o < n) {
      const a = (addr + o) >>> 0, pi = Math.floor(a / PAGE), b = this.pages.get(pi);
      if (!b) throw new AccessViolation(a, 'writing');
      const off = a % PAGE, k = Math.min(n - o, PAGE - off);
      b.set(bytes.subarray ? bytes.subarray(o, o + k) : bytes.slice(o, o + k), off);
      this.dirty.add(pi);
      o += k;
    }
    if (this.writesTo && n) this.writesTo(addr, n);
  }
  u8(addr) { addr >>>= 0; return this.page(addr, 'reading')[addr % PAGE]; }
  u32(addr) {
    addr >>>= 0;
    const off = addr % PAGE;
    if (off <= PAGE - 4) {
      const b = this.page(addr, 'reading');
      return (b[off] | (b[off + 1] << 8) | (b[off + 2] << 16) | (b[off + 3] << 24)) >>> 0;
    }
    const r = this.read(addr, 4);
    return (r[0] | (r[1] << 8) | (r[2] << 16) | (r[3] << 24)) >>> 0;
  }
  w8(addr, v) { this.write(addr, Uint8Array.of(v & 0xff)); }
  w32(addr, v) { v >>>= 0; this.write(addr, Uint8Array.of(v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, v >>> 24)); }
  cstring(addr, max) {
    const out = [];
    for (let i = 0; max === undefined || i < max; i++) {
      const c = this.u8(addr + i);
      if (!c) break;
      out.push(c);
    }
    return Buffer.from(out).toString('latin1');
  }
  takeDirty() {
    const out = [];
    for (const p of [...this.dirty].sort((a, b) => a - b)) out.push([p, Buffer.from(this.pages.get(p)).toString('base64')]);
    this.dirty.clear();
    return out;
  }
  apply(pages) {
    for (const [p, b64] of pages || []) {
      let b = this.pages.get(p);
      if (!b) { b = new Uint8Array(PAGE); this.pages.set(p, b); }
      b.set(Buffer.from(b64, 'base64'));
    }
  }
}

module.exports = { Space, PAGE, AccessViolation };
