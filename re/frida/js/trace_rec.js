// ---------------------------------------------------------------- TRACE RECORDER (phase 2.3, 2026-10-18)
// Appended after scenario_det.js (and any family extension) by re/tools/trace_tool.py, as `const TRACE_CFG = {...};` +
// this file, only when a trace is asked for. scenario_det.js itself is not changed: this code reaches its top-level
// bindings (D, CFG, HWND, timers, doAction, fireTimer, poll, dumpRegions, patchModule) because the agent is one script.
// Schema of the file it writes: re/model/trace_schema.md.
//
// RECORD: every main-thread clock call (the timeGetTime / QueryPerformanceCounter slots scenario_det.js redirects) goes
// first through a native wrapper that logs [return address, slot, value] into a ring (the value is the one the virtual
// clock stub is about to return: D+0 for timeGetTime, D+0x10/0x14 for QPC), counts it (TD+0 = n, the clock ORDINAL),
// calls onFrame at mainLoop's frame-head call (return address 0x0040fc2f) and then jumps to scenario_det's stub. The
// harness's deliveries (doAction, fireTimer), the polls and the checkpoint dumps are logged with the ordinal of the clock
// call whose onDue ran them (TD+0x30, set by a wrapper around D+0x1c). The game's input reads (window messages entering
// jgld's windowProc, the root-window handlers, the key take-and-clear readers, GetKeyState / GetAsyncKeyState /
// GetKeyboardState, the cursor after ScreenToClient, MapVirtualKeyA, Sleep, timer registrations) are logged with the
// ordinal they follow.
// REPLAY (TRACE_CFG.mode === 'replay'): the main-thread clock no longer comes from the virtual clock: the wrapper returns
// the recorded value of ordinal n (arrays read from TRACE_CFG.plan), counts a call whose return address differs from the
// recorded one (TD+0x18, first at TD+0x1c), and at the ordinals where the record delivered something calls onReplayDue,
// which re-applies the recorded deliveries (doAction, timer callbacks, polls) in their recorded order. scenario_det.js's
// own event queue never runs on the main thread then. Past the recorded ordinals the call falls back to the virtual clock.
// The same logging runs in both modes, so a replay writes a trace that can be compared byte for byte with the record.
const TRC = TRACE_CFG;
const TR_REPLAY = TRC.mode === 'replay';
const TR_CAP = 1 << 16;                     // ring entries (16 bytes each)
const TR_FRAME_RA = (TRC.frameRa || 0x0040fc2f) >>> 0;
const TD = K(Memory.alloc(Process.pageSize));
const TLOG = K(Memory.alloc(TR_CAP * 16));
TD.add(0x08).writeU32(MAIN);
TD.add(0x10).writeU32(0xffffffff);
TD.add(0x1c).writeU32(0xffffffff);
TD.add(0x30).writeU32(0xffffffff);
TD.add(0x38).writeU32(TR_FRAME_RA);
TD.add(0x48).writePointer(TLOG);
const tn = () => TD.readU32();
const trPos = () => { const c = TD.add(0x30).readU32(); return c === 0xffffffff ? tn() : c; };

// ---- replay plan: u32 N, then RA[N], VLO[N], VHI[N] (written by trace_tool.py from the recorded trace)
let TR_PLAN = null;
if (TR_REPLAY) {
  const buf = File.readAllBytes(TRC.plan);
  const n = new DataView(buf).getUint32(0, true);
  const mem = K(Memory.alloc(Math.max(12 * n, 16)));
  mem.writeByteArray(buf.slice(4, 4 + 12 * n));
  TD.add(0x14).writeU32(n);
  TD.add(0x3c).writePointer(mem);
  TD.add(0x40).writePointer(mem.add(4 * n));
  TD.add(0x44).writePointer(mem.add(8 * n));
  TR_PLAN = { n, deliveries: TRC.deliveries, idx: 0 };
}

// ---- tiny assembler (rel32 jumps with labels)
function trAsm() {
  const b = [], labels = {}, fix = [];
  return {
    b,
    e(...x) { for (const v of x) { if (Array.isArray(v)) b.push(...v); else b.push(v); } },
    label(n) { labels[n] = b.length; },
    j(op, n) { if (op === 0xe9) b.push(0xe9); else b.push(0x0f, op); b.push(0, 0, 0, 0); fix.push([b.length - 4, n]); },
    done() {
      for (const [at, n] of fix) {
        const rel = labels[n] - (at + 4);
        b[at] = rel & 0xff; b[at + 1] = (rel >>> 8) & 0xff; b[at + 2] = (rel >>> 16) & 0xff; b[at + 3] = (rel >>> 24) & 0xff;
      }
      return b;
    },
  };
}
const td = (off) => u32(TD.add(off));
const JNE = 0x85, JE = 0x84, JB = 0x82, JAE = 0x83, JMP = 0xe9;

function trStub(innerCell, slotIndex, kind) {
  const A = trAsm();
  const tag = (((kind === 'qpc' ? 2 : 1) << 24) | slotIndex) >>> 0;
  A.e(0x64, 0xa1, 0x24, 0, 0, 0, 0x3b, 0x05, td(0x08)); A.j(JNE, 'fwd');          // other threads: real path
  A.e(0xa1, td(0x00), 0x25, u32(TR_CAP - 1), 0xc1, 0xe0, 0x04, 0x03, 0x05, td(0x48)); // eax = &LOG[n % CAP]
  A.e(0x8b, 0x0c, 0x24, 0x89, 0x08, 0xc7, 0x40, 0x04, u32(tag));                   // [eax] = ra, [eax+4] = tag
  if (!TR_REPLAY) {
    if (kind === 'qpc') A.e(0x8b, 0x0d, d(0x10), 0x89, 0x48, 0x08, 0x8b, 0x0d, d(0x14), 0x89, 0x48, 0x0c);
    else A.e(0x8b, 0x0d, d(0x00), 0x89, 0x48, 0x08, 0xc7, 0x40, 0x0c, 0, 0, 0, 0);
  } else {
    A.e(0x8b, 0x0d, td(0x00), 0x3b, 0x0d, td(0x14)); A.j(JAE, 'over');             // ecx = n; n >= N: overrun
    A.e(0x8b, 0x15, td(0x3c), 0x8b, 0x14, 0x8a, 0x85, 0xd2); A.j(JE, 'ok');         // RA[n] == 0: a DLL site, not checked here
    A.e(0x3b, 0x14, 0x24); A.j(JE, 'ok');                                            // RA[n] == [esp]?
    A.e(0xff, 0x05, td(0x18), 0x83, 0x3d, td(0x1c), 0xff); A.j(JNE, 'ok');
    A.e(0x89, 0x0d, td(0x1c));
    A.label('ok');
    A.e(0x8b, 0x15, td(0x40), 0x8b, 0x14, 0x8a, 0x89, 0x50, 0x08);                 // [eax+8] = VLO[n]
    A.e(0x8b, 0x15, td(0x44), 0x8b, 0x14, 0x8a, 0x89, 0x50, 0x0c);                 // [eax+12] = VHI[n]
  }
  A.e(0xff, 0x05, td(0x00));                                                         // n++
  A.e(0x81, 0x3c, 0x24, u32(TR_FRAME_RA)); A.j(JNE, 'nf');
  A.e(0x60, 0xff, 0x15, td(0x24), 0x61);                                             // frame head: onFrame
  A.label('nf');
  if (TR_REPLAY) {
    A.e(0x83, 0x3d, td(0x0c), 0x00); A.j(JNE, 'nodue');
    A.e(0xa1, td(0x00), 0x3b, 0x05, td(0x10)); A.j(JNE, 'nodue');
    A.e(0x60, 0xff, 0x15, td(0x2c), 0x61);                                           // recorded deliveries at n
    A.label('nodue');
  }
  A.e(0xa1, td(0x00), 0x2b, 0x05, td(0x04), 0x3d, u32(TR_CAP - 256)); A.j(JB, 'nofl');
  A.e(0x60, 0xff, 0x15, td(0x28), 0x61);                                             // ring nearly full: flush
  A.label('nofl');
  if (!TR_REPLAY) {
    A.j(JMP, 'fwd');
  } else {
    A.e(0xa1, td(0x00), 0x48);                                                       // eax = n - 1
    if (kind === 'qpc') {
      A.e(0x8b, 0x15, td(0x40), 0x8b, 0x0c, 0x82, 0x8b, 0x54, 0x24, 0x04, 0x89, 0x0a,
          0x8b, 0x0d, td(0x44), 0x8b, 0x0c, 0x81, 0x89, 0x4a, 0x04, 0xb8, 1, 0, 0, 0, 0xc2, 0x04, 0x00);
    } else {
      A.e(0x8b, 0x15, td(0x40), 0x8b, 0x04, 0x82, 0xc3);
    }
    A.label('over');
    A.e(0xff, 0x05, td(0x20));
  }
  A.label('fwd');
  A.e(0xff, 0x25, u32(innerCell));
  return emit(A.done());
}

// ---- onDue wrapper (record): the ordinal of the call whose onDue runs is the position of everything it delivers
{
  const origCell = TD.add(0x34);
  origCell.writePointer(D.add(0x1c).readPointer());
  const st = emit([0xa1, ...td(0x00), 0xa3, ...td(0x30), 0xff, 0x15, ...td(0x34),
                   0xc7, 0x05, ...td(0x30), 0xff, 0xff, 0xff, 0xff, 0xc3]);
  if (!TR_REPLAY) D.add(0x1c).writePointer(st);
}

// ---------------------------------------------------------------- output buffer and records
let trBuf = new Uint8Array(1 << 20), trLen = 0, trSeq = 0, trEnded = false;
const trStats = { otherMsgs: {}, chunks: 0, replayErrors: [] };
function trNeed(n) {
  if (trLen + n <= trBuf.length) return;
  const nb = new Uint8Array(Math.max(trBuf.length * 2, trLen + n));
  nb.set(trBuf.subarray(0, trLen)); trBuf = nb;
}
function trSendChunk(final) {
  if (!trLen && !final) return;
  send({ type: 'trec', seq: trSeq++, final: !!final }, trBuf.slice(0, trLen).buffer);
  trLen = 0; trStats.chunks++;
}
// record: u8 type, u8 sub, u16 payload length, payload (little-endian)
function trRec(type, sub, fields) {
  // fields: [['u8'|'u16'|'u32'|'i32'|'u64'|'bytes', v], ...]
  let n = 0;
  for (const [k, v] of fields) n += k === 'u8' ? 1 : k === 'u16' ? 2 : (k === 'u64' ? 8 : (k === 'bytes' ? v.length : 4));
  if (n > 0xffff) throw new Error('trace record too long ' + n);
  trNeed(4 + n);
  const dv = new DataView(trBuf.buffer);
  let o = trLen;
  trBuf[o] = type; trBuf[o + 1] = sub; dv.setUint16(o + 2, n, true); o += 4;
  for (const [k, v] of fields) {
    if (k === 'u8') { trBuf[o] = v & 0xff; o += 1; }
    else if (k === 'u16') { dv.setUint16(o, v & 0xffff, true); o += 2; }
    else if (k === 'u32') { dv.setUint32(o, v >>> 0, true); o += 4; }
    else if (k === 'i32') { dv.setInt32(o, v | 0, true); o += 4; }
    else if (k === 'u64') { dv.setUint32(o, v % 0x100000000 >>> 0, true); dv.setUint32(o + 4, Math.floor(v / 0x100000000) >>> 0, true); o += 8; }
    else { trBuf.set(v, o); o += v.length; }
  }
  trLen = o;
  if (trLen >= (1 << 20)) trSendChunk(false);
}
const TRT = { CLOCKRUN: 1, FRAME: 2, DELIVER: 3, POLL: 4, MSG: 5, KEYSTATE: 6, KBSTATE: 7, CURSOR: 8, HANDLER: 9,
              TAKE: 10, SLEEP: 11, MAPVK: 12, TIMERSET: 13, END: 0x7f };

// ---- call sites: golf_clean.exe return addresses as VAs (no relocations); a DLL's as 0x80000000 | id << 24 | rva, so a
// trace does not depend on where a DLL was mapped (jgld.dll is not at its preferred base under the shim); 0xff000000 |
// low 24 bits for an address in no module of the table
const TR_MODS = ['golf_clean.exe', 'jgld.dll', 'sound.dll', 'terrain.dll', 'binkw32.dll', 'winmm.dll'];
const trSiteCache = new Map();
function trSite(ra) {
  ra = ra >>> 0;
  if (ra >= 0x00400000 && ra < 0x00900000) return ra;
  const c = trSiteCache.get(ra);
  if (c !== undefined) return c;
  let v = (0xff000000 | (ra & 0xffffff)) >>> 0;
  const m = Process.findModuleByAddress(ptr(ra));
  if (m) {
    const id = TR_MODS.indexOf(m.name.toLowerCase());
    if (id > 0) v = (0x80000000 | (id << 24) | (ra - m.base.toUInt32())) >>> 0;
  }
  trSiteCache.set(ra, v);
  return v;
}

// ---- clock runs: consecutive ordinals with the same return address and slot whose values step by the clock's step
let run = null;          // {first, count, ra, tag, v0, vlast}
const stepOf = (tag) => (tag >>> 24) === 2 ? CFG.qstep : CFG.step;
function trCloseRun() {
  if (!run) return;
  trRec(TRT.CLOCKRUN, run.tag >>> 24, [['u32', run.first], ['u32', run.count], ['u32', run.ra], ['u32', run.tag & 0xffffff],
                                         ['u64', run.v0]]);
  run = null;
}
function trFlush() {
  const n = tn();
  if (trEnded) { TD.add(0x04).writeU32(n); return; }     // after END nothing more is written
  let f = TD.add(0x04).readU32();
  for (; f < n; f++) {
    const p = TLOG.add((f % TR_CAP) * 16);
    const ra = trSite(p.readU32()), tag = p.add(4).readU32(), v = p.add(8).readU32() + p.add(12).readU32() * 0x100000000;
    if (run && run.ra === ra && run.tag === tag && run.first + run.count === f && v === run.vlast + stepOf(tag)) {
      run.count++; run.vlast = v; continue;
    }
    trCloseRun();
    run = { first: f, count: 1, ra, tag, v0: v, vlast: v };
  }
  TD.add(0x04).writeU32(n);
}
// any non-clock record: the clock calls before it go first
function trEvent(type, sub, fields) {
  if (trEnded) return;
  trFlush(); trCloseRun();
  trRec(type, sub, fields);
}

// ---- FRAME: mainLoop's frame head (0x0040fc29); input globals and a few state words as the frame starts
const TR_SNAP = [[0x00822d58, 0x40], [0x00822b98, 4], [0x00822b9c, 4], [0x0083821c, 4], [0x00838208, 4], [0x0083ab9c, 4],
                 [0x0059e7b8, 4], [0x00834170, 4], [0x00822d9c, 4]];
let trFrames = 0;
const onFrame = K(new NativeCallback(function () {
  try {
    trFlush();
    const n = tn();
    const p = TLOG.add(((n - 1) % TR_CAP) * 16);
    const parts = [];
    for (const [va, len] of TR_SNAP) parts.push(new Uint8Array(ptr(va).readByteArray(len)));
    const snap = new Uint8Array(parts.reduce((s, x) => s + x.length, 0));
    let o = 0; for (const x of parts) { snap.set(x, o); o += x.length; }
    trEvent(TRT.FRAME, 0, [['u32', trFrames++], ['u32', n], ['u32', p.add(8).readU32()], ['bytes', snap]]);
  } catch (e) { send({ type: 'error', where: 'trace onFrame', msg: String(e && e.stack || e) }); }
}, 'void', []));
const onFlush = K(new NativeCallback(function () { try { trFlush(); } catch (e) { send({ type: 'error', where: 'trace flush', msg: String(e) }); } }, 'void', []));
TD.add(0x24).writePointer(onFrame);
TD.add(0x28).writePointer(onFlush);

// ---- timer handles: Timer::startMmPeriodic / startMmOneShot 0x00486dc0 / 0x00486e40 pass the Timer object (a heap
// pointer, different in every boot) as SetTimer's nIDEvent and timeSetEvent's dwUser, and the timer procedures post 0x401
// with it as wParam (0x00486f63..0x00486f71, 0x00486fc0..0x00486fce). A trace names a timer by its REGISTRATION ORDINAL
// (0, 1, ... per SetTimer / timeSetEvent call); the raw id, mm id and object pointer map to the latest ordinal.
const trH = new Map(), trHRaw = [];
function trHandleNew(keys) { const k = trHRaw.length; trHRaw.push(keys[0]); for (const x of keys) trH.set(x, k); return k; }
const trHandle = (key) => { const k = trH.get(key); return k === undefined ? 0xffffffff : k; };

// ---- deliveries (record: wrappers of scenario_det's functions; replay: the same wrappers, called by onReplayDue)
const TRD = { cursor: 1, vkey: 2, post: 3, w32: 4, timer: 5, note: 6 };
const _doAction = doAction, _fireTimer = fireTimer, _poll = poll, _dumpRegions = dumpRegions;
doAction = function (a) {
  const k = TRD[a.k] || 0;
  let f;
  if (a.k === 'cursor') f = [['i32', a.x], ['i32', a.y]];
  else if (a.k === 'vkey') f = [['u32', a.vk], ['u32', a.st]];
  else if (a.k === 'post') f = [['u32', a.hwnd ? 2 : (HWND.isNull() ? 0 : 1)], ['u32', a.hwnd ? a.hwnd : 0], ['u32', a.msg], ['u32', a.wp], ['u32', a.lp]];
  else if (a.k === 'w32') f = [['u32', a.addr], ['u32', a.v]];
  else f = [];
  trEvent(TRT.DELIVER, k, [['u32', trPos()], ...f]);
  return _doAction(a);
};
fireTimer = function (tm) {
  const live = timers.indexOf(tm) >= 0 && !tm.dead;
  if (live) trEvent(TRT.DELIVER, TRD.timer, [['u32', trPos()], ['u32', tm.kind === 'win' ? 1 : 2],
                                               ['u32', trHandle((tm.kind === 'win' ? 'w:' : 'm:') + (tm.id >>> 0))], ['u32', T()]]);
  return _fireTimer(tm);
};
let trPolls = 0, trDumps = 0, trPendingName = null;
dumpRegions = function () {
  let name = '';
  recv('trecname', (m) => { name = m.name || ''; }).wait();
  trPendingName = name;
  trDumps++;
  return _dumpRegions();
};
const pollEvents = () => events.filter(e => e.desc && e.desc.k === 'poll').length;
poll = function (now) {
  const before = pollEvents();
  trPendingName = null;
  const pos = trPos();
  _poll(now);
  const final = pollEvents() <= before;
  const name = trPendingName === null ? null : trPendingName;
  const nb = name === null ? new Uint8Array(0) : new Uint8Array(Array.from(name).map(c => c.charCodeAt(0) & 0xff));
  trEvent(TRT.POLL, (name !== null ? 1 : 0) | (final ? 2 : 0), [['u32', pos], ['u32', trPolls++], ['u8', nb.length], ['bytes', nb]]);
  if (final) trEnd();
};
function trEnd() {
  if (trEnded) return;
  trFlush(); trCloseRun();
  trRec(TRT.END, 0, [['u32', tn()], ['u32', trFrames], ['u32', trPolls], ['u32', trDumps]]);
  trEnded = true;
  trSendChunk(true);
  send({ type: 'trec_stats', stats: trStats, n: tn(), frames: trFrames, polls: trPolls, dumps: trDumps,
         replay: TR_REPLAY ? { mismatches: TD.add(0x18).readU32(), firstMismatch: TD.add(0x1c).readU32(), overrun: TD.add(0x20).readU32(),
                               delivered: TR_PLAN.idx, planned: TR_PLAN.deliveries.length } : undefined });
}

// ---- replay: re-apply the recorded deliveries at their ordinals
let trGameHwnd = null;
function trFindHwnd() {
  if (trGameHwnd) return trGameHwnd;
  const u = Process.getModuleByName('user32.dll');
  const FindWindowExA = new NativeFunction(u.getExportByName('FindWindowExA'), 'pointer', ['pointer', 'pointer', 'pointer', 'pointer'], 'stdcall');
  const GetWindowThreadProcessId = new NativeFunction(u.getExportByName('GetWindowThreadProcessId'), 'uint32', ['pointer', 'pointer'], 'stdcall');
  const cls = Memory.allocAnsiString('JackalClass'), pid = Memory.alloc(4);
  let h = ptr(0);
  for (;;) {
    h = FindWindowExA(ptr(0), h, cls, ptr(0));
    if (h.isNull()) return null;
    GetWindowThreadProcessId(h, pid);
    if (pid.readU32() === Process.id) { trGameHwnd = h; return h; }
  }
}
function trSetNextDue() {
  const P = TR_PLAN;
  TD.add(0x10).writeU32(P.idx < P.deliveries.length ? P.deliveries[P.idx].pos >>> 0 : 0xffffffff);
}
if (TR_REPLAY) {
  const onReplayDue = K(new NativeCallback(function () {
    TD.add(0x0c).writeU32(1);
    try {
      const P = TR_PLAN, n = tn();
      TD.add(0x30).writeU32(n);
      while (P.idx < P.deliveries.length && P.deliveries[P.idx].pos === n) {
        const e = P.deliveries[P.idx++];
        if (e.k === 'act') {
          if (e.a.k === 'post' && e.cls === 1) { const h = trFindHwnd(); if (h) HWND = h; else trStats.replayErrors.push(['no hwnd', n]); }
          if (e.a.k === 'post' && e.cls === 0) HWND = ptr(0);
          doAction(e.a);
        } else if (e.k === 'timer') {
          const raw = trHRaw[e.id];
          const tm = raw === undefined ? null : timers.find(t => ((t.kind === 'win' ? 'w:' : 'm:') + (t.id >>> 0)) === raw && !t.dead);
          if (!tm) { trStats.replayErrors.push(['timer missing', n, e.tkind, e.id]); continue; }
          D.writeU32(e.t >>> 0);
          fireTimer(tm);
        } else if (e.k === 'poll') {
          D.writeU32(e.t >>> 0);
          poll(e.t >>> 0);
        }
      }
    } catch (err) {
      send({ type: 'error', where: 'trace replay', msg: String(err && err.stack || err) });
    } finally {
      TD.add(0x30).writeU32(0xffffffff);
      trSetNextDue();
      TD.add(0x0c).writeU32(0);
    }
  }, 'void', []));
  TD.add(0x2c).writePointer(onReplayDue);
  trSetNextDue();
}

// ---------------------------------------------------------------- clock slots: wrap scenario_det's stubs
const trWrapped = {};
function trWrapModule(m) {
  const key = m.name.toLowerCase();
  if (trWrapped[key]) return;
  patchModule(m);                                     // scenario_det's patch first (idempotent)
  const mine = CFG.slots.map((s, i) => [s, i]).filter(([s]) => s.module.toLowerCase() === key && (s.kind === 'tgt' || s.kind === 'qpc'));
  if (!mine.length) return;
  trWrapped[key] = true;
  for (const [s, i] of mine) {
    const slot = m.base.add(s.rva);
    const inner = slot.readPointer();
    const c = cell(inner);
    const stub = trStub(c, i, s.kind);
    Memory.protect(slot, 4, 'rw-');
    slot.writePointer(stub);
  }
}

// ---------------------------------------------------------------- consumption hooks
// golf_clean.exe's slots are read from its import directory (Frida's enumerateImports() misreports them: c3cy); fixed VAs
const TR_EXE_SLOTS = { GetKeyState: 0x004ba1a8, GetKeyboardState: 0x004ba1e8, GetAsyncKeyState: 0x004ba1f0, Sleep: 0x004ba0b0,
                       MapVirtualKeyA: 0x004ba1c0, SetTimer: 0x004ba1b8, KillTimer: 0x004ba1b4, timeSetEvent: 0x004ba218,
                       timeKillEvent: 0x004ba214 };
function trWrapSlot(slot, ret, args, impl) {
  const orig = new NativeFunction(slot.readPointer(), ret, args, 'stdcall');
  const cb = K(new NativeCallback(function (...a) {
    if (Process.getCurrentThreadId() !== MAIN || trEnded) return orig(...a);
    let ra = 0;
    try { ra = trSite(this.returnAddress.toUInt32()); } catch (e) { ra = 0; }
    return impl(orig, a, ra);
  }, ret, args, 'stdcall'));
  Memory.protect(slot, 4, 'rw-');
  slot.writePointer(cb);
}
const TRK = { GetKeyState: 1, GetAsyncKeyState: 2 };
function trHookExe() {
  const S = (n) => ptr(TR_EXE_SLOTS[n]);
  for (const api of ['GetKeyState', 'GetAsyncKeyState']) {
    trWrapSlot(S(api), 'uint32', ['int'], (o, a, ra) => {
      const r = o(a[0]);                               // eax unchanged (callers read ah)
      trEvent(TRT.KEYSTATE, TRK[api], [['u32', trPos()], ['u32', ra], ['u32', a[0]], ['u32', r & 0xffff]]);
      return r;
    });
  }
  trWrapSlot(S('GetKeyboardState'), 'int', ['pointer'], (o, a, ra) => {
    const r = o(a[0]);
    const nz = [];
    if (r && !a[0].isNull()) {
      const b = new Uint8Array(a[0].readByteArray(256));
      for (let i = 0; i < 256; i++) if (b[i]) nz.push(i, b[i]);
    }
    trEvent(TRT.KBSTATE, r ? 1 : 0, [['u32', trPos()], ['u32', ra], ['u16', nz.length / 2], ['bytes', new Uint8Array(nz)]]);
    return r;
  });
  trWrapSlot(S('Sleep'), 'void', ['uint32'], (o, a, ra) => {
    trEvent(TRT.SLEEP, 0, [['u32', trPos()], ['u32', ra], ['u32', a[0]]]);
    return o(a[0]);
  });
  trWrapSlot(S('MapVirtualKeyA'), 'uint32', ['uint32', 'uint32'], (o, a, ra) => {
    const r = o(a[0], a[1]);
    trEvent(TRT.MAPVK, 0, [['u32', trPos()], ['u32', ra], ['u32', a[0]], ['u32', a[1]], ['u32', r]]);
    return r;
  });
  // timer registrations (the slots already hold scenario_det's virtual-timer callbacks); fields: handle ordinal, period,
  // procedure, extra (SetTimer: 0; timeSetEvent: fuEvent; kills: the API result)
  trWrapSlot(S('SetTimer'), 'uint32', ['pointer', 'uint32', 'uint32', 'pointer'], (o, a, ra) => {
    const r = o(a[0], a[1], a[2], a[3]);
    const k = trHandleNew(['w:' + (r >>> 0), 'p:' + (a[1] >>> 0)]);
    trEvent(TRT.TIMERSET, 1, [['u32', trPos()], ['u32', ra], ['u32', k], ['u32', a[2]], ['u32', a[3].toUInt32()], ['u32', 0]]);
    return r;
  });
  trWrapSlot(S('KillTimer'), 'int', ['pointer', 'uint32'], (o, a, ra) => {
    const k = trHandle('w:' + (a[1] >>> 0));
    const r = o(a[0], a[1]);
    trEvent(TRT.TIMERSET, 2, [['u32', trPos()], ['u32', ra], ['u32', k], ['u32', 0], ['u32', 0], ['u32', r]]);
    return r;
  });
  trWrapSlot(S('timeSetEvent'), 'uint32', ['uint32', 'uint32', 'pointer', 'pointer', 'uint32'], (o, a, ra) => {
    const r = o(a[0], a[1], a[2], a[3], a[4]);
    const k = trHandleNew(['m:' + (r >>> 0), 'p:' + a[3].toUInt32()]);
    trEvent(TRT.TIMERSET, 3, [['u32', trPos()], ['u32', ra], ['u32', k], ['u32', a[0]], ['u32', a[2].toUInt32()], ['u32', a[4]]]);
    return r;
  });
  trWrapSlot(S('timeKillEvent'), 'uint32', ['uint32'], (o, a, ra) => {
    const k = trHandle('m:' + (a[0] >>> 0));
    const r = o(a[0]);
    trEvent(TRT.TIMERSET, 4, [['u32', trPos()], ['u32', ra], ['u32', k], ['u32', 0], ['u32', 0], ['u32', r]]);
    return r;
  });
  // root-window input handlers (vtable 0x004ba2fc slots 29..39, re/names/exe_13.tsv) and the H1 key / character readers
  const H = [[1, 0x00465580, 2], [2, 0x00465610, 2], [3, 0x00465650, 0], [4, 0x00465680, 2], [5, 0x00465720, 2], [6, 0x00465740, 0],
             [7, 0x00465750, 2], [8, 0x004657b0, 1], [9, 0x004657d0, 0], [10, 0x004657e0, 2], [11, 0x00465840, 2]];
  for (const [id, va, nargs] of H) {
    Interceptor.attach(ptr(va), {
      onEnter(a) {
        if (Process.getCurrentThreadId() !== MAIN || trEnded) return;
        trEvent(TRT.HANDLER, id, [['u32', trPos()], ['u32', nargs > 0 ? a[0].toUInt32() : 0], ['u32', nargs > 1 ? a[1].toUInt32() : 0]]);
      }
    });
  }
  for (const [id, va] of [[1, 0x0045ae50], [2, 0x0045ae30]]) {
    Interceptor.attach(ptr(va), {
      onEnter() { this.tr = Process.getCurrentThreadId() === MAIN; this.ra = trSite(this.returnAddress.toUInt32()); },
      onLeave(r) {
        if (!this.tr || trEnded) return;
        const v = r.toUInt32();
        if (v) trEvent(TRT.TAKE, id, [['u32', trPos()], ['u32', this.ra], ['u32', v]]);
      }
    });
  }
}
// input-class messages; 0x400..0xbfff are the application's own (the game posts 0x401); 0xc000.. are registered by other
// software (their numbers are session-wide atoms) and go to the statistics only
const TR_MSG_KEEP = (m) => (m >= 0x100 && m <= 0x109) || m === 0x113 || (m >= 0x200 && m <= 0x20e) || (m >= 0x400 && m < 0xc000);
function trHookJgld(m) {
  // window procedure (jgld RVA 0x692b0): main-thread input-class messages in dispatch order
  Interceptor.attach(m.base.add(0x692b0), {
    onEnter(a) {
      if (Process.getCurrentThreadId() !== MAIN || trEnded) return;
      const msg = a[1].toUInt32();
      if (!TR_MSG_KEEP(msg)) { trStats.otherMsgs[msg] = (trStats.otherMsgs[msg] || 0) + 1; return; }
      let wp = a[2].toUInt32(), sub = 0;
      if (msg === 0x401) { const k = trH.get('p:' + wp); if (k !== undefined) { wp = k; sub = 1; } }   // Timer object -> ordinal
      trEvent(TRT.MSG, sub, [['u32', trPos()], ['u32', msg], ['u32', wp], ['u32', a[3].toUInt32()]]);
    }
  });
  // the cursor as the game receives it: Display::getCursor 0x10066780 = GetCursorPos + ScreenToClient (client point)
  const slot = m.base.add(0x12c618);
  const orig = new NativeFunction(slot.readPointer(), 'int', ['pointer', 'pointer'], 'stdcall');
  const cb = K(new NativeCallback(function (h, pt) {
    const r = orig(h, pt);
    if (Process.getCurrentThreadId() === MAIN && !trEnded && !pt.isNull())
      trEvent(TRT.CURSOR, r ? 1 : 0, [['u32', trPos()], ['i32', pt.readS32()], ['i32', pt.add(4).readS32()]]);
    return r;
  }, 'int', ['pointer', 'pointer'], 'stdcall'));
  Memory.protect(slot, 4, 'rw-');
  slot.writePointer(cb);
}

for (const m of Process.enumerateModules()) trWrapModule(m);
trHookExe();
{
  const j = Process.findModuleByName('jgld.dll');
  if (j) trHookJgld(j);
}
Process.attachModuleObserver({ onAdded(m) {
  try {
    trWrapModule(m);
    if (m.name.toLowerCase() === 'jgld.dll') trHookJgld(m);
  } catch (e) { send({ type: 'error', where: 'trace patch ' + m.name, msg: String(e && e.stack || e) }); }
} });
send({ type: 'trec_ready', mode: TRC.mode, td: TD.toString() });
