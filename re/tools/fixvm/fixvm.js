'use strict';
// Fixture VM (step S4.1 of the SimGolf port roadmap): runs the path-1 A/B fixtures (re/frida/js/fixtures.d/*.js and the
// helpers diff_hook.py loads with them) under Node over a mock Frida API (frida.js) and a 32-bit virtual address space
// (mem.js), next to an emulator that runs the native code (port/test/sound_emu.py: the original sound.dll in Unicorn; the
// S4.2 port runner can take its place). sound_replay.md describes the whole tool.
//
// LINK. One Node process per client, spoken to over stdin / stdout with frames of `u32 length (LE) + JSON`. Both sides make
// requests ({op, id, ...}) and answer with {op: 'reply', id, ...}; a request may arrive while the other side waits for a
// reply (the VM asks the emulator to run a NativeFunction while it is serving the emulator's trap), so both sides serve
// requests inside their wait loop. Everything is synchronous (fs.readSync on the pipe).
//
// MEMORY. Every message carries `pages`: the pages its sender changed since its previous message ([page index, base64]); the
// receiver applies them before it acts. So at any message boundary both copies of the address space are equal. Messages
// from the VM also carry `hooks` (the Interceptor redirects and replaceFast trampolines) whenever they changed.
//
// Requests the VM serves:
//   load    {name, base, image (base64 of SizeOfImage bytes, relocated), exports {name: rva}, sections [[rva, vsize, prot]]}
//   begin   {name, base, layout {heap, emuheap, pad}, tid, mainTid, teb, regions [[base, size]], scripts [paths], nofreeze}
//           a fresh machine for one key: the image, its IAT bound to the fake system DLLs, a new vm context with the mock
//           Frida API, then the scripts in order (diff_hook.py's helpers, then the fixture fragments)
//   fixture {name}  -> {fx {name: {p|n|s}}, used [style flags]}
//   trap    {addr, esp, ecx, edx}  -> {eax, edx, f, pop} | {error, api}
//   timers  {}      run the queued setTimeout callbacks (the live run fires them after the key's rpc returns)
//   eval    {src}   evaluate an expression in the key's context (debugging and tests)
//   quit
// Requests the VM makes: call {addr, abi, ret, args [{t, v}]} -> {eax, edx, f} | {error}
const fs = require('fs');
const vm = require('vm');
const path = require('path');
const { Space, PAGE } = require('./mem');
const { Sys, UnmodelledImport } = require('./syslibs');

const TRAP_CB = 0x7b000000;          // NativeCallback / replaceFast trampoline slots, 16 bytes each

// ---- framing ------------------------------------------------------------------------------------------------------------
const hdr = Buffer.alloc(4);
function readExact(buf, n) {
  let o = 0;
  while (o < n) {
    let k;
    try { k = fs.readSync(0, buf, o, n - o, null); } catch (e) { if (e.code === 'EAGAIN') continue; if (e.code === 'EOF') process.exit(0); throw e; }
    if (k === 0) process.exit(0);
    o += k;
  }
}
function recvRaw() {
  readExact(hdr, 4);
  const n = hdr.readUInt32LE(0);
  const b = Buffer.alloc(n);
  readExact(b, n);
  return JSON.parse(b.toString('utf8'));
}
function sendRaw(obj) {
  const b = Buffer.from(JSON.stringify(obj), 'utf8');
  const h = Buffer.alloc(4);
  h.writeUInt32LE(b.length, 0);
  fs.writeSync(1, h);
  let o = 0;
  while (o < b.length) o += fs.writeSync(1, b, o, b.length - o);
}

// ---- state --------------------------------------------------------------------------------------------------------------
const images = new Map();            // `${name}@${base}` -> {image Buffer, exports, sections, base, name}
const scriptCache = new Map();       // path -> vm.Script
let M = null;                        // the current key's machine
let nextId = 1;

function attach(msg) {
  if (M) {
    msg.pages = M.space.takeDirty();
    if (M.hooksVersion !== M.hooksSent) {
      msg.hooks = { redirects: [...M.redirects.entries()], tramps: [...M.tramps.entries()] };
      M.hooksSent = M.hooksVersion;
    }
  }
  return msg;
}
function receive(m) { if (M && m.pages) M.space.apply(m.pages); }

// a request to the emulator; serves the emulator's nested requests while waiting
function request(msg) {
  msg.id = nextId++;
  sendRaw(attach(msg));
  for (;;) {
    const m = recvRaw();
    receive(m);
    if (m.op === 'reply') {
      if (m.id !== msg.id) throw new Error('fixvm: reply ' + m.id + ' while waiting for ' + msg.id);
      return m;
    }
    serve(m);
  }
}
function serve(m) {
  let r;
  try {
    r = handle(m) || {};
  } catch (e) {
    r = { error: String(e && e.message || e), stack: String(e && e.stack || '').split('\n').slice(0, 6).join('\n') };
    if (e instanceof UnmodelledImport || (e && e.api)) r.api = e.api;
  }
  r.op = 'reply';
  r.id = m.id;
  sendRaw(attach(r));
}

// ---- allocators ---------------------------------------------------------------------------------------------------------
// Bump allocator. Layout B shifts the base and puts pseudo-random gaps between blocks, so that a value that differs between
// the A and B runs of a key is a run address (sound_replay.md, "position independence").
class Heap {
  constructor(space, base, pad, prot) {
    this.space = space; this.small = base; this.big = (base + 0x04000000) >>> 0; this.pad = pad >>> 0; this.prot = prot || 'rw-';
    this.r = (pad * 2654435761) >>> 0 || 1;
  }
  rnd() { let x = this.r; x ^= x << 13; x >>>= 0; x ^= x >>> 17; x ^= x << 5; this.r = x >>> 0; return this.r; }
  alloc(n) {
    n = Math.max(n >>> 0, 1);
    if (n >= PAGE) return this.allocPages(n);
    if (this.pad) this.small += 16 * (1 + this.rnd() % 64);
    if (this.pad && Math.floor(this.small / PAGE) !== Math.floor((this.small + n - 1) / PAGE)) this.small = ((this.small + PAGE) & ~(PAGE - 1)) >>> 0;
    const p = this.small;
    this.small = (this.small + n + 15) & ~15;
    this.space.map(p, n, 0, this.prot);
    return p >>> 0;
  }
  allocPages(n) {
    if (this.pad) this.big += PAGE * (1 + this.rnd() % 7);   // every big block moves against its neighbours
    const p = this.big;
    const sz = (n + PAGE - 1) & ~(PAGE - 1);
    this.big = (this.big + sz + PAGE) >>> 0;          // one unmapped guard page between big blocks
    this.space.map(p, sz, 0, this.prot);
    return p >>> 0;
  }
}

// ---- PE helpers (on the VM's copy of the image) -------------------------------------------------------------------------
function bindImports(S, sys, base, log) {
  const nt = base + S.u32(base + 0x3c);
  const impRva = S.u32(nt + 0x80);
  if (!impRva) return;
  for (let d = base + impRva; ; d += 20) {
    const oft = S.u32(d), nameRva = S.u32(d + 12), ft = S.u32(d + 16);
    if (!nameRva && !ft) break;
    const dll = S.cstring(base + nameRva);
    const names = oft || ft;
    for (let i = 0; ; i++) {
      const e = S.u32(base + names + 4 * i);
      if (!e) break;
      const name = (e & 0x80000000) ? ('#' + (e & 0xffff)) : S.cstring(base + e + 2);
      const addr = sys.exportAddr(dll, name);
      if (addr === null) { log.push('import of an unknown DLL ' + dll + '!' + name); continue; }
      S.w32(base + ft + 4 * i, addr);
    }
  }
  S.w32(nt + 0x34, base);           // the loader stores the actual base in OptionalHeader.ImageBase
}

// ---- requests -----------------------------------------------------------------------------------------------------------
function handle(m) {
  switch (m.op) {
    case 'load': {
      images.set(m.name.toLowerCase() + '@' + m.base, { name: m.name, base: m.base >>> 0, image: Buffer.from(m.image, 'base64'),
        exports: m.exports || {}, sections: m.sections || [], data: m.data || null });
      return {};
    }
    case 'begin': return begin(m);
    case 'fixture': return fixture(m);
    case 'regions': return regions(m);
    case 'mkarm': return mkarm(m);
    case 'resolve': return resolveVec(m);
    case 'arm': return arm(m);
    case 'curargs': return curargs(m);
    case 'trap': return trap(m);
    case 'timers': { const t = M.timers.splice(0); t.forEach((f) => { if (f) f(); }); return { ran: t.length }; }
    case 'eval': { const v = vm.runInContext(m.src, M.ctx); return { value: v === undefined ? null : JSON.parse(JSON.stringify(v, (k, x) => typeof x === 'bigint' ? x.toString() : x)) }; }
    // negative control: every non-void NativeCallback made so far returns its value + delta (sound_replay.md, controls)
    case 'cbpatch': {
      let n = 0;
      for (const cb of M.cbs.values()) {
        if (cb.kind !== 'callback' || cb.ret === 'void' || cb.ret === 'double' || cb.ret === 'float' || cb.patched) continue;
        const f = cb.fn, d = m.delta >>> 0, I = M.fr._internal;
        cb.fn = function () { const r = f.apply(this, arguments); return (I.toU32(r === undefined || r === null ? 0 : r) + d) >>> 0; };
        cb.patched = true;
        n++;
      }
      return { patched: n };
    }
    case 'state': return { used: [...M.used], imports: M.importsCalled.slice(), log: M.logLines.slice(-50), cbErrors: M.cbErrors.slice(0, 20), cbErrorCount: M.cbErrors.length };
    case 'quit': process.exit(0);
    default: throw new Error('fixvm: unknown op ' + m.op);
  }
}

function begin(m) {
  const img = images.get(m.name.toLowerCase() + '@' + m.base);
  if (!img) throw new Error('fixvm: image ' + m.name + '@0x' + (m.base >>> 0).toString(16) + ' not loaded');
  const space = new Space();
  const lay = m.layout || {};
  M = {
    space, tid: m.tid >>> 0, mainTid: m.mainTid >>> 0, teb: m.teb >>> 0,
    cbs: new Map(), redirects: new Map(), tramps: new Map(), hooksVersion: 0, hooksSent: -1,
    timers: [], used: new Set(), importsCalled: [], logLines: [], trapNext: 0, cbErrors: [],
    log: (s) => { M.logLines.push(String(s)); },
    host: { request },
  };
  M.tlsFirst = (lay.tls === undefined ? 0 : lay.tls) >>> 0;
  M.heap = new Heap(space, (lay.heap || 0x30000000) >>> 0, lay.pad || 0);
  M.emuHeap = new Heap(space, (lay.emuheap || 0x60000000) >>> 0, lay.pad || 0);
  M.allocTrap = () => {
    const a = (TRAP_CB + 16 * M.trapNext++) >>> 0;
    if (!space.isMapped(a)) space.map(a, PAGE, 0xcc, 'r-x');
    return a;
  };
  // the emulator's own regions (stack, TEB, stubs): present here as zero pages, not dirty (the emulator already has them)
  for (const [b, n] of m.regions || []) {
    for (let p = Math.floor(b / PAGE); p <= Math.floor((b + n - 1) / PAGE); p++) if (!space.pages.has(p)) space.pages.set(p, new Uint8Array(PAGE));
  }
  // the module image
  space.map(img.base, img.image.length, 0);
  space.write(img.base, new Uint8Array(img.image));
  for (const [rva, vs, prot] of img.sections) space.map(img.base + rva, vs, 0, prot);
  const sys = new Sys(M);
  M.sys = sys;
  const soundRec = {
    name: img.name, base: img.base, size: img.image.length,
    exp: (n) => (Object.prototype.hasOwnProperty.call(img.exports, n) ? (img.base + img.exports[n]) >>> 0 : null),
    exports: () => Object.keys(img.exports).map((n) => ({ name: n, addr: (img.base + img.exports[n]) >>> 0 })),
    ranges: () => img.sections.map(([rva, vs, prot]) => ({ base: img.base + rva, size: (vs + PAGE - 1) & ~(PAGE - 1), protection: prot })),
  };
  M.modules = [soundRec].concat(sys.moduleRecords(), [{ name: 'golf_clean.exe', base: 0x00400000, size: 0x445000,
    exp: () => null, exports: () => [], ranges: () => [] }, { name: 'winmm.dll(shim)', base: 0x7e000000, size: 0, exp: () => null, exports: () => [], ranges: () => [] }]);
  for (const mod of sys.mods) space.map(mod.base, mod.size, 0xcc, 'r-x');
  installShimExports(M);
  const notes = [];
  bindImports(space, sys, img.base, notes);
  // sound.dll globals written by the fixture (style detection)
  const dr = img.data || [img.base, img.image.length];
  space.writesTo = (a, n) => { if (M.inFixture && a < dr[0] + dr[1] && a + n > dr[0]) M.used.add('globals'); };
  // the context
  const ctxGlobals = { Buffer, DIFF_FIXTURES: {}, __diffKeepAlive: [] };
  const ctx = vm.createContext(ctxGlobals);
  M.ctx = ctx;
  const fr = vm.runInContext('(function(){ const module = undefined; ' + FRIDA_SRC + '\n return makeFrida; })()', ctx)(M);
  for (const k of Object.keys(fr)) if (k !== '_internal') ctx[k] = fr[k];
  M.fr = fr;
  const errors = [];
  for (const p of m.scripts || []) {
    if (p === '@freeze') { vm.runInContext(FREEZE_MOCK, ctx); continue; }
    try {
      scriptFor(p).runInContext(ctx);
    } catch (e) {
      errors.push({ file: path.basename(p), error: String(e && e.message || e) });
    }
  }
  // style flags: wrap the stand-in entry points
  vm.runInContext(STYLE_WRAP, ctx)(M.used);
  return { errors, notes, modules: M.modules.map((x) => [x.name, x.base, x.size]) };
}

function scriptFor(p) {
  let s = scriptCache.get(p);
  if (!s) {
    let src = fs.readFileSync(p, 'utf8');
    const base = path.basename(p);
    // diff_hook.py rewrites Memory.alloc( to __keepAlloc( in every part except keepalive.js and diff_hook.js
    if (base !== 'keepalive.js' && base !== 'diff_hook.js') src = src.split('Memory.alloc(').join('__keepAlloc(');
    s = new vm.Script(src, { filename: p });
    scriptCache.set(p, s);
  }
  return s;
}

function fixture(m) {
  const fn = M.ctx.DIFF_FIXTURES[m.name];
  if (typeof fn !== 'function') throw new Error('fixvm: no fixture ' + m.name);
  M.used.clear();                                   // style flags describe the fixture, not the helpers' load
  M.inFixture = true;
  let fx;
  try { fx = fn(); } finally { M.inFixture = false; }
  M.fx = fx || {};
  const out = {};
  const NP = M.fr._internal.NativePointer;
  for (const k of Object.keys(fx || {})) {
    const d = Object.getOwnPropertyDescriptor(fx, k);
    if (d && d.get) continue;                       // a getter runs when a vector names it, not here
    const v = fx[k];
    if (v instanceof NP || (typeof v === 'function' && typeof v.v === 'number')) out[k] = { p: v.v >>> 0 };
    else if (typeof v === 'number') out[k] = { n: v };
    else if (typeof v === 'string') out[k] = { s: v };
    else if (typeof v === 'boolean') out[k] = { n: v ? 1 : 0 };
  }
  return { fx: out, used: [...M.used] };
}

// ---- diff_hook.js run(), step by step (re/frida/js/diff_hook.js; the emulator side holds the snapshots) ----------------
// regions: `spec.state.map(s => (typeof s[0] === 'string' ? fx[s[0].slice(1)] : ptr(s[0])).add(s[1]))`, in that order
function regions(m) {
  const fx = M.fx || {};
  const out = [];
  for (const [b, off, n] of m.state) {
    const base = typeof b === 'string' ? fx[b.slice(1)] : M.fr.ptr(b);
    if (base === undefined || base === null) throw new Error('fixvm: state region base ' + b + ' not returned by the fixture');
    out.push([M.fr._internal.toU32(base.add(off)), n]);
  }
  return { regions: out };
}
// mkarm: `new NativeFunction(p, spec.ret, spec.args, spec.abi)` with the context's NativeFunction at that moment (a fixture
// may have replaced it, c3fo's x87 probe)
function mkarm(m) {
  M.spec = { ret: m.ret, args: m.args, abi: m.abi };
  M.arm = new M.ctx.NativeFunction(M.fr.ptr(m.addr), m.ret, m.args, m.abi);
  return {};
}
// resolve: the vector's arguments, `$name` through the fixture object (a getter there runs now, before the snapshot)
function resolveVec(m) {
  const fx = M.fx || {};
  const res = (v) => (typeof v === 'string' && v.startsWith('$')) ? fx[v.slice(1)] : v;
  M.curArgs = m.vec.map((v, i) => {
    const x = res(v);
    if (x === undefined) throw new Error('fixvm: fixture returned no ' + v);
    return M.spec.args[i] === 'pointer' && typeof x === 'number' ? M.fr.ptr(x) : x;
  });
  return {};
}
// curargs: the current vector's arguments as raw records {t, v} (what a second executor, the port, is called with)
function curargs(m) {
  const I = M.fr._internal;
  return { args: M.spec.args.map((t, i) => I.encodeArg(t, M.curArgs[i])), obj: M.fx && M.fx.obj ? I.toU32(M.fx.obj) : null };
}
// arm: one call of the arm; `norm` is diff_hook.js norm(); doubles also come back as `f` (the CSV's printing of a double
// depends on the Frida runtime of the run, so the emulator side formats them)
function arm(m) {
  const r = M.arm.apply(null, M.curArgs);
  const ret = M.spec.ret;
  const out = {};
  if (ret === 'void') out.norm = 'void';
  else if (ret !== 'pointer') {
    out.norm = r.toString();
    if (ret === 'double' || ret === 'float') out.f = Number.isNaN(r) ? 'NaN' : (r === Infinity ? 'Infinity' : (r === -Infinity ? '-Infinity' : r));
  } else if (r.isNull()) out.norm = 'NULL';
  else out.norm = (M.fx && M.fx.obj) ? 'obj+0x' + r.sub(M.fx.obj).toString(16) : r.toString();
  return out;
}

function readArgs(argt, abi, esp, ecx, edx) {
  const S = M.space;
  const vals = [];
  let sp = esp + 4;
  const regs = abi === 'thiscall' ? [ecx] : (abi === 'fastcall' ? [ecx, edx] : []);
  for (const t of argt) {
    const wide = t === 'double' || t === 'int64' || t === 'uint64' || t === 'longlong' || t === 'ulonglong';
    if (!wide && regs.length) { vals.push(regs.shift() >>> 0); continue; }
    if (t === 'double') { vals.push(Buffer.from(S.read(sp, 8)).readDoubleLE(0)); sp += 8; continue; }
    if (t === 'float') { vals.push(Buffer.from(S.read(sp, 4)).readFloatLE(0)); sp += 4; continue; }
    if (wide) { vals.push(Buffer.from(S.read(sp, 8)).readBigUInt64LE(0)); sp += 8; continue; }
    vals.push(S.u32(sp)); sp += 4;
  }
  return { vals, stackBytes: sp - esp - 4 };
}

function trap(m) {
  const addr = m.addr >>> 0;
  const cb = M.cbs.get(addr);
  if (cb) {
    const I = M.fr._internal;
    const { vals, stackBytes } = readArgs(cb.args, cb.abi, m.esp >>> 0, m.ecx >>> 0, m.edx >>> 0);
    const js = cb.args.map((t, i) => {
      if (t === 'double' || t === 'float') return vals[i];
      if (typeof vals[i] === 'bigint') return t === 'int64' ? M.ctx.int64(vals[i].toString()) : M.ctx.uint64(vals[i].toString());
      return I.decodeArg(t, vals[i]);
    });
    // Frida emits a JS exception thrown inside a NativeCallback as a script error and the native caller goes on; the VM
    // logs it and returns 0 [U-1402: the value Frida hands back after an exception is taken as 0]
    let r, threw = false;
    try { r = cb.fn.apply(null, js); } catch (e) {
      threw = true;
      M.cbErrors.push(String(e && e.message || e));
    }
    const out = threw ? (cb.ret === 'void' ? {} : (cb.ret === 'double' || cb.ret === 'float' ? { f: 0 } : { eax: 0, edx: 0 })) : I.encodeRet(cb.ret, r);
    out.pop = (cb.abi === 'default' || cb.abi === 'mscdecl' || cb.abi === 'cdecl') ? 0 : stackBytes;
    return out;
  }
  const ex = M.sys.byAddr.get(addr);
  if (ex) {
    const n = ex.nargs === null ? 0 : ex.nargs;
    const args = [];
    for (let i = 0; i < n; i++) args.push(M.space.u32((m.esp >>> 0) + 4 + 4 * i));
    const r = M.sys.invoke(ex, args);
    const o = typeof r === 'object' ? r : { eax: r >>> 0 };
    o.pop = ex.abi === 'stdcall' ? 4 * n : 0;
    return o;
  }
  throw new Error('fixvm: trap at 0x' + addr.toString(16) + ' is neither a callback nor an import');
}

// ---- the shim's test exports a fixture calls (winmm.dll in the game is our shim) ----------------------------------------
// C3foX87Probe (shim/src/re/c3fo.cpp, naked asm): hand-assembled below instruction for instruction; its call to
// C3foProbeScrub goes to a VM callback that does the scrub's memory edits in JS and the one x87 conversion
// (`fninit; fld tbyte; fstp qword; fninit`) as native code on the emulator. C3foSehCall5: the target called with five
// stdcall arguments; an exception under it is outside the VM (the call fails and names it).
const SHIM_CODE = 0x7c800000;
function installShimExports(M) {
  const S = M.space;
  S.map(SHIM_CODE, PAGE, 0xcc, 'r-x');
  // conversion stub at +0x200: fninit; mov eax,[esp+4]; fld tbyte [eax]; mov eax,[esp+8]; fstp qword [eax]; fninit; ret
  const CONV = SHIM_CODE + 0x200;
  S.write(CONV, Uint8Array.from([0xdb, 0xe3, 0x8b, 0x44, 0x24, 0x04, 0xdb, 0x28, 0x8b, 0x44, 0x24, 0x08, 0xdd, 0x18, 0xdb, 0xe3, 0xc3]));
  const scrubAddr = M.allocTrap();
  M.cbs.set(scrubAddr, { kind: 'callback', ret: 'void', args: ['uint32'], abi: 'default', fn: function (blk) {
    const im = blk + 0x48;
    for (const o of [2, 3, 6, 7, 10, 11]) S.w8(im + o, 0);
    S.write(im + 12, new Uint8Array(16));
    const sw = S.u8(im + 4) | (S.u8(im + 5) << 8), tw = S.u8(im + 8) | (S.u8(im + 9) << 8);
    const top = (sw >>> 11) & 7;
    for (let i = 0; i < 8; i++) if (((tw >>> (2 * ((top + i) & 7))) & 3) === 3) S.write(im + 28 + 10 * i, new Uint8Array(10));
    if (((tw >>> (2 * top)) & 3) !== 3) {
      const r = M.host.request({ op: 'call', addr: CONV, abi: 'default', ret: 'void', args: [{ t: 'pointer', v: im + 28 }, { t: 'pointer', v: blk + 0x38 }] });
      if (r.error) throw new Error('C3foProbeScrub conversion: ' + r.error);
    } else {
      S.write(blk + 0x38, Uint8Array.from([0, 0, 0, 0, 0xad, 0xde, 0xf8, 0x7f]));
    }
  } });
  const P = SHIM_CODE;
  const code = [0x55, 0x8b, 0xec, 0x53, 0x56, 0x57, 0x8b, 0x5d, 0x0c, 0xd9, 0x7b, 0x0c, 0x8d, 0x75, 0x10, 0x33, 0xd2,
    0xf7, 0x03, 0x01, 0, 0, 0, 0x74, 0x05, 0x8b, 0x16, 0x83, 0xc6, 0x04,
    0x8b, 0x4b, 0x04, 0x8d, 0x04, 0x8d, 0, 0, 0, 0, 0x2b, 0xe0, 0x8b, 0xfc, 0xfc, 0xf3, 0xa5, 0xdb, 0xe3];
  for (let i = 0; i < 8; i++) code.push(0xd9, 0xe8);
  for (let i = 0; i < 8; i++) code.push(0xdd, 0xd8);
  code.push(0xd9, 0x6b, 0x08, 0x85, 0xd2, 0x74, 0x08, 0x80, 0x7a, 0x0a, 0x00, 0x75, 0x02, 0xdb, 0x2a,
    0xff, 0x55, 0x08, 0x8b, 0x5d, 0x0c, 0x89, 0x43, 0x40, 0x89, 0x53, 0x44, 0xdd, 0x73, 0x48, 0xd9, 0x6b, 0x0c, 0x53);
  const at = P + code.length;
  const rel = (scrubAddr - (at + 5)) >>> 0;
  code.push(0xe8, rel & 0xff, (rel >>> 8) & 0xff, (rel >>> 16) & 0xff, rel >>> 24);
  code.push(0x83, 0xc4, 0x04, 0xd9, 0x6b, 0x0c, 0xf7, 0x03, 0x04, 0, 0, 0, 0x74, 0x03, 0xdd, 0x43, 0x38,
    0x8b, 0x43, 0x40, 0x8b, 0x53, 0x44, 0x8d, 0x65, 0xf4, 0x5f, 0x5e, 0x5b, 0x5d, 0xc3);
  S.write(P, Uint8Array.from(code));
  M.sys.defineNative('winmm.dll', 'C3foX87Probe', P);
  // C3foSehCall5 (c3fo.cpp) and C3fzaSehCall4 (c3fza.cpp): target(a0..a4 / a0..a3) stdcall under __try; code -> blk+0x30
  const sehCall = (name, n) => M.sys.defineBody('winmm.dll', name, 2 + n, 'cdecl', (a, X) => {
    const args = [];
    for (let i = 0; i < n; i++) args.push({ t: 'uint32', v: a[2 + i] });
    const r = X.M.host.request({ op: 'call', addr: a[0], abi: 'stdcall', ret: 'void', args });
    if (r.error) { const e = new Error(name + ': ' + r.error); if (r.api) e.api = r.api; throw e; }
    X.S.w32(a[1] + 0x30, 0);
    return 0;
  });
  sehCall('C3foSehCall5', 5);
  sehCall('C3fzaSehCall4', 4);
}

// __freeze for the VM: there is one thread, so thread() suspends nothing; save / fill / setU32 still write (and remember
// the old bytes for the flush, which the VM runs only when asked); negOn finds no NegCtl export (no shim in the VM)
const FREEZE_MOCK = `(function () {
  const pend = [];
  const status = globalThis.__freezeStatus = { freezes: 0, flushes: 0, resumed: [], lastTries: 0, lastEip: 0 };
  function flush() { const u = pend.splice(0); for (let i = u.length - 1; i >= 0; i--) u[i](); status.flushes++; }
  function atFlush(fn) { pend.push(fn); }
  function save(p, n) { const b = p.readByteArray(n); atFlush(function () { p.writeByteArray(b); }); }
  function fill(p, n, byte) { save(p, n); p.writeByteArray(new Array(n).fill(byte)); }
  function setU32(p, v) { save(p, 4); p.writeU32(v); }
  function thread() { status.freezes++; globalThis.__fixvmUsed && globalThis.__fixvmUsed.add('freeze'); return function () {}; }
  globalThis.__freeze = { begin: flush, flush: flush, schedule: function () {}, thread: thread, atFlush: atFlush,
    save: save, fill: fill, setU32: setU32, status: status, negOn: function () {}, negCtlWords: function () { return []; } };
})();`;

const STYLE_WRAP = `(function (used) {
  globalThis.__fixvmUsed = used;
  [['__heapStandIn', 'heap'], ['__importStandIn', 'import'], ['__mmioStandIn', 'mmio'], ['__fileStandIn', 'file'],
   ['__uiTree', 'uitree'], ['__modal', 'modal']].forEach(function (e) {
    const f = globalThis[e[0]];
    if (typeof f !== 'function') return;
    const w = function () { used.add(e[1]); return f.apply(this, arguments); };
    Object.keys(f).forEach(function (k) { w[k] = f[k]; });
    globalThis[e[0]] = w;
  });
})`;

const FRIDA_SRC = fs.readFileSync(path.join(__dirname, 'frida.js'), 'utf8').replace(/^'use strict';/, '');

// ---- main loop ----------------------------------------------------------------------------------------------------------
for (;;) {
  const m = recvRaw();
  receive(m);
  serve(m);
}
