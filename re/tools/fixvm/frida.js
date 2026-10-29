'use strict';
// Mock of the Frida JavaScript API the A/B fixtures use (re/frida/js/fixtures.d/*.js and the helpers they call), over the
// fixture VM's 32-bit address space (mem.js). Built per key by fixvm.js: makeFrida(M) returns the globals to put in the
// key's vm context. M is the key's machine: { space, heap, sys, cbs, host, tid, modules, ... } (fixvm.js).
//
// What is modelled and how (sound_replay.md has the full table):
//   NativePointer      a uint32 value; arithmetic wraps to 32 bits like Frida in a 32-bit process; read/write through M.space
//   Memory.alloc       M.heap's bump allocator (zero-filled; sizes >= one page page-aligned, smaller ones 16-aligned)
//   NativeCallback     a trap address in the VM's trap range; a call into it from emulated code comes back here as a trap
//   NativeFunction     a JS function: a fake system DLL export runs its JS body here, a NativeCallback runs its JS function,
//                      any other address is executed by the emulator on the other side of the link (M.host.request 'call')
//   Interceptor        replace / replaceFast / revert record redirects the emulator applies at the target's first byte
//   Process / Module   the VM's module table (sound.dll at the chosen base, fake system DLLs)
//   setTimeout etc.    queued; they run when the key ends (as in the live run: after the rpc call that ran the vectors)
// Frida quirks reproduced (measured in earlier batches, CLAUDE.md): writeDouble(+-Infinity) stores a quiet NaN (c3fe notes,
// Frida 17.22.1); a double returned by a NativeFunction as +-Infinity reads as NaN.

const QNAN_LO = 0x00000000, QNAN_HI = 0x7ff80000;

function makeFrida(M) {
  const S = M.space;
  const dv = new DataView(new ArrayBuffer(8));

  function toU32(v) {
    if (v instanceof NativePointer || (typeof v === 'function' && typeof v.v === 'number')) return v.v;
    if (typeof v === 'number') return (v < 0 ? (v >>> 0) : (Math.floor(v) % 0x100000000)) >>> 0;
    if (typeof v === 'bigint') return Number(BigInt.asUintN(32, v));
    if (v instanceof Int64Base) return Number(BigInt.asUintN(32, v.b));
    if (typeof v === 'string') return parsePtr(v);
    if (typeof v === 'boolean') return v ? 1 : 0;
    if (v === null || v === undefined) throw new TypeError('expected a pointer or an integer');
    if (typeof v === 'object' && typeof v.handle !== 'undefined') return toU32(v.handle);
    throw new TypeError('expected a pointer or an integer, got ' + typeof v);
  }
  function parsePtr(s) {
    s = String(s).trim();
    const neg = s.startsWith('-');
    if (neg) s = s.slice(1);
    let n = /^0x/i.test(s) ? BigInt(s) : BigInt(s);
    if (neg) n = -n;
    return Number(BigInt.asUintN(32, n));
  }

  class NativePointer {
    constructor(v) { this.v = (v === undefined) ? 0 : toU32(v); }
    get handle() { return this; }
    add(x) { return new NativePointer((this.v + toU32(x)) >>> 0); }
    sub(x) { return new NativePointer((this.v - toU32(x)) >>> 0); }
    and(x) { return new NativePointer((this.v & toU32(x)) >>> 0); }
    or(x) { return new NativePointer((this.v | toU32(x)) >>> 0); }
    xor(x) { return new NativePointer((this.v ^ toU32(x)) >>> 0); }
    shr(n) { n = toU32(n); return new NativePointer(n >= 32 ? 0 : (this.v >>> n)); }
    shl(n) { n = toU32(n); return new NativePointer(n >= 32 ? 0 : ((this.v << n) >>> 0)); }
    not() { return new NativePointer((~this.v) >>> 0); }
    sign() { return this; }
    strip() { return this; }
    blend() { return this; }
    isNull() { return this.v === 0; }
    equals(o) { return this.v === toU32(o); }
    compare(o) { const b = toU32(o); return this.v === b ? 0 : (this.v < b ? -1 : 1); }
    toInt32() { return this.v | 0; }
    toUInt32() { return this.v; }
    toNumber() { return this.v; }
    valueOf() { return this.v; }
    toString(radix) {
      if (radix === undefined) return '0x' + this.v.toString(16);
      return this.v.toString(radix);
    }
    toJSON() { return this.toString(); }
    toMatchPattern() { const b = []; for (let i = 0; i < 4; i++) b.push(((this.v >>> (8 * i)) & 0xff).toString(16).padStart(2, '0')); return b.join(' '); }
    // ---- reads
    readU8() { return S.u8(this.v); }
    readS8() { return (S.u8(this.v) << 24) >> 24; }
    readU16() { const b = S.read(this.v, 2); return b[0] | (b[1] << 8); }
    readS16() { return (this.readU16() << 16) >> 16; }
    readU32() { return S.u32(this.v); }
    readS32() { return S.u32(this.v) | 0; }
    readU64() { const b = S.read(this.v, 8); return new UInt64(Buffer.from(b).readBigUInt64LE(0)); }
    readS64() { const b = S.read(this.v, 8); return new Int64(Buffer.from(b).readBigInt64LE(0)); }
    readInt() { return this.readS32(); }
    readUInt() { return this.readU32(); }
    readLong() { return this.readS32(); }
    readULong() { return this.readU32(); }
    readShort() { return this.readS16(); }
    readUShort() { return this.readU16(); }
    readPointer() { return new NativePointer(S.u32(this.v)); }
    readFloat() { const b = S.read(this.v, 4); return Buffer.from(b).readFloatLE(0); }
    readDouble() { const b = S.read(this.v, 8); return Buffer.from(b).readDoubleLE(0); }
    readByteArray(n) { n = toU32(n); const b = S.read(this.v, n); return b.buffer.slice(b.byteOffset, b.byteOffset + n); }
    readCString(n) { return S.cstring(this.v, n === undefined || n < 0 ? undefined : n); }
    readUtf8String(n) {
      const raw = [];
      for (let i = 0; n === undefined || n < 0 || i < n; i++) { const c = S.u8(this.v + i); if (!c && (n === undefined || n < 0)) break; raw.push(c); }
      return Buffer.from(raw).toString('utf8');
    }
    readAnsiString(n) { return this.readCString(n); }
    readUtf16String(n) {
      const out = [];
      for (let i = 0; n === undefined || n < 0 || i < n; i++) { const c = S.u8(this.v + 2 * i) | (S.u8(this.v + 2 * i + 1) << 8); if (!c) break; out.push(c); }
      return String.fromCharCode.apply(null, out);
    }
    // ---- writes (each returns this, like Frida)
    _w(bytes) { S.write(this.v, bytes); return this; }
    writeU8(v) { return this._w(Uint8Array.of(toU32(v) & 0xff)); }
    writeS8(v) { return this.writeU8(v); }
    writeU16(v) { const x = toU32(v); return this._w(Uint8Array.of(x & 0xff, (x >>> 8) & 0xff)); }
    writeS16(v) { return this.writeU16(v); }
    writeU32(v) { const x = toU32(v); return this._w(Uint8Array.of(x & 0xff, (x >>> 8) & 0xff, (x >>> 16) & 0xff, x >>> 24)); }
    writeS32(v) { return this.writeU32(v); }
    writeInt(v) { return this.writeU32(v); }
    writeUInt(v) { return this.writeU32(v); }
    writeLong(v) { return this.writeU32(v); }
    writeULong(v) { return this.writeU32(v); }
    writeShort(v) { return this.writeU16(v); }
    writeUShort(v) { return this.writeU16(v); }
    writePointer(p) { return this.writeU32(p); }
    writeU64(v) { const b = Buffer.alloc(8); b.writeBigUInt64LE(BigInt.asUintN(64, big(v))); return this._w(new Uint8Array(b)); }
    writeS64(v) { return this.writeU64(v); }
    writeFloat(v) {
      const b = Buffer.alloc(4);
      if (v === Infinity || v === -Infinity) b.writeUInt32LE(0x7fc00000); else b.writeFloatLE(Number(v));
      return this._w(new Uint8Array(b));
    }
    writeDouble(v) {
      const b = Buffer.alloc(8);
      if (v === Infinity || v === -Infinity) { b.writeUInt32LE(QNAN_LO, 0); b.writeUInt32LE(QNAN_HI, 4); } else b.writeDoubleLE(Number(v));
      return this._w(new Uint8Array(b));
    }
    writeByteArray(a) {
      let u;
      if (Object.prototype.toString.call(a) === '[object ArrayBuffer]') u = new Uint8Array(a);
      else if (ArrayBuffer.isView(a)) u = new Uint8Array(a.buffer, a.byteOffset, a.byteLength);
      else u = Uint8Array.from(a, (x) => x & 0xff);
      return this._w(u);
    }
    writeUtf8String(s) { return this._w(new Uint8Array(Buffer.concat([Buffer.from(String(s), 'utf8'), Buffer.of(0)]))); }
    writeAnsiString(s) { return this._w(new Uint8Array(Buffer.concat([Buffer.from(String(s), 'latin1'), Buffer.of(0)]))); }
    writeUtf16String(s) { const b = Buffer.alloc(2 * s.length + 2); for (let i = 0; i < s.length; i++) b.writeUInt16LE(s.charCodeAt(i), 2 * i); return this._w(new Uint8Array(b)); }
  }

  function big(v) {
    if (typeof v === 'bigint') return v;
    if (v instanceof Int64Base) return v.b;
    if (v instanceof NativePointer) return BigInt(v.v);
    if (typeof v === 'string') return BigInt(v);
    return BigInt(Math.trunc(Number(v)));
  }
  class Int64Base {
    constructor(v, signed) { this.signed = signed; this.b = signed ? BigInt.asIntN(64, big(v)) : BigInt.asUintN(64, big(v)); }
    _n(v) { return new this.constructor(v); }
    add(x) { return this._n(this.b + big(x)); }
    sub(x) { return this._n(this.b - big(x)); }
    and(x) { return this._n(this.b & big(x)); }
    or(x) { return this._n(this.b | big(x)); }
    xor(x) { return this._n(this.b ^ big(x)); }
    shr(n) { return this._n(this.b >> big(n)); }
    shl(n) { return this._n(this.b << big(n)); }
    not() { return this._n(~this.b); }
    compare(o) { const c = big(o); return this.b === c ? 0 : (this.b < c ? -1 : 1); }
    equals(o) { return this.b === big(o); }
    toNumber() { return Number(this.b); }
    valueOf() { return Number(this.b); }
    toString(r) { return this.b.toString(r || 10); }
    toJSON() { return this.toString(); }
  }
  class Int64 extends Int64Base { constructor(v) { super(v, true); } }
  class UInt64 extends Int64Base { constructor(v) { super(v, false); } }

  const ptr = (v) => new NativePointer(v);
  const NULL = new NativePointer(0);

  // ---- calls ---------------------------------------------------------------------------------------------------------
  const SIZES = { void: 0, pointer: 4, int: 4, uint: 4, uint32: 4, int32: 4, long: 4, ulong: 4, char: 4, uchar: 4,
    int8: 4, uint8: 4, int16: 4, uint16: 4, short: 4, ushort: 4, bool: 4, size_t: 4, ssize_t: 4, float: 4, double: 8,
    int64: 8, uint64: 8, longlong: 8, ulonglong: 8 };
  function abiOf(x) {
    if (x === undefined || x === null) return 'default';
    if (typeof x === 'string') return x;
    return x.abi || 'default';
  }
  // JS value -> raw argument record for the link: {t, v} with v a uint32 number, a float number or a decimal string (64-bit)
  function encodeArg(t, v) {
    if (t === 'double' || t === 'float') return { t, v: Number(v) };
    if (t === 'int64' || t === 'uint64' || t === 'longlong' || t === 'ulonglong') return { t, v: big(v).toString() };
    return { t, v: toU32(v) };
  }
  // raw argument record -> JS value as Frida hands it to a NativeCallback
  function decodeArg(t, raw) {
    switch (t) {
      case 'pointer': return new NativePointer(raw);
      case 'int': case 'int32': case 'long': case 'ssize_t': return raw | 0;
      case 'int8': case 'char': return (raw << 24) >> 24;
      case 'uint8': case 'uchar': case 'bool': return raw & 0xff;
      case 'int16': case 'short': return (raw << 16) >> 16;
      case 'uint16': case 'ushort': return raw & 0xffff;
      default: return raw >>> 0;
    }
  }
  function decodeRet(t, r) {
    switch (t) {
      case 'void': return undefined;
      case 'pointer': return new NativePointer(r.eax);
      case 'double': case 'float': {
        const v = r.f;
        return (v === Infinity || v === -Infinity) ? NaN : v;
      }
      case 'int64': case 'longlong': return new Int64((BigInt(r.edx >>> 0) << 32n) | BigInt(r.eax >>> 0));
      case 'uint64': case 'ulonglong': return new UInt64((BigInt(r.edx >>> 0) << 32n) | BigInt(r.eax >>> 0));
      default: return decodeArg(t, r.eax >>> 0);
    }
  }
  // JS result of a NativeCallback -> {eax, edx, f}
  function encodeRet(t, v) {
    if (t === 'void') return {};
    if (t === 'double' || t === 'float') return { f: Number(v) };
    if (t === 'int64' || t === 'uint64' || t === 'longlong' || t === 'ulonglong') {
      const b = BigInt.asUintN(64, big(v));
      return { eax: Number(b & 0xffffffffn), edx: Number(b >> 32n) };
    }
    if (v === undefined || v === null) return { eax: 0 };
    return { eax: toU32(v) };
  }

  // NativeCallback: a trap address; M.cbs maps it to the JS function and its signature
  function NativeCallback(fn, ret, args, abi) {
    const addr = M.allocTrap();
    const cb = new NativePointer(addr);
    M.cbs.set(addr, { fn, ret: ret || 'void', args: args || [], abi: abiOf(abi), kind: 'callback' });
    M.used.add('callback');
    return cb;
  }
  NativeCallback.prototype = Object.create(NativePointer.prototype);

  function nativeCall(addr, ret, argt, abi, vals) {
    addr >>>= 0;
    const cb = M.cbs.get(addr);
    if (cb && cb.kind === 'callback') {
      return cb.fn.apply(null, argt.map((t, i) => decodeArg(t, encodeArg(t, vals[i]).v)));
    }
    const ex = M.sys.byAddr.get(addr);
    if (ex) {
      const raw = argt.map((t, i) => encodeArg(t, vals[i]).v);
      const r = M.sys.invoke(ex, raw);
      return decodeRet(ret, typeof r === 'object' ? r : { eax: r >>> 0, edx: 0 });
    }
    const r = M.host.request({ op: 'call', addr, abi, ret, args: argt.map((t, i) => encodeArg(t, vals[i])) });
    if (r.error) { const e = new Error('native call 0x' + addr.toString(16) + ' failed: ' + r.error); if (r.api) e.api = r.api; throw e; }
    return decodeRet(ret, r);
  }
  function NativeFunction(address, ret, args, abi, opts) {
    const addr = toU32(address);
    const argt = args || [];
    const a = abiOf(typeof abi === 'object' && abi !== null ? abi : (abi || (opts && opts.abi)));
    const f = function () { return nativeCall(addr, ret || 'void', argt, a, Array.prototype.slice.call(arguments)); };
    Object.setPrototypeOf(f, NativeFunctionProto);
    f.v = addr;
    return f;
  }
  // a NativeFunction is callable and also a NativePointer
  const NativeFunctionProto = Object.create(Function.prototype);
  for (const k of Object.getOwnPropertyNames(NativePointer.prototype)) {
    if (k !== 'constructor') Object.defineProperty(NativeFunctionProto, k, Object.getOwnPropertyDescriptor(NativePointer.prototype, k));
  }
  const SystemFunction = NativeFunction;

  // ---- Memory ---------------------------------------------------------------------------------------------------------
  const Memory = {
    alloc(size, opts) { M.used.add('alloc'); return new NativePointer(M.heap.alloc(toU32(size))); },
    allocUtf8String(s) { const b = Buffer.from(String(s), 'utf8'); const p = M.heap.alloc(b.length + 1); S.write(p, new Uint8Array(Buffer.concat([b, Buffer.of(0)]))); return new NativePointer(p); },
    allocAnsiString(s) { const b = Buffer.from(String(s), 'latin1'); const p = M.heap.alloc(b.length + 1); S.write(p, new Uint8Array(Buffer.concat([b, Buffer.of(0)]))); return new NativePointer(p); },
    allocUtf16String(s) { const p = new NativePointer(M.heap.alloc(2 * s.length + 2)); p.writeUtf16String(s); return p; },
    protect(p, size, prot) { const a = toU32(p); for (let x = a & ~0xfff; x < a + toU32(size); x += 0x1000) S.prot.set(Math.floor(x / 0x1000), prot); return true; },
    copy(dst, src, n) { n = toU32(n); if (n) S.write(toU32(dst), S.read(toU32(src), n)); },
    dup(p, n) { const q = Memory.alloc(n); Memory.copy(q, p, n); return q; },
    patchCode(p, size, fn) { fn(new NativePointer(toU32(p))); },
    readByteArray(p, n) { return new NativePointer(toU32(p)).readByteArray(n); },
    writeByteArray(p, a) { return new NativePointer(toU32(p)).writeByteArray(a); },
    scanSync() { return []; },
    queryProtection(p) { return S.prot.get(Math.floor(toU32(p) / 0x1000)) || 'rw-'; },
  };

  // ---- modules / process ------------------------------------------------------------------------------------------------
  function modObj(m) {
    return {
      name: m.name, base: new NativePointer(m.base), size: m.size, path: 'C:\\fixvm\\' + m.name,
      getExportByName(n) { const a = m.exp(n); if (a === null) throw new Error("unable to find export '" + n + "'"); return new NativePointer(a); },
      findExportByName(n) { const a = m.exp(n); return a === null ? null : new NativePointer(a); },
      enumerateExports() { return m.exports().map((e) => ({ type: 'function', name: e.name, address: new NativePointer(e.addr) })); },
      enumerateImports() { return []; },
      enumerateRanges(prot) {
        return m.ranges().filter((r) => !prot || protMatch(r.protection, prot))
          .map((r) => ({ base: new NativePointer(r.base), size: r.size, protection: r.protection }));
      },
      enumerateSymbols() { return []; },
      findSymbolByName() { return null; },
      getSymbolByName(n) { throw new Error('no symbol ' + n); },
      ensureInitialized() {},
    };
  }
  function protMatch(have, want) {
    for (let i = 0; i < 3; i++) if (want[i] !== '-' && have[i] !== want[i]) return false;
    return true;
  }
  function findMod(name) {
    const m = M.modules.find((x) => x.name.toLowerCase() === String(name).toLowerCase());
    if (m && m.name.toLowerCase() === 'sound.dll') M.used.add('module_sound');
    return m ? modObj(m) : null;
  }
  const Process = {
    arch: 'ia32', platform: 'windows', pointerSize: 4, pageSize: 0x1000, id: 0x1f00, codeSigningPolicy: 'optional',
    isDebuggerAttached() { return false; },
    getCurrentThreadId() { return M.tid; },
    enumerateThreads() { return [{ id: M.mainTid, state: 'waiting', context: {} }, { id: M.tid, state: 'running', context: {} }]; },
    getModuleByName(n) { const m = findMod(n); if (!m) throw new Error("unable to find module '" + n + "'"); return m; },
    findModuleByName(n) { return findMod(n); },
    findModuleByAddress(p) { const a = toU32(p); const m = M.modules.find((x) => a >= x.base && a < x.base + x.size); return m ? modObj(m) : null; },
    getModuleByAddress(p) { const m = Process.findModuleByAddress(p); if (!m) throw new Error('unable to find module'); return m; },
    enumerateModules() { return M.modules.map(modObj); },
    findRangeByAddress(p) {
      const a = toU32(p);
      if (!S.isMapped(a)) return null;
      const pi = Math.floor(a / 0x1000);
      return { base: new NativePointer(pi * 0x1000), size: 0x1000, protection: S.prot.get(pi) || 'rw-' };
    },
    enumerateRanges() { return []; },
    setExceptionHandler() {},
    getCurrentDir() { return 'C:\\fixvm'; },
  };
  const Module = {
    findExportByName(mod, n) { if (mod === null) { for (const m of M.modules) { const a = m.exp(n); if (a !== null) return new NativePointer(a); } return null; } const m = findMod(mod); return m ? m.findExportByName(n) : null; },
    getExportByName(mod, n) { const r = Module.findExportByName(mod, n); if (!r) throw new Error("unable to find export '" + n + "'"); return r; },
    getGlobalExportByName(n) { return Module.getExportByName(null, n); },
    findGlobalExportByName(n) { return Module.findExportByName(null, n); },
    enumerateImports(mod) { return []; },
    enumerateExports(mod) { const m = findMod(mod); return m ? m.enumerateExports() : []; },
    findBaseAddress(mod) { const m = findMod(mod); return m ? m.base : null; },
    getBaseAddress(mod) { return Process.getModuleByName(mod).base; },
    load(n) { return Process.getModuleByName(n); },
  };

  // ---- Interceptor ------------------------------------------------------------------------------------------------------
  const Interceptor = {
    replace(target, repl) {
      const t = toU32(target);
      if (M.redirects.has(t)) throw new Error('already replaced this function');
      M.redirects.set(t, toU32(repl));
      M.hooksVersion++;
      M.used.add('replace');
    },
    replaceFast(target, repl) {
      const t = toU32(target);
      if (M.redirects.has(t)) throw new Error('already replaced this function');
      M.redirects.set(t, toU32(repl));
      const tramp = M.allocTrap();
      M.tramps.set(tramp, t);
      M.hooksVersion++;
      M.used.add('replace');
      return new NativePointer(tramp);
    },
    revert(target) {
      const t = toU32(target);
      if (M.redirects.delete(t)) M.hooksVersion++;
    },
    attach() { throw new Error('fixvm: Interceptor.attach is not modelled'); },
    detachAll() {},
    flush() {},
    breakpointKind: 'soft',
  };

  // ---- timers / misc ----------------------------------------------------------------------------------------------------
  function setTimeout(fn, ms) { M.timers.push(fn); return M.timers.length; }
  function clearTimeout(id) { if (id > 0 && id <= M.timers.length) M.timers[id - 1] = null; }
  const Thread = { sleep() {}, backtrace() { return []; } };
  const Script = { runtime: 'QJS', evaluate() { throw new Error('fixvm: Script.evaluate not modelled'); }, pin() {}, unpin() {}, setGlobalAccessHandler() {} };
  const console = { log: (...a) => M.log(a.join(' ')), warn: (...a) => M.log(a.join(' ')), error: (...a) => M.log(a.join(' ')) };
  function hexdump(p, o) { const n = (o && o.length) || 64; return Buffer.from(S.read(toU32(p), n)).toString('hex'); }

  return {
    NativePointer, NativeCallback, NativeFunction, SystemFunction, Int64, UInt64, ptr, NULL,
    int64: (v) => new Int64(v), uint64: (v) => new UInt64(v),
    Memory, Process, Module, Interceptor, Thread, Script, console, hexdump,
    setTimeout, clearTimeout, setImmediate: (fn) => setTimeout(fn, 0), setInterval: () => 0, clearInterval() {},
    send() {}, recv() { return { wait() {} }; }, rpc: { exports: {} },
    Instruction: { parse() { throw new Error('fixvm: Instruction.parse not modelled'); } },
    // for fixvm.js
    _internal: { toU32, encodeArg, decodeArg, encodeRet, decodeRet, SIZES, NativePointer },
  };
}

if (typeof module !== 'undefined') module.exports = { makeFrida };
