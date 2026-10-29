'use strict';
// Fake system DLLs of the fixture VM. Each module sits at a fixed base inside the trap range (0x7a000000 + 0x100000 * i);
// every export is a 16-byte slot filled with int3 whose address is a trap: the emulator stops there and asks the VM to run
// the export's JS body (or a fixture's NativeFunction on that address calls the body directly).
//
// The bodies are deterministic stand-ins for the Win32 calls whose result does not depend on the live system (the CRT's
// heap, TLS, critical sections, code pages, version), written from the Win32 documentation. Anything else (clocks, devices,
// files, windows, threads, COM) has NO body: a call raises `unmodelled import <DLL>!<name>`, which ends the vector and is
// the "missing API" the census names. In the live A/B run such a call reached the real system, so its result there is not
// reproducible from the fixture; a fixture that wanted it reproducible stood it in with __importStandIn (whose stubs then
// call NativeCallbacks, not these bodies).
//
// Body signature: impl(args /* uint32[] */, X) -> uint32 | {eax, edx}; X = { S (address space), M (machine), sys }.
const SYS_BASE = 0x7a000000, SYS_STRIDE = 0x100000, SYS_SIZE = 0x10000;
const MODULES = ['kernel32.dll', 'user32.dll', 'winmm.dll', 'ole32.dll', 'dsound.dll', 'ntdll.dll', 'gdi32.dll', 'msvcrt.dll',
  'advapi32.dll', 'kernelbase.dll'];
// a DLL name used by the import directory that maps onto a module above
const ALIAS = { 'kernel32': 'kernel32.dll', 'user32': 'user32.dll', 'winmm': 'winmm.dll', 'ole32': 'ole32.dll', 'dsound': 'dsound.dll' };

function u16le(s) { const b = Buffer.alloc(2 * s.length + 2); for (let i = 0; i < s.length; i++) b.writeUInt16LE(s.charCodeAt(i), 2 * i); return b; }

class Sys {
  constructor(M) {
    this.M = M;
    this.mods = MODULES.map((name, i) => ({ name, base: SYS_BASE + SYS_STRIDE * i, size: SYS_SIZE, byName: new Map(), next: 0 }));
    this.byAddr = new Map();          // export address -> {mod, name, nargs, abi, impl}
    this.handles = 0x100;             // fake kernel handles (events, heaps): 0x100, 0x104, ...
    this.tls = [];
    this.statics = new Map();         // name -> address of a constant string block the VM allocated
    this.heaps = new Map();           // block address -> size (HeapAlloc / VirtualAlloc of the emulated code)
  }
  // VM module records (fixvm.js puts them in M.modules)
  moduleRecords() {
    return this.mods.map((m) => ({
      name: m.name, base: m.base, size: m.size,
      exp: (n) => this.exportAddr(m.name, n),
      exports: () => [...m.byName.values()].map((e) => ({ name: e.name, addr: e.addr })),
      ranges: () => [{ base: m.base, size: m.size, protection: 'r-x' }],
    }));
  }
  modByName(dll) {
    const k = String(dll).toLowerCase();
    return this.mods.find((m) => m.name === k || m.name === ALIAS[k] || m.name === k + '.dll') || null;
  }
  // the export's slot, created on first use (an unknown name gets a slot whose call raises unmodelled import)
  exportAddr(dll, name) {
    const m = this.modByName(dll);
    if (!m) return null;
    let e = m.byName.get(name);
    if (!e) {
      const addr = m.base + 0x1000 + 16 * m.next++;
      const def = (BODIES[m.name] || {})[name];
      e = { mod: m.name, name, addr, nargs: def ? def[0] : null, abi: def && def[2] ? def[2] : 'stdcall', impl: def ? def[1] : null };
      m.byName.set(name, e);
      this.byAddr.set(addr, e);
    }
    return e.addr;
  }
  // an export whose address is native code (not a trap)
  defineNative(dll, name, addr) {
    const m = this.modByName(dll);
    m.byName.set(name, { mod: m.name, name, addr, nargs: null, abi: 'cdecl', impl: null, native: true });
  }
  // an export with a JS body defined by the VM (not in BODIES)
  defineBody(dll, name, nargs, abi, impl) {
    const a = this.exportAddr(dll, name);
    const e = this.byAddr.get(a);
    e.nargs = nargs; e.abi = abi; e.impl = impl;
  }
  invoke(e, args) {
    if (!e.impl) throw new UnmodelledImport(e);
    this.M.importsCalled.push(e.mod + '!' + e.name);
    return e.impl(args, { S: this.M.space, M: this.M, sys: this });
  }
  staticString(key, bytes) {
    let a = this.statics.get(key);
    if (a === undefined) { a = this.M.emuHeap.alloc(bytes.length); this.M.space.write(a, new Uint8Array(bytes)); this.statics.set(key, a); }
    return a;
  }
}

class UnmodelledImport extends Error {
  constructor(e) { super('unmodelled import ' + e.mod.toUpperCase().replace('.DLL', '.dll') + '!' + e.name); this.api = e.mod + '!' + e.name; }
}

const ok1 = () => 1, ret0 = () => 0;
function host(api) {
  return (a, X) => {
    const r = X.M.host.request({ op: 'hostcall', api, args: a });
    if (r.error) throw new Error('hostcall ' + api + ': ' + r.error);
    if (r.lastError !== undefined) X.S.w32(lastErr(X), r.lastError);
    return r.eax >>> 0;
  };
}
function crit(X, p) { return { S: X.S, p }; }
// RTL_CRITICAL_SECTION (Windows 8+ layout and lock-bit protocol): +0 DebugInfo, +4 LockCount (-1 free; bit 0 clear =
// held), +8 RecursionCount, +0xc OwningThread, +0x10 LockSemaphore, +0x14 SpinCount. InitializeCriticalSection without debug
// info stores DebugInfo -1 on Windows 8 and later [U-1404: the SpinCount flags the real call stores are not modelled].
function initCrit(a, X) {
  const S = X.S, p = a[0];
  S.w32(p, 0xffffffff); S.w32(p + 4, 0xffffffff); S.w32(p + 8, 0); S.w32(p + 12, 0); S.w32(p + 16, 0); S.w32(p + 20, 0);
  return 0;
}
function enterCrit(a, X) {
  const S = X.S, p = a[0], tid = X.M.tid;
  if (S.u32(p + 12) === tid) { S.w32(p + 8, S.u32(p + 8) + 1); return 0; }
  const lc = S.u32(p + 4);
  if (!(lc & 1)) throw new Error('EnterCriticalSection: section held by another thread (single-threaded VM)');
  S.w32(p + 4, (lc & ~1) >>> 0); S.w32(p + 12, tid); S.w32(p + 8, 1);
  return 0;
}
function leaveCrit(a, X) {
  const S = X.S, p = a[0];
  const r = (S.u32(p + 8) - 1) >>> 0;
  S.w32(p + 8, r);
  if (r === 0) { S.w32(p + 12, 0); S.w32(p + 4, (S.u32(p + 4) + 1) >>> 0); }
  return 0;
}
function lastErr(X) { return X.M.teb + 0x34; }
function heapAlloc(X, n, zero) {
  const p = X.M.emuHeap.alloc(Math.max(n, 1));
  X.sys.heaps.set(p, n);
  if (!zero) X.S.write(p, new Uint8Array(n).fill(0));      // [U-1405] HeapAlloc without HEAP_ZERO_MEMORY: zero here, undefined in Windows
  return p;
}
function mapped(X, p, n) {
  if (!p) return false;
  for (let a = p - (p % 0x1000); a < p + Math.max(n, 1); a += 0x1000) if (!X.S.isMapped(a)) return false;
  return true;
}

// name -> [stack argument count, body, abi]
const BODIES = {
  'kernel32.dll': {
    GetVersion: [0, () => 0x23f00206],                     // Windows 8+ without a manifest reports 6.2 build 9200
    GetVersionExA: [1, (a, X) => { const S = X.S; S.w32(a[0] + 4, 6); S.w32(a[0] + 8, 2); S.w32(a[0] + 12, 9200); S.w32(a[0] + 16, 2); S.write(a[0] + 20, new Uint8Array(128)); return 1; }],
    GetCommandLineA: [0, (a, X) => X.sys.staticString('cmdline', Buffer.from('golf_clean.exe\0', 'latin1'))],
    GetEnvironmentStrings: [0, (a, X) => X.sys.staticString('env', Buffer.from('A=1\0\0', 'latin1'))],
    GetEnvironmentStringsW: [0, (a, X) => X.sys.staticString('envw', Buffer.concat([u16le('A=1'), Buffer.alloc(2)]))],
    FreeEnvironmentStringsA: [1, ok1], FreeEnvironmentStringsW: [1, ok1],
    GetStartupInfoA: [1, (a, X) => { X.S.write(a[0], new Uint8Array(0x44)); X.S.w32(a[0], 0x44); return 0; }],
    GetStdHandle: [1, ret0], GetFileType: [1, ret0], SetHandleCount: [1, (a) => a[0]],
    GetModuleFileNameA: [3, (a, X) => { const s = Buffer.from('C:\\SimGolf\\sound.dll\0', 'latin1'); const n = Math.min(s.length, a[2]); X.S.write(a[1], new Uint8Array(s.subarray(0, n))); return n - 1; }],
    GetModuleHandleA: [1, (a, X) => {
      if (!a[0]) return 0x00400000;
      const n = X.S.cstring(a[0]).toLowerCase();
      const m = X.M.modules.find((x) => x.name.toLowerCase() === n || x.name.toLowerCase() === n + '.dll');
      return m ? m.base : 0;
    }],
    GetProcAddress: [2, (a, X) => {
      const m = X.M.modules.find((x) => x.base === a[0]);
      if (!m || a[1] < 0x10000) return 0;
      const r = m.exp(X.S.cstring(a[1]));
      return r === null ? 0 : r;
    }],
    LoadLibraryA: [1, (a, X) => { const m = X.sys.modByName(X.S.cstring(a[0]).toLowerCase().replace(/^.*\\/, '')); return m ? m.base : 0; }],
    GetACP: [0, host('GetACP')], GetOEMCP: [0, host('GetOEMCP')], IsValidCodePage: [1, host('IsValidCodePage')],
    GetCPInfo: [2, host('GetCPInfo')], IsProcessorFeaturePresent: [1, host('IsProcessorFeaturePresent')],
    HeapCreate: [3, (a, X) => { const h = X.sys.handles; X.sys.handles += 4; return 0x00510000 + h; }],
    HeapDestroy: [1, ok1],
    HeapAlloc: [3, (a, X) => heapAlloc(X, a[2], (a[1] & 8) !== 0)],
    HeapFree: [3, ok1],
    HeapSize: [3, (a, X) => { const n = X.sys.heaps.get(a[2]); return n === undefined ? 0xffffffff : n; }],
    HeapReAlloc: [4, (a, X) => {
      const old = X.sys.heaps.get(a[2]);
      if (old === undefined) return 0;
      const p = heapAlloc(X, a[3], (a[1] & 8) !== 0);
      const n = Math.min(old, a[3]);
      if (n) X.S.write(p, X.S.read(a[2], n));
      return p;
    }],
    VirtualAlloc: [4, (a, X) => { const n = (a[1] + 0xfff) & ~0xfff; const p = X.M.emuHeap.allocPages(n); X.sys.heaps.set(p, n); return p; }],
    VirtualFree: [3, ok1],
    // protection is recorded, not enforced: the old value written is the recorded one ('rw-' = PAGE_READWRITE 4)
    VirtualProtect: [4, (a, X) => { const P = { 'r--': 2, 'rw-': 4, 'r-x': 0x20, 'rwx': 0x40 }; const pi = Math.floor(a[0] / 0x1000); if (a[3]) X.S.w32(a[3], P[X.S.prot.get(pi) || 'rw-'] || 4); return 1; }],
    // TLS indexes from M.tlsFirst (the live process's first free index is not known to the fixture; see sound_replay.md)
    TlsAlloc: [0, (a, X) => { const i = X.M.tlsFirst + X.sys.tls.length; X.sys.tls.push(0); return i; }],
    TlsGetValue: [1, (a, X) => { X.S.w32(lastErr(X), 0); return X.sys.tls[a[0] - X.M.tlsFirst] || 0; }],
    TlsSetValue: [2, (a, X) => { X.sys.tls[a[0] - X.M.tlsFirst] = a[1]; return 1; }],
    TlsFree: [1, ok1],
    InitializeCriticalSection: [1, initCrit], EnterCriticalSection: [1, enterCrit], LeaveCriticalSection: [1, leaveCrit],
    DeleteCriticalSection: [1, ret0],
    GetCurrentThreadId: [0, (a, X) => X.M.tid], GetCurrentThread: [0, () => 0xfffffffe], GetCurrentProcess: [0, () => 0xffffffff],
    GetLastError: [0, (a, X) => X.S.u32(lastErr(X))], SetLastError: [1, (a, X) => { X.S.w32(lastErr(X), a[0]); return 0; }],
    InterlockedIncrement: [1, (a, X) => { const v = (X.S.u32(a[0]) + 1) >>> 0; X.S.w32(a[0], v); return v; }],
    InterlockedDecrement: [1, (a, X) => { const v = (X.S.u32(a[0]) - 1) >>> 0; X.S.w32(a[0], v); return v; }],
    Sleep: [1, ret0], OutputDebugStringA: [1, ret0], SetUnhandledExceptionFilter: [1, ret0],
    IsBadReadPtr: [2, (a, X) => mapped(X, a[0], a[1]) ? 0 : 1], IsBadWritePtr: [2, (a, X) => mapped(X, a[0], a[1]) ? 0 : 1],
    IsBadCodePtr: [1, (a, X) => mapped(X, a[0], 1) ? 0 : 1],
    CreateEventA: [4, (a, X) => { const h = X.sys.handles; X.sys.handles += 4; return h; }],
    SetEvent: [1, ok1], ResetEvent: [1, ok1], CloseHandle: [1, ok1], SetThreadPriority: [2, ok1],
    GetUserDefaultLCID: [0, () => 0x409],
    // Pure locale / code-page functions: answered by the REAL Windows API of the machine running the replay (the emulator
    // side calls it through ctypes: sound_emu.py HOSTCALLS), because their tables are the operating system's [U-1406].
    MultiByteToWideChar: [6, host('MultiByteToWideChar')], WideCharToMultiByte: [8, host('WideCharToMultiByte')],
    GetStringTypeW: [4, host('GetStringTypeW')], GetStringTypeA: [5, host('GetStringTypeA')],
    LCMapStringW: [6, host('LCMapStringW')], LCMapStringA: [6, host('LCMapStringA')],
    CompareStringW: [6, host('CompareStringW')], CompareStringA: [6, host('CompareStringA')],
    GetLocaleInfoA: [4, host('GetLocaleInfoA')], GetLocaleInfoW: [4, host('GetLocaleInfoW')],
  },
  'ole32.dll': {},
  'user32.dll': {},
  'winmm.dll': {},
  'dsound.dll': {},
};

module.exports = { Sys, UnmodelledImport, SYS_BASE, SYS_STRIDE, MODULES };
