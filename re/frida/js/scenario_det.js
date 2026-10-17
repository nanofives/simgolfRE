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

// opt-in CLOCK LOG (cfg.clockLog = N, scenario_ab.py --clock-log N; batch b2, 2026-10-17): the first N main-thread clock
// calls are recorded natively as [return address, kind | slot] pairs (kind 1 = timeGetTime, 2 = QPC; slot = index into
// CFG.slots), counter at D+0x30, so the caller of every call between spawn and a checkpoint can be compared across boots.
// Without the key the stubs below are byte-identical to the default ones.
const CLOG_N = CFG.clockLog ? (CFG.clockLog >>> 0) : 0;
const CLOG = CLOG_N ? K(Memory.alloc(CLOG_N * 8 + 16)) : null;
function clogBytes(kind, slotIndex) {
  if (!CLOG_N) return [];
  const tag = ((kind << 24) | (slotIndex & 0xffffff)) >>> 0;
  const b = [0x8b, 0x0d, ...d(0x30),                 // mov ecx, [count]
             0x81, 0xf9, ...u32(CLOG_N),             // cmp ecx, N
             0x73, 0x00];                            // jae skip
  const jaeAt = b.length - 1;
  b.push(0x8b, 0x14, 0x24,                           // mov edx, [esp]       (return address)
         0x89, 0x14, 0xcd, ...u32(CLOG),             // mov [ecx*8 + CLOG], edx
         0xc7, 0x04, 0xcd, ...u32(CLOG.add(4)), ...u32(tag), // mov dword [ecx*8 + CLOG + 4], tag
         0xff, 0x05, ...d(0x30));                    // inc [count]
  b[jaeAt] = b.length - (jaeAt + 1);
  return b;
}

function tgtStub(realCell, slotIndex) {
  // mov eax, fs:[0x24]; cmp eax, [main]; jne real
  const b = [0x64, 0xa1, 0x24, 0, 0, 0, 0x3b, 0x05, ...d(0xc), 0x75, 0x00];
  const jneAt = b.length - 1;
  // mov eax,[T]; mov ecx,[step]; add [T],ecx; inc [tgt calls]; (clock log); cmp eax,[due]; jae slow; ret
  b.push(0xa1, ...d(0), 0x8b, 0x0d, ...d(4), 0x01, 0x0d, ...d(0), 0xff, 0x05, ...d(0x20));
  b.push(...clogBytes(1, slotIndex));
  b.push(0x3b, 0x05, ...d(8), 0x73, 0x01, 0xc3);
  // slow: push eax; call [onDue]; pop eax; ret
  b.push(0x50, 0xff, 0x15, ...d(0x1c), 0x58, 0xc3);
  b[jneAt] = b.length - (jneAt + 1);
  // real: lock inc [forwarded]; jmp [cell]
  b.push(0xf0, 0xff, 0x05, ...d(0x28), 0xff, 0x25, ...u32(realCell));
  return emit(b);
}

function qpcStub(realCell, slotIndex) {
  const b = [0x64, 0xa1, 0x24, 0, 0, 0, 0x3b, 0x05, ...d(0xc), 0x75, 0x00];
  const jneAt = b.length - 1;
  b.push(...clogBytes(2, slotIndex));
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

// ---------------------------------------------------------------- AUDIO STAND-IN (cfg.audio === 'none'; batch b2, 2026-10-17)
// Whether the PC has an audio output decides the boot's timeline: with one, initSound 0x00483e90 opens the devices, the
// sound timer thread runs and the main thread makes one more clock call before seedFromTime 0x00467110 (seed_time
// 0x0051b380 = T0 + 1, the RNG seed and the title ticker 0x00822c78 follow); without one, every open fails and
// g_soundReady 0x0083afc4 stays 0. On LIVING-PC the HDMI audio endpoint exists only while the TV is on, so stored
// references flipped between the two timelines (re/analysis/data/scenario_baseline.md). This stand-in gives sound.dll the
// device-less answers measured on this PC with no endpoint (log/c3/b2_soundprobe.js, 2026-10-17), on every PC and with
// any device, so the no-device path runs everywhere:
//   DSOUND #2 DirectSoundEnumerateA (sound.dll+0x1153f): the real enumeration runs, but only its NULL-guid entry (the
//     primary driver, which DirectSound lists with or without devices; one callback, return 1) reaches sound.dll's
//     callback; the result is the real one (DS_OK = 0 measured).
//   ole32 CoCreateInstance (sound.dll+0x11d06) for CLSID_DirectSound {47d4d946-62e8-11cf-93bc-444553540000}: S_OK and a
//     fake IDirectSound whose Initialize (slot 10, called from sound.dll+0x11d1d) returns DSERR_NODRIVER 0x88780078 and
//     whose Release (slot 2, sound.dll+0x11a2a) returns 0, the two calls and results measured; other slots return
//     DSERR_UNINITIALIZED 0x887800aa and are logged (never called on the measured path). Other CLSIDs go to the real API.
//   binkw32 WINMM waveOutOpen: MMSYSERR_BADDEVICEID 2 (this PC's result for WAVE_MAPPER and device 0 with no device).
// Each stand-in call is counted in audioStats (status()), any call off the measured sequence is logged.
const audioStats = { dsenum: 0, dsenumPassed: 0, dsenumDropped: 0, cocreateDs: 0, cocreateOther: 0, dsCalls: [], waveOutOpen: 0 };
const AUDIO_NONE = CFG.audio === 'none';
const ACB = {};
if (AUDIO_NONE) {
  let gameEnumCb = null, realEnum = null, realCoCreate = null;
  const filterCb = K(new NativeCallback(function (guid, desc, mod, ctx) {
    if (!guid.isNull()) { audioStats.dsenumDropped++; return 1; }
    audioStats.dsenumPassed++;
    return gameEnumCb(guid, desc, mod, ctx);
  }, 'int', ['pointer', 'pointer', 'pointer', 'pointer'], 'stdcall'));
  ACB.dsenum = K(new NativeCallback(function (cb, ctx) {
    audioStats.dsenum++;
    gameEnumCb = new NativeFunction(cb, 'int', ['pointer', 'pointer', 'pointer', 'pointer'], 'stdcall');
    return realEnum(filterCb, ctx);
  }, 'int', ['pointer', 'pointer'], 'stdcall'));
  ACB.dsenum.bind = (real) => { realEnum = new NativeFunction(real, 'int', ['pointer', 'pointer'], 'stdcall'); };
  // fake IDirectSound: [vtbl] -> 11 slots (stdcall, this first; argument counts of the IDirectSound methods)
  const DS_ARGS = [2, 0, 0, 3, 1, 2, 2, 0, 1, 1, 1];   // QI, AddRef, Release, CreateSoundBuffer, GetCaps, Duplicate,
                                                       // SetCooperativeLevel, Compact, GetSpeakerConfig, Set..., Initialize
  const DS_RET = (i) => i === 10 ? 0x88780078 : (i === 2 ? 0 : (i === 1 ? 1 : (i === 0 ? 0x80004002 : 0x887800aa)));
  const vtbl = K(Memory.alloc(4 * 11));
  for (let i = 0; i < 11; i++) {
    const n = DS_ARGS[i], idx = i;
    const cb = K(new NativeCallback(function () {
      if (audioStats.dsCalls.length < 64) audioStats.dsCalls.push(idx);
      return DS_RET(idx) | 0;
    }, 'int', Array(n + 1).fill('pointer'), 'stdcall'));
    vtbl.add(4 * i).writePointer(cb);
  }
  const fakeDs = K(Memory.alloc(8));
  fakeDs.writePointer(vtbl);
  ACB.cocreate = K(new NativeCallback(function (clsid, outer, ctx, riid, ppv) {
    if (clsid.readU32() === 0x47d4d946 && clsid.add(4).readU32() === 0x11cf62e8 && clsid.add(8).readU32() === 0x4544bc93 &&
        clsid.add(12).readU32() === 0x00005453) {
      audioStats.cocreateDs++;
      ppv.writePointer(fakeDs);
      return 0;
    }
    audioStats.cocreateOther++;
    return realCoCreate(clsid, outer, ctx, riid, ppv);
  }, 'int', ['pointer', 'pointer', 'uint32', 'pointer', 'pointer'], 'stdcall'));
  ACB.cocreate.bind = (real) => { realCoCreate = new NativeFunction(real, 'int', ['pointer', 'pointer', 'uint32', 'pointer', 'pointer'], 'stdcall'); };
  ACB.waveOutOpen = K(new NativeCallback(function () { audioStats.waveOutOpen++; return 2; },
    'uint32', ['pointer', 'uint32', 'pointer', 'pointer', 'pointer', 'uint32'], 'stdcall'));
  ACB.waveOutOpen.bind = () => {};
}

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
    if (s.kind === 'tgt') stub = tgtStub(c, CFG.slots.indexOf(s));
    else if (s.kind === 'qpc') stub = qpcStub(c, CFG.slots.indexOf(s));
    else if (s.kind.startsWith('audio:')) {
      const ak = s.kind.slice(6);
      if (!AUDIO_NONE || !ACB[ak]) continue;
      ACB[ak].bind(real);
      stub = ACB[ak];
    } else {
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
  // opt-in write watch (cfg.watch): a debug-register data hit arrives as EXCEPTION_SINGLE_STEP after the write
  if (d.type === 'single-step' && CFG.watch && Process.getCurrentThreadId() === MAIN) return onWatchHit(d);
  if (d.type !== 'breakpoint') {
    // opt-in (cfg.trace / cfg.watch): report any other main-thread exception before it kills the process
    if ((CFG.trace || CFG.watch) && Process.getCurrentThreadId() === MAIN) {
      let bt = [], bta = [], stack = [];
      try { bt = Thread.backtrace(d.context, Backtracer.FUZZY).slice(0, 12).map(fmtAddr); } catch (e) { bt = ['bt ' + e.message]; }
      try { bta = Thread.backtrace(d.context, Backtracer.ACCURATE).slice(0, 16).map(fmtAddr); } catch (e) { bta = ['bt ' + e.message]; }
      try { for (let i = 0; i < 64; i++) stack.push(fmtAddr(d.context.esp.add(4 * i).readPointer())); } catch (e) { stack.push('stack ' + e.message); }
      const r = d.context;
      send({ type: 'error', where: 'exception', msg: JSON.stringify({ T: T() - CFG.t0, type: d.type, pc: fmtAddr(d.address),
        mem: d.memory ? [d.memory.operation, d.memory.address.toString()] : null, bt, bta, stack,
        regs: { eax: r.eax.toString(), ebx: r.ebx.toString(), ecx: r.ecx.toString(), edx: r.edx.toString(),
                esi: r.esi.toString(), edi: r.edi.toString(), ebp: r.ebp.toString(), esp: r.esp.toString() } }) });
    }
    return false;
  }
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
    audio: AUDIO_NONE ? audioStats : undefined,
    timers: timers.map(t => [t.kind, t.id, t.elapse, t.fired || 0]), timerStats, events: events.length,
    msgs: CFG.debugMsg ? msgCounts : undefined,
    dbg: CFG.debugMsg ? (function () {
      const j = Process.findModuleByName('jgld.dll');
      return { listener: j ? j.base.add(0x12872c).readPointer().toString() : null, hwnd: HWND.toString(),
               g68: ptr(0x822d68).readU32(), g74: ptr(0x822d74).readU32(), g8c: ptr(0x822d8c).readS32() };
    })() : undefined,
  };
}
// ---------------------------------------------------------------- opt-in trace and write watch (batch c3fy2, 2026-10-16)
// cfg.trace = [[va, n], ...] (scenario_ab.py --trace): these regions are read at EVERY poll (100 virtual ms) and sent with
// it as hex, so two arms can be compared between checkpoints (scenario_ab.py tracecmp prints the first poll that
// differs). cfg.watch = [{va, size}] (scenario_ab.py --watch, at most 4, size 1/2/4): hardware WRITE watchpoints on the
// main thread, armed at the first poll; the debug registers trap after the writing instruction, so each hit logs
// [T, pc of the NEXT instruction, the first frames of the caller chain, the value now]. A hit is logged only when the
// value differs from the previous hit's (a flag word the game rewrites every frame would flood the log otherwise); the
// hit count is kept per watch. Both keys are absent in the default mode, which then runs none of this code.
const traceRegs = CFG.trace || null;
function traceHex() {
  let s = '';
  for (const [va, n] of traceRegs) {
    const b = new Uint8Array(ptr(va).readByteArray(n));
    for (let i = 0; i < b.length; i++) s += (b[i] < 16 ? '0' : '') + b[i].toString(16);
  }
  return s;
}
const watches = (CFG.watch || []).map((w, i) => ({ ...w, i, hits: 0, last: null, armed: false, err: null }));
const watchLog = [];
function watchVal(w) {
  const p = ptr(w.va);
  return w.size === 1 ? p.readU8() : (w.size === 2 ? p.readU16() : p.readU32());
}
function fmtAddr(a) {
  const m = Process.findModuleByAddress(a);
  return m ? m.name + '+' + a.sub(m.base).toString(16) : a.toString();
}
function armWatches() {
  if (!watches.length || watches.every(w => w.armed || w.err)) return;
  const th = Process.enumerateThreads().find(t => t.id === MAIN);
  for (const w of watches) {
    if (w.armed || w.err) continue;
    try {
      th.setHardwareWatchpoint(w.i, ptr(w.va), w.size, 'w');
      w.armed = true;
      w.last = watchVal(w);
      watchLog.push([T(), 'armed', w.va, w.last]);
    } catch (e) { w.err = String(e && e.message || e); }
  }
}
function onWatchHit(d) {
  // a write watchpoint on the main thread: which of ours (several can share a dword) changed
  let mine = false;
  for (const w of watches) {
    if (!w.armed) continue;
    const v = watchVal(w);
    w.hits++;
    if (v !== w.last) {
      let bt = [];
      try { bt = Thread.backtrace(d.context, Backtracer.ACCURATE).slice(0, 4).map(fmtAddr); } catch (e) { bt = ['bt ' + e.message]; }
      watchLog.push([T(), fmtAddr(d.context.pc), bt, w.va, v]);
      if (watchLog.length > 20000) watchLog.splice(0, watchLog.length - 20000);
      w.last = v;
    }
    mine = true;
  }
  return mine;
}

// opt-in WINDOW MESSAGE LOG (cfg.msgLog = N, scenario_ab.py --msg-log N; batch b2): every main-thread entry of jgld's
// window procedure windowProc 0x100692b0 (RVA 0x692b0) as [T, msg, wParam, lParam], first N entries, sent with each dump.
// Messages the harness posts arrive here too; anything else (activation when another window opens, the real mouse's
// WM_NCHITTEST / WM_MOUSEMOVE, WM_PALETTECHANGED from another palette window) shows up as a difference between two runs.
const msgLogRows = [];
let msgLogSent = 0;
if (CFG.msgLog) {
  const hookWp = (m) => {
    if (m.name.toLowerCase() !== 'jgld.dll') return;
    Interceptor.attach(m.base.add(0x692b0), {
      onEnter(a) {
        if (Process.getCurrentThreadId() !== MAIN || msgLogRows.length >= CFG.msgLog) return;
        msgLogRows.push([T() - CFG.t0, a[1].toUInt32(), a[2].toUInt32(), a[3].toUInt32()]);
      }
    });
  };
  const j = Process.findModuleByName('jgld.dll');
  if (j) hookWp(j); else Process.attachModuleObserver({ onAdded(m) { try { hookWp(m); } catch (e) { send({ type: 'error', where: 'msgLog', msg: String(e) }); } } });
}
function msgLog() {
  if (!CFG.msgLog) return undefined;
  const out = msgLogRows.slice(msgLogSent);
  msgLogSent = msgLogRows.length;
  return out;
}

// opt-in clock log (see clogBytes): every recorded call as [index, 'tgt' | 'qpc', slot module, caller as module+rva]
let clogSent = 0;
function clockLog() {
  if (!CLOG_N) return undefined;
  const n = Math.min(D.add(0x30).readU32(), CLOG_N);
  const cache = new Map(), out = [];
  for (let i = clogSent; i < n; i++) {
    const ra = CLOG.add(i * 8).readPointer(), tag = CLOG.add(i * 8 + 4).readU32();
    const k = ra.toString();
    if (!cache.has(k)) cache.set(k, fmtAddr(ra));
    const sl = CFG.slots[tag & 0xffffff];
    out.push([i, (tag >>> 24) === 1 ? 'tgt' : 'qpc', sl ? sl.module : '?', cache.get(k)]);
  }
  clogSent = n;
  return { total: D.add(0x30).readU32(), entries: out };
}

// opt-in CALL LOG (cfg.callLog = [{va, n, ra, s}], scenario_ab.py --call-log; batch c3fyl, 2026-10-17): every MAIN-thread
// entry of the golf_clean.exe function at va (absolute; the exe has no relocations) is logged as
// [T - t0, va, return address, n stack arguments..., the C string at argument s (<= 64 bytes) when s is given,
// ['ret', eax] when r is set (appended when the call returns)], only for
// the return addresses in ra when ra is given, at most cfg.callLogMax rows (default 20000; the dump reports a truncation).
// It is an extended oracle for effects the checkpoint dump does not see: the sound id and volume playSound 0x004481b0
// receives (audio is not compared), or the text and screen position drawMoneyLabels 0x0040c910 passes to
// drawCenteredText2 0x00404bc0 after the present. The rows go with each dump (`cl`) into <run>/calls.json. The entry
// hook is a Frida Interceptor (no clock call, so the virtual timeline is unchanged); the key is absent by default and
// none of this code runs then.
const callLogRows = [];
let callLogSent = 0, callLogDropped = 0;
if (CFG.callLog) {
  const max = CFG.callLogMax || 20000;
  for (const c of CFG.callLog) {
    const ras = c.ra ? new Set(c.ra.map(x => x >>> 0)) : null;
    const cb = {
      onEnter(a) {
        if (Process.getCurrentThreadId() !== MAIN) return;
        const ra = this.returnAddress.toUInt32();
        if (ras && !ras.has(ra)) return;
        if (callLogRows.length >= max) { callLogDropped++; return; }
        const row = [T() - CFG.t0, c.va, ra];
        for (let i = 0; i < c.n; i++) row.push(a[i].toUInt32());
        if (c.s !== undefined && c.s !== null) {
          try {
            let st = null;
            if (!a[c.s].isNull()) {
              // the bytes up to the first NUL (at most 64), one character per byte: readCString(size) decodes all
              // `size` bytes, so a buffer's stale tail after the NUL would be compared too
              const by = new Uint8Array(a[c.s].readByteArray(64));
              let n = by.indexOf(0);
              if (n < 0) n = by.length;
              st = String.fromCharCode.apply(null, Array.from(by.subarray(0, n)));
            }
            row.push(st);
          } catch (e) { row.push('?'); }
        }
        callLogRows.push(row);
        this.clRow = row;
      }
    };
    // onLeave only with `r`: without it Frida does not touch the return path
    if (c.r) cb.onLeave = function (r) { if (this.clRow) this.clRow.push(['ret', r.toUInt32()]); };
    Interceptor.attach(ptr(c.va), cb);
  }
}
function callLog() {
  if (!CFG.callLog) return undefined;
  const out = callLogRows.slice(callLogSent);
  callLogSent = callLogRows.length;
  return { rows: out, dropped: callLogDropped };
}
// opt-in EXTRA REGIONS (cfg.extraRegions = [[va, n], ...], scenario_ab.py --extra-regions; batch c3fyl): read on the
// blocked main thread when a poll reply asks for them (`extra`), sent as their own binary payload, so the default dump
// blob (and every comparison against an existing reference) is unchanged.
function extraRegions() {
  let total = 0;
  for (const [, n] of CFG.extraRegions) total += n;
  const buf = new Uint8Array(total);
  let off = 0;
  for (const [va, n] of CFG.extraRegions) { buf.set(new Uint8Array(ptr(va).readByteArray(n)), off); off += n; }
  return buf.buffer;
}

// BACK BUFFER (reply.bb; batch b5, 2026-10-17). At the location screen of cov_b_career_phx one boot in six showed, at the
// `offer` checkpoint and the two after it (90 virtual s), an older picture in the window than in the back buffer, while
// the game called Display::present on the full 800x600 rect every frame (log/c3/b5_evlog.js) and its back buffer was the
// same in every boot; boots run one at a time showed it in 0 of 10 (b2), boots run beside another game in 1 to 5 of 10.
// The window's contents pass through the DWM8And16BitMitigation layer game.py starts the game with, outside the game.
// When the window is current its capture equals the back buffer converted from RGB555 by (v << 3) | (v >> 2) per channel
// (0 px differ in all 12 checkpoints compared, location screen and in-game), so the frame is taken from here.
let GetObjectA = null;
function backBuffer() {
  const j = Process.findModuleByName('jgld.dll');
  const meta = { ok: false };
  if (!j) { meta.why = 'no jgld.dll'; return { meta, bits: null }; }
  const disp = j.base.add(0x128420).readPointer();
  if (disp.isNull()) { meta.why = 'no display'; return { meta, bits: null }; }
  const mode = disp.add(0x148).readPointer();
  const surf = mode.isNull() ? mode : mode.add(4).readPointer();
  if (surf.isNull()) { meta.why = 'no surface'; return { meta, bits: null }; }
  if (!GetObjectA) GetObjectA = new NativeFunction(Process.getModuleByName('gdi32.dll').getExportByName('GetObjectA'), 'int', ['pointer', 'int', 'pointer'], 'stdcall');
  const ds = Memory.alloc(84);
  const got = GetObjectA(surf.add(0x4b4).readPointer(), 84, ds);
  meta.got = got;
  meta.w = ds.add(4).readS32(); meta.h = ds.add(8).readS32(); meta.wb = ds.add(12).readS32(); meta.bpp = ds.add(18).readU16();
  meta.biHeight = ds.add(32).readS32(); meta.comp = ds.add(40).readU32();
  meta.masks = [ds.add(64).readU32(), ds.add(68).readU32(), ds.add(72).readU32()];
  const bits = ds.add(20).readPointer();
  if (got !== 84 || bits.isNull() || meta.wb <= 0 || meta.h === 0) { meta.why = 'not a DIB section'; return { meta, bits: null }; }
  meta.ok = true;
  return { meta, bits: bits.readByteArray(meta.wb * Math.abs(meta.h)) };
}

let firstPollDone = false;
function poll(now) {
  if (CFG.spawnReach && !firstPollDone) {
    // spawn mode: hits taken before the first poll (static initializers, early boot) are kept apart for the report
    firstPollDone = true;
    for (const r of reach) { r.bootHits = r.hits; r.bootOther = r.hitsOther; }
  }
  attachReach();
  if (watches.length) armWatches();
  const lg = log.splice(0, log.length);
  const msg = { type: 'poll', st: status(), log: lg, bt: where(), reach: CFG.debugBt ? reachCounts() : undefined };
  if (traceRegs) msg.tr = traceHex();
  if (watches.length) {
    msg.wl = watchLog.splice(0, watchLog.length);
    msg.ws = watches.map(w => [w.va, w.armed, w.hits, w.err]);
  }
  send(msg);
  for (;;) {
    let reply = null;
    recv('go', (m) => { reply = m; }).wait();
    if (reply.hwnd) HWND = ptr(reply.hwnd);
    if (reply.full) {
      // opt-in (scenario_ab.py C3FT_FULLDUMP=1, batch b2): golf_clean.exe's whole .data section (globals and .bss)
      send({ type: 'full', va: 0x4c1000 }, ptr(0x4c1000).readByteArray(0x381174));
      continue;
    }
    if (reply.extra && CFG.extraRegions) {
      // opt-in (scenario_ab.py --extra-regions, batch c3fyl)
      send({ type: 'extra' }, extraRegions());
      continue;
    }
    if (reply.dump) {
      send({ type: 'dump', st: status(), witness: witnessBytes(), reach: reachCounts(), clk: clockLog(), wm: msgLog(), cl: callLog() }, dumpRegions());
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
