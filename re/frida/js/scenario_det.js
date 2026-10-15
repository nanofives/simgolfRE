// Deterministic scenario agent for the C4 scenario ON/OFF A/B (re/frida/scenario_ab.py, batch c3ft, round 35, 2026-10-14).
//
// A canonical scenario (re/tools/scenario.py) is driven by wall-clock sleeps and posted input, so two runs of the same
// build never reach the same game state at the same moment: the RNG is seeded from timeGetTime (seedFromTime 0x00467110:
// [0x00822d9c] = timeGetTime() * 37, 0x00467110..0x00467121), every wait loop (waitTicks 0x0045bf80) spins on
// timeGetTime, the palette-cycle timer (startColorCycle 0x004832e0 -> Timer::startMmPeriodic 0x00486dc0, 50 ms,
// SetTimer 0x00486e1d with TIMERPROC 0x00486f40, which posts message 0x401) fires on real time, and posted input lands in
// whichever frame happens to be running. This agent makes the MAIN THREAD's view of time a function of its own work:
//
//  1. VIRTUAL CLOCK. The import slots of timeGetTime (golf_clean.exe, sound.dll, binkw32.dll) and QueryPerformanceCounter
//     (jgld.dll, binkw32.dll) point at native stubs. On the main thread (TEB+0x24 == the thread that runs the exe's
//     entry) each timeGetTime call returns T and advances T by cfg.step ms; each QPC call returns Q and advances Q by
//     cfg.qstep counts. Every other thread jumps to the real API without entering JavaScript (the sound timer thread,
//     DirectSound, winmm). So a wait of N ms is exactly N / step clock calls, and T after any sequence of main-thread
//     work depends only on that work.
//  2. EVENT QUEUE ON THE VIRTUAL TIMELINE. When a main-thread timeGetTime call finds T >= due, the stub calls into
//     JavaScript (onDue) on the main thread, which runs every event whose time has come, in (time, sequence) order:
//     scheduled input (virtual cursor, virtual key state, PostMessageW to the game window, all from the main thread, so
//     the messages are retrieved by the next PeekMessage of this same thread), the RNG seed write, the virtual timers,
//     and POLLS. A poll blocks the main thread (recv().wait()) while Python takes a screenshot, asks for a state dump and
//     sends the next actions; nothing the main thread owns can change while it is blocked.
//  3. VIRTUAL TIMERS. golf_clean.exe's SetTimer / KillTimer / timeSetEvent / timeKillEvent slots are NativeCallbacks: a
//     main-thread call registers the timer in this agent and returns a non-zero id (the callers test it: 0x00486e27,
//     0x00486e00); the timer fires on the virtual timeline (SetTimer: the TIMERPROC is called as (hwnd, WM_TIMER, id, T),
//     or WM_TIMER is posted when it has none; timeSetEvent: the callback is called as (id, 0, user, 0, 0) on the main
//     thread). Calls from other threads go to the real API.
//  4. STATE DUMPS. A poll's reply may ask for a dump: the regions in cfg.regions (the .sve globals of re/tools/sve.py and
//     a few extra words) are read on the blocked main thread and sent as one binary payload, with the first bytes of the
//     witness addresses (0xE9 = a live MinHook hook) and the reach counters.
//  5. REACH. cfg.reach lists functions whose entries are counted (CModule counters, no JavaScript per call): in the ON arm
//     the shim's detour (SimGolfShim_FindHookM), in the OFF arm the original entry. DLL entries are attached at the first
//     poll after their module is mapped and its hook installed, so calls before that are not counted (a lower bound).
//
// Config (prefixed to this file by scenario_ab.py as `const SCENARIO_CFG = {...};`):
//   step, qstep, t0, q0       clock parameters (ms per timeGetTime call, counts per QPC call, start values)
//   slots: [{module, rva, kind: 'tgt' | 'qpc' | 'SetTimer' | 'KillTimer' | 'timeSetEvent' | 'timeKillEvent'}]
//   regions: [[va, size], ...]   (golf_clean.exe has no relocations: VAs are absolute)
//   witness: [{module, rva}]     reach: [{module, rva, arm: 'on' | 'off'}]
//   firstPoll                  virtual time of the first poll
const CFG = SCENARIO_CFG;
const EXE = 'golf_clean.exe';
const MAIN = Process.enumerateThreads()[0].id;   // at spawn the only game thread: the one that runs the exe entry
const keep = [];
function K(x) { keep.push(x); return x; }

// ---------------------------------------------------------------- data block
// +0x00 T   +0x04 step   +0x08 due   +0x0c main tid   +0x10 Qlo   +0x14 Qhi   +0x18 qstep   +0x1c onDue callback
// +0x20 main-thread tgt calls   +0x24 main-thread qpc calls   +0x28 forwarded calls   +0x2c busy
const D = K(Memory.alloc(Process.pageSize));
D.writeU32(CFG.t0 >>> 0);
D.add(4).writeU32(CFG.step >>> 0);
D.add(8).writeU32(0xffffffff);
D.add(0xc).writeU32(MAIN);
D.add(0x10).writeU32(CFG.q0 >>> 0);
D.add(0x14).writeU32(0);
D.add(0x18).writeU32(CFG.qstep >>> 0);
const T = () => D.readU32();
const setDue = (v) => D.add(8).writeU32(v >>> 0);

const CODE = K(Memory.alloc(Process.pageSize));
let codeOff = 0;
function emit(bytes) {
  const p = CODE.add(codeOff);
  p.writeByteArray(bytes);
  codeOff += bytes.length;
  codeOff = (codeOff + 15) & ~15;
  return p;
}
const u32 = (v) => { v = (typeof v === 'number') ? v >>> 0 : ptr(v).toUInt32(); return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; };
const d = (off) => u32(D.add(off));

// real-function pointer cells (one per patched slot), read by the stubs' forward path
const CELLS = K(Memory.alloc(Process.pageSize));
let cellOff = 0;
function cell(v) { const c = CELLS.add(cellOff); cellOff += 4; c.writePointer(v); return c; }

function tgtStub(realCell) {
  // mov eax, fs:[0x24]; cmp eax, [main]; jne real
  const b = [0x64, 0xa1, 0x24, 0, 0, 0, 0x3b, 0x05, ...d(0xc), 0x75, 0x00];
  const jneAt = b.length - 1;
  // mov eax,[T]; mov ecx,[step]; add [T],ecx; inc [tgt calls]; cmp eax,[due]; jae slow; ret
  b.push(0xa1, ...d(0), 0x8b, 0x0d, ...d(4), 0x01, 0x0d, ...d(0), 0xff, 0x05, ...d(0x20), 0x3b, 0x05, ...d(8), 0x73, 0x01, 0xc3);
  // slow: push eax; call [onDue]; pop eax; ret
  b.push(0x50, 0xff, 0x15, ...d(0x1c), 0x58, 0xc3);
  b[jneAt] = b.length - (jneAt + 1);
  // real: lock inc [forwarded]; jmp [cell]
  b.push(0xf0, 0xff, 0x05, ...d(0x28), 0xff, 0x25, ...u32(realCell));
  return emit(b);
}

function qpcStub(realCell) {
  const b = [0x64, 0xa1, 0x24, 0, 0, 0, 0x3b, 0x05, ...d(0xc), 0x75, 0x00];
  const jneAt = b.length - 1;
  b.push(0x8b, 0x44, 0x24, 0x04,                 // mov eax, [esp+4]
         0x8b, 0x0d, ...d(0x10), 0x89, 0x08,     // mov ecx, [Qlo]; mov [eax], ecx
         0x8b, 0x0d, ...d(0x14), 0x89, 0x48, 0x04, // mov ecx, [Qhi]; mov [eax+4], ecx
         0x8b, 0x0d, ...d(0x18), 0x01, 0x0d, ...d(0x10), 0x83, 0x15, ...d(0x14), 0x00, // Q += qstep
         0xff, 0x05, ...d(0x24),                 // inc [qpc calls]
         0xb8, 1, 0, 0, 0, 0xc2, 0x04, 0x00);    // mov eax, 1; ret 4
  b[jneAt] = b.length - (jneAt + 1);
  b.push(0xf0, 0xff, 0x05, ...d(0x28), 0xff, 0x25, ...u32(realCell));
  return emit(b);
}

// ---------------------------------------------------------------- event queue
let seq = 0;
const events = [];          // {t, s, fn}
function schedule(t, fn) { events.push({ t: t >>> 0, s: seq++, fn }); recompute(); }
function recompute() {
  if (D.add(0x2c).readU32()) return;   // onDue recomputes when it finishes
  let m = 0xffffffff;
  for (const e of events) if (e.t < m) m = e.t;
  setDue(m);
}
const log = [];             // agent-side event log (sent with every poll)
let dueCtx = null;          // CPU context of the main thread at the clock call that ran onDue (debug backtraces)
function where() {
  if (!CFG.debugBt || !dueCtx) return null;
  try {
    return Thread.backtrace(dueCtx, Backtracer.FUZZY).slice(0, 10).map(a => {
      const m = Process.findModuleByAddress(a);
      return m ? m.name + '+' + a.sub(m.base).toString(16) : a.toString();
    });
  } catch (e) { return ['bt failed ' + e.message]; }
}
const onDue = K(new NativeCallback(function () {
  dueCtx = this.context;
  D.add(0x2c).writeU32(1);
  setDue(0xffffffff);
  try {
    for (;;) {
      const now = T();
      let best = -1;
      for (let i = 0; i < events.length; i++) {
        const e = events[i];
        if (e.t <= now && (best < 0 || e.t < events[best].t || (e.t === events[best].t && e.s < events[best].s))) best = i;
      }
      if (best < 0) break;
      const e = events.splice(best, 1)[0];
      e.fn(now);
    }
  } catch (err) {
    send({ type: 'error', where: 'onDue', msg: String(err && err.stack || err) });
  } finally {
    D.add(0x2c).writeU32(0);
    recompute();
  }
}, 'void', []));
D.add(0x1c).writePointer(onDue);

// ---------------------------------------------------------------- shim + user32 helpers
let shimFns = null;
function shim() {
  if (shimFns) return shimFns;
  const m = Process.enumerateModules().find(x => x.findExportByName('SimGolfShim_GetInfo'));
  if (!m) return null;
  shimFns = {
    cursor: new NativeFunction(m.getExportByName('SimGolfShim_SetVirtualCursor'), 'void', ['int', 'int'], 'stdcall'),
    vkey: new NativeFunction(m.getExportByName('SimGolfShim_SetVirtualKey'), 'void', ['int', 'int'], 'stdcall'),
    findHookM: new NativeFunction(m.getExportByName('SimGolfShim_FindHookM'), 'int', ['pointer', 'uint32', 'pointer', 'pointer', 'pointer'], 'stdcall'),
  };
  return shimFns;
}
const user32 = Process.getModuleByName('user32.dll');
const PostMessageW = new NativeFunction(user32.getExportByName('PostMessageW'), 'int', ['pointer', 'uint32', 'uint32', 'uint32'], 'stdcall');
let HWND = ptr(0);

function doAction(a) {
  const s = shim();
  switch (a.k) {
    case 'cursor': s.cursor(a.x, a.y); break;
    case 'vkey': s.vkey(a.vk, a.st); break;
    case 'post': {
      const r = PostMessageW(a.hwnd ? ptr(a.hwnd) : HWND, a.msg >>> 0, a.wp >>> 0, a.lp >>> 0);
      log.push([T(), 'post', a.msg.toString(16) + ' ' + (a.lp >>> 0).toString(16) + ' -> ' + r + ' hwnd ' + HWND]);
      return;
    }
    case 'w32': ptr(a.addr).writeU32(a.v >>> 0); break;
    case 'note': break;
    default: throw new Error('unknown action ' + a.k);
  }
  log.push([T(), a.k, a.tag || '']);
}

// ---------------------------------------------------------------- virtual timers (golf_clean.exe slots)
const timers = [];          // {kind, hwnd, id, elapse, proc|cb, user, periodic, next, s}
let mmId = 0x100;
let timerSeq = 0;
const REAL = {};
function armTimer(tm) {
  tm.next = (T() + Math.max(tm.elapse, 1)) >>> 0;
  const mine = tm;
  schedule(tm.next, () => fireTimer(mine));
}
function fireTimer(tm) {
  if (timers.indexOf(tm) < 0 || tm.dead) return;
  if (tm.kind === 'win') {
    if (!tm.proc.isNull()) new NativeFunction(tm.proc, 'void', ['pointer', 'uint32', 'uint32', 'uint32'], 'stdcall')(tm.hwnd, 0x113, tm.id, T());
    else PostMessageW(tm.hwnd, 0x113, tm.id, 0);
    tm.fired = (tm.fired || 0) + 1;
    if (timers.indexOf(tm) >= 0 && !tm.dead) armTimer(tm);
  } else {
    tm.fired = (tm.fired || 0) + 1;
    if (!tm.periodic) { tm.dead = true; timers.splice(timers.indexOf(tm), 1); }
    new NativeFunction(tm.cb, 'void', ['uint32', 'uint32', 'pointer', 'uint32', 'uint32'], 'stdcall')(tm.id, 0, tm.user, 0, 0);
    if (tm.periodic && timers.indexOf(tm) >= 0 && !tm.dead) armTimer(tm);
  }
}
const isMain = () => Process.getCurrentThreadId() === MAIN;
const timerStats = { set: 0, kill: 0, mmset: 0, mmkill: 0, foreign: 0 };
function makeTimerCallbacks() {
  return {
    SetTimer: K(new NativeCallback(function (hwnd, id, elapse, proc) {
      if (!isMain()) { timerStats.foreign++; return REAL.SetTimer(hwnd, id, elapse, proc); }
      timerStats.set++;
      let tid = id >>> 0;
      if (hwnd.isNull()) tid = 0x7000 + (++timerSeq);
      for (const tm of timers.slice()) if (tm.kind === 'win' && tm.hwnd.equals(hwnd) && tm.id === tid) { tm.dead = true; timers.splice(timers.indexOf(tm), 1); }
      const tm = { kind: 'win', hwnd, id: tid, elapse: elapse >>> 0, proc };
      timers.push(tm); armTimer(tm);
      log.push([T(), 'SetTimer', tid.toString(16) + '/' + (elapse >>> 0)]);
      return tid;
    }, 'uint32', ['pointer', 'uint32', 'uint32', 'pointer'], 'stdcall')),
    KillTimer: K(new NativeCallback(function (hwnd, id) {
      if (!isMain()) { timerStats.foreign++; return REAL.KillTimer(hwnd, id); }
      timerStats.kill++;
      let found = 0;
      for (const tm of timers.slice()) if (tm.kind === 'win' && tm.hwnd.equals(hwnd) && tm.id === (id >>> 0)) { tm.dead = true; timers.splice(timers.indexOf(tm), 1); found = 1; }
      log.push([T(), 'KillTimer', (id >>> 0).toString(16)]);
      return found;
    }, 'int', ['pointer', 'uint32'], 'stdcall')),
    timeSetEvent: K(new NativeCallback(function (delay, res, cb, user, flags) {
      if (!isMain() || (flags & 0x30)) { timerStats.foreign++; return REAL.timeSetEvent(delay, res, cb, user, flags); }
      timerStats.mmset++;
      const tm = { kind: 'mm', id: ++mmId, elapse: delay >>> 0, cb, user, periodic: (flags & 1) !== 0 };
      timers.push(tm); armTimer(tm);
      log.push([T(), 'timeSetEvent', tm.id + '/' + (delay >>> 0) + '/' + flags]);
      return tm.id;
    }, 'uint32', ['uint32', 'uint32', 'pointer', 'pointer', 'uint32'], 'stdcall')),
    timeKillEvent: K(new NativeCallback(function (id) {
      if (!isMain() || id < 0x100 || id > mmId) { timerStats.foreign++; return REAL.timeKillEvent(id); }
      timerStats.mmkill++;
      let found = false;
      for (const tm of timers.slice()) if (tm.kind === 'mm' && tm.id === id) { tm.dead = true; timers.splice(timers.indexOf(tm), 1); found = true; }
      log.push([T(), 'timeKillEvent', String(id)]);
      return found ? 0 : 97;
    }, 'uint32', ['uint32'], 'stdcall')),
  };
}
const TCB = makeTimerCallbacks();
const SIGS = {
  SetTimer: ['uint32', ['pointer', 'uint32', 'uint32', 'pointer']], KillTimer: ['int', ['pointer', 'uint32']],
  timeSetEvent: ['uint32', ['uint32', 'uint32', 'pointer', 'pointer', 'uint32']], timeKillEvent: ['uint32', ['uint32']],
};

// ---------------------------------------------------------------- IAT patching
const patched = {};         // module name (lower) -> true
const slotInfo = [];
function patchModule(m) {
  const key = m.name.toLowerCase();
  if (patched[key]) return;
  const mine = CFG.slots.filter(s => s.module.toLowerCase() === key);
  if (!mine.length) return;
  patched[key] = true;
  for (const s of mine) {
    const slot = m.base.add(s.rva);
    const real = slot.readPointer();
    const c = cell(real);
    let stub;
    if (s.kind === 'tgt') stub = tgtStub(c);
    else if (s.kind === 'qpc') stub = qpcStub(c);
    else {
      if (!REAL[s.kind]) REAL[s.kind] = new NativeFunction(real, SIGS[s.kind][0], SIGS[s.kind][1], 'stdcall');
      stub = TCB[s.kind];
    }
    Memory.protect(slot, 4, 'rw-');
    slot.writePointer(stub);
    slotInfo.push([m.name, s.rva, s.kind, real.toString()]);
  }
}
Memory.protect(CODE, Process.pageSize, 'rwx');
for (const m of Process.enumerateModules()) patchModule(m);
Process.attachModuleObserver({ onAdded(m) { try { patchModule(m); } catch (e) { send({ type: 'error', where: 'patch ' + m.name, msg: String(e) }); } } });

// ---------------------------------------------------------------- reach (one-shot INT3, re-armed at every poll)
// Reach is measured with one-byte breakpoints, the technique of the shim's coverage.cpp: at every poll each target's
// first byte is saved and replaced by 0xCC; the first thread to enter it raises EXCEPTION_BREAKPOINT, the handler puts the
// byte back and resumes at the same address, and the target's counter goes up by one. So reach = the number of poll
// intervals (100 virtual ms) in which the function was entered at least once, at one exception per interval at most.
// Frida Interceptor counters were tried first and killed the game twice: an inline patch relocates the first 5+ bytes,
// which breaks a function whose loop jumps back into them or whose call returns into them while the main thread is
// inside (pumpInput483c30 and the jgld window handlers, probe5 / the first debug run, 2026-10-14). One byte has neither
// problem. Target: the ORIGINAL entry in both arms. In the ON arm that byte is MinHook's 0xE9: the breakpoint saves it,
// and the first entry puts it back and resumes into the jmp, so the detour runs as usual; while armed, the install
// witness reports the saved byte. (The shim's detour was the first target, but MSVC's identical-code folding gives
// instruction-identical reimplementations ONE detour: the eight c3cn list helpers shared one and all read reach 5 in the
// ON arm while their originals read 0 in the OFF arm, and Sprite::getField30 the same.) Hits are counted per thread
// kind: only main-thread entries are on the virtual timeline whose effects are compared; entries from other threads
// (the sound timer thread runs on real time) are reported separately.
const reach = CFG.reach.map((r, i) => ({ ...r, i, state: 'pending', target: null, hits: 0, hitsOther: 0, armed: false, orig: 0 }));
const bpByAddr = new Map();   // address string -> [reach entries]
let lastRearm = null;
const protectedPages = new Set();
function writable(p) {
  const page = p.and(ptr(Process.pageSize - 1).not());
  const k = page.toString();
  if (protectedPages.has(k)) return;
  Memory.protect(page, Process.pageSize, 'rwx');
  protectedPages.add(k);
}
Process.setExceptionHandler(function (d) {
  if (d.type !== 'breakpoint') return false;
  const list = bpByAddr.get(d.address.toString());
  if (!list) return false;
  const e = list[0];
  if (e.armed) {
    d.address.writeU8(e.orig);
    for (const x of list) x.armed = false;
  }
  const onMain = Process.getCurrentThreadId() === MAIN;
  for (const x of list) { if (onMain) x.hits++; else x.hitsOther++; }
  d.context.pc = d.address;
  return true;
});
function moduleBase(name) {
  if (name.toLowerCase() === EXE) return ptr(0);
  const m = Process.findModuleByName(name);
  return m ? m.base : null;
}
// first byte of each witness address as seen BEFORE this agent attached anything there (an OFF-arm reach counter puts
// Frida's own jmp at the original entry, which would read as 0xE9)
const preBytes = {};
function capturePre(module, rva) {
  const k = module.toLowerCase() + ':' + rva;
  if (k in preBytes) return;
  const base = moduleBase(module);
  if (base === null) return;
  preBytes[k] = base.add(rva).readU8();
}
function attachReach() {
  const s = shim();
  for (const w of CFG.witness) capturePre(w.module, w.rva);
  for (const r of reach) {
    if (r.state !== 'pending') continue;
    const base = moduleBase(r.module);
    if (base === null) continue;
    capturePre(r.module, r.rva);
    const target = base.add(r.rva);
    if (r.arm === 'on') {
      // arm only once the hook is installed (a DLL's hooks install after LoadLibraryA returns), so the saved byte is 0xE9
      if (!s) continue;
      const det = Memory.alloc(4), orig = Memory.alloc(4), inst = Memory.alloc(4);
      inst.writeU32(0);
      if (!s.findHookM(Memory.allocAnsiString(r.module), r.rva, det, orig, inst)) { r.state = 'no hook'; continue; }
      if (!inst.readU32()) continue;
      r.detour = det.readPointer().toString();
    }
    try {
      writable(target);
      r.target = target;
      r.orig = target.readU8();
      const k = target.toString();
      if (!bpByAddr.has(k)) bpByAddr.set(k, []);
      const list = bpByAddr.get(k);
      if (list.length) r.orig = list[0].orig;   // a shared detour: its byte was saved by the first entry
      list.push(r);
      r.state = 'armed';
    } catch (e) { r.state = 'arm failed: ' + e.message; }
  }
  // (re)arm every target whose breakpoint fired since the last re-arm, once per cfg.rearmMs of virtual time (re-arming
  // 2,886 targets at every 100 ms poll made a 600 s course_season arm take 8+ minutes); a byte someone else changed is
  // left alone
  const now = T();
  if (lastRearm !== null && now - lastRearm < (CFG.rearmMs || 0)) return;
  lastRearm = now;
  for (const [k, list] of bpByAddr) {
    const e = list[0];
    if (e.armed) continue;
    const p = e.target;
    if (p.readU8() !== e.orig) { for (const x of list) x.state = 'byte changed'; continue; }
    p.writeU8(0xcc);
    for (const x of list) x.armed = true;
  }
}
function reachCounts() {
  if (CFG.spawnReach) return reach.map(r => [r.module, r.rva, r.state, r.hits, r.hitsOther, r.bootHits, r.bootOther, r.armedAt]);
  return reach.map(r => [r.module, r.rva, r.state, r.hits, r.hitsOther]);
}
const reachByKey = new Map(reach.map(r => [r.module.toLowerCase() + ':' + r.rva, r]));
function armedOrig(module, rva) {
  const r = reachByKey.get(module.toLowerCase() + ':' + rva);
  return (r && r.target && r.armed) ? r.orig : null;
}

// ---------------------------------------------------------------- polls and dumps
function witnessBytes() {
  // [module, rva, byte before any agent attach (-1: module not mapped then), live byte now]
  return CFG.witness.map(w => {
    const base = moduleBase(w.module);
    const k = w.module.toLowerCase() + ':' + w.rva;
    const pre = (k in preBytes) ? preBytes[k] : -1;
    if (base === null) return [w.module, w.rva, pre, -1];
    const saved = armedOrig(w.module, w.rva);       // our breakpoint sits there: report the byte it replaced
    if (saved !== null) return [w.module, w.rva, pre, saved];
    try { return [w.module, w.rva, pre, base.add(w.rva).readU8()]; } catch (e) { return [w.module, w.rva, pre, -2]; }
  });
}
function dumpRegions() {
  let total = 0;
  for (const [, n] of CFG.regions) total += n;
  const buf = new Uint8Array(total);
  let off = 0;
  for (const [va, n] of CFG.regions) {
    buf.set(new Uint8Array(ptr(va).readByteArray(n)), off);
    off += n;
  }
  return buf.buffer;
}
function status() {
  return {
    T: T(), tgt: D.add(0x20).readU32(), qpc: D.add(0x24).readU32(), forwarded: D.add(0x28).readU32(),
    timers: timers.map(t => [t.kind, t.id, t.elapse, t.fired || 0]), timerStats, events: events.length,
    msgs: CFG.debugMsg ? msgCounts : undefined,
    dbg: CFG.debugMsg ? (function () {
      const j = Process.findModuleByName('jgld.dll');
      return { listener: j ? j.base.add(0x12872c).readPointer().toString() : null, hwnd: HWND.toString(),
               g68: ptr(0x822d68).readU32(), g74: ptr(0x822d74).readU32(), g8c: ptr(0x822d8c).readS32() };
    })() : undefined,
  };
}
let firstPollDone = false;
function poll(now) {
  if (CFG.spawnReach && !firstPollDone) {
    // spawn mode: hits taken before the first poll (static initializers, early boot) are kept apart for the report
    firstPollDone = true;
    for (const r of reach) { r.bootHits = r.hits; r.bootOther = r.hitsOther; }
  }
  attachReach();
  const lg = log.splice(0, log.length);
  send({ type: 'poll', st: status(), log: lg, bt: where(), reach: CFG.debugBt ? reachCounts() : undefined });
  for (;;) {
    let reply = null;
    recv('go', (m) => { reply = m; }).wait();
    if (reply.hwnd) HWND = ptr(reply.hwnd);
    if (reply.dump) {
      send({ type: 'dump', st: status(), witness: witnessBytes(), reach: reachCounts() }, dumpRegions());
      continue;
    }
    for (const a of (reply.actions || [])) {
      const act = a;
      if (!a.dt) doAction(act); else schedule(now + a.dt, () => doAction(act));
    }
    if (reply.next) schedule(now + reply.next, poll);
    break;
  }
}
schedule(CFG.firstPoll, poll);

// SPAWN-TIME REACH (cfg.spawnReach, scenario_ab.py --spawn-reach; batch s36c, round 36). Game.start() loads this agent
// while the spawned process is still suspended: the shim's DllMain has installed its hooks (0xE9 at hooked entries,
// measured 2026-10-15) but the main thread has not reached the exe's entry, so __cinit 0x004a50db and the boot have not
// run. Without this block the breakpoints are first armed at the first poll (cfg.firstPoll), after all of that code. Here
// every target whose module is mapped now (and, in the ON arm, whose hook is installed) is armed before the resume; DLLs
// loaded later (jgld.dll, sound.dll) are armed at the polls as before. lastRearm is reset so the first poll re-arms and
// the re-arm grid of the default mode (first poll, then every cfg.rearmMs) is kept.
if (CFG.spawnReach) {
  attachReach();
  for (const r of reach) r.armedAt = r.state === 'armed' ? 'spawn' : 'poll';
  lastRearm = null;
}

// debug: messages dispatched on the main thread (DispatchMessageA), by message number
const msgCounts = {};
if (CFG.debugMsg) {
  Interceptor.attach(user32.getExportByName('DispatchMessageA'), {
    onEnter(a) {
      if (Process.getCurrentThreadId() !== MAIN) return;
      const msg = a[0].add(4).readU32();
      let m = msg.toString(16);
      if (msg >= 0x200 && msg <= 0x20a) m += '@' + a[0].readPointer() + '/' + (this.returnAddress.sub(Process.findModuleByAddress(this.returnAddress).base)).toString(16);
      msgCounts[m] = (msgCounts[m] || 0) + 1;
    }
  });
}

rpc.exports = {
  // where the main thread is (for a run that stopped polling: a hang outside the clock)
  mainWhere() {
    const t = Process.enumerateThreads().find(x => x.id === MAIN);
    if (!t) return null;
    const fmt = (a) => { const m = Process.findModuleByAddress(a); return m ? m.name + '+' + a.sub(m.base).toString(16) : a.toString(); };
    let bt = [];
    try { bt = Thread.backtrace(t.context, Backtracer.FUZZY).slice(0, 12).map(fmt); } catch (e) { bt = ['bt failed ' + e.message]; }
    return { pc: fmt(t.context.pc), bt, T: T() - CFG.t0 };
  },
  info() { return { main: MAIN, slots: slotInfo, status: status(), code: CODE.toString(), data: D.toString() }; },
};
send({ type: 'ready', main: MAIN, slots: slotInfo });
