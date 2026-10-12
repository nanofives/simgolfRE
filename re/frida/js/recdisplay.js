// Thread guard for fixture displays swapped into 0x0083ad50 (2026-10-12; finding 4.1 of log/c3/c3ef_notes.txt).
// Loaded by diff_hook.py after freeze.js, before every fixture fragment.
//
// Why: the game's multimedia-timer callback 0x00486f90 runs on the winmm thread and, for a timer without a callback
// (+4 == 0) whose +0x24 is 0, reads the display global 0x0083ad50 (0x00486fba) and calls its slot +0x20 (0x00486fca),
// then PostMessageA (0x00486fce) and sets +0x24 (0x00486fd4). A key that swaps 0x0083ad50 for a recording display lets
// that thread run the recorder's slot mid-arm: it writes into compared state and hands the live timer a fake HWND (its
// message is lost and its +0x24 stays 1). The first thread is frozen during such keys, the winmm thread is not.
//
//   __recDisplay.guard(fake[, real])   returns fake. From now on every call through fake's vtable on a thread other
//                                      than the caller's (the key's A/B thread, which runs the fixture and both arms)
//                                      goes to the REAL display: ecx = real, `jmp [[real] + slot]`. Calls on the A/B
//                                      thread go to fake's own slot, read from its own vtable at call time (slots a
//                                      fixture patches later still count). real defaults to the display 0x0083ad50
//                                      holds now (or the last genuine one seen, when it already holds a fake or 0).
//   __recDisplay.forwarded(fake)       how many calls were forwarded since fake was last guarded
//   __recDisplay.status                { guards, real }
//
// How: guard() replaces fake's vtable pointer (dword 0) by the vtable of one of POOL native thunk tables built at load
// time (no allocation, no Win32 call, safe while the first thread is frozen). Slot s of a table is `mov edx, s;
// jmp common`; common reads the thread id from the TEB (fs:[0x24]) and either jumps through fake's original vtable or
// through the real display's, stack untouched, so no slot argument count is needed (thiscall: the callee pops) and the
// winmm thread never enters JavaScript. The other fields of fake are untouched; dword 0 is the same in both arms of a key.
// With no real display known, or 0x0083ad50 = 0 at load and since, a foreign call runs fake's slot as before.
(function () {
  if (globalThis.__recDisplay) return;
  const DISPLAY = ptr(0x0083ad50);
  const POOL = 32;                 // tables reused round robin: a key guards a few displays at most
  const SLOTS = 0x100;             // 0x400 bytes of vtable; jgld's Display vtable 0x1011d640 has 86 slots (0x158)
  const CTX = 0x1000;              // per table: +0 data, +0x20 common code, +0x80 slot thunks (10 bytes each), +0xc00 vtable
  const DATA_TID = 0, DATA_ORIGVT = 4, DATA_REAL = 8, DATA_FWD = 0xc, DATA_OWNER = 0x10;
  const OFF_COMMON = 0x20, OFF_THUNKS = 0x80, OFF_VT = 0xc00;

  const base = Memory.alloc(POOL * CTX);
  Memory.protect(base, POOL * CTX, 'rwx');
  globalThis.__diffKeepAlive && globalThis.__diffKeepAlive.push(base);

  function le(v) { v = v >>> 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }
  for (let c = 0; c < POOL; c++) {
    const ctx = base.add(c * CTX);
    const d = ctx.toUInt32();
    const common = ctx.add(OFF_COMMON);
    // common (edx = slot offset):
    //   mov eax, fs:[0x24]        ; current thread id
    //   cmp eax, [d+TID]
    //   je ab
    //   lock inc dword [d+FWD]
    //   mov eax, [d+REAL]
    //   test eax, eax
    //   je ab                      ; no real display known: behave as before the guard
    //   mov ecx, eax
    //   mov eax, [eax]
    //   jmp [eax+edx]              ; the real display's slot, ecx = real
    // ab:
    //   mov eax, [d+ORIGVT]
    //   jmp [eax+edx]              ; fake's own slot, ecx = fake
    common.writeByteArray([].concat(
      [0x64, 0xa1, 0x24, 0x00, 0x00, 0x00],
      [0x3b, 0x05], le(d + DATA_TID),
      [0x74, 0x17],
      [0xf0, 0xff, 0x05], le(d + DATA_FWD),
      [0xa1], le(d + DATA_REAL),
      [0x85, 0xc0],
      [0x74, 0x07],
      [0x8b, 0xc8],
      [0x8b, 0x00],
      [0xff, 0x24, 0x10],
      [0xa1], le(d + DATA_ORIGVT),
      [0xff, 0x24, 0x10]));
    for (let s = 0; s < SLOTS; s++) {
      const t = ctx.add(OFF_THUNKS + 10 * s);
      const rel = common.toUInt32() - (t.toUInt32() + 10);
      t.writeByteArray([].concat([0xba], le(4 * s), [0xe9], le(rel)));   // mov edx, 4*s; jmp common
      ctx.add(OFF_VT + 4 * s).writePointer(t);
    }
    ctx.writeByteArray(new Array(0x20).fill(0));
  }

  const lo = base, hi = base.add(POOL * CTX);
  function ctxOfVt(vt) {
    if (vt.compare(lo) < 0 || vt.compare(hi) >= 0) return null;
    const off = vt.sub(lo).toUInt32();
    return (off % CTX === OFF_VT) ? lo.add(off - OFF_VT) : null;
  }
  const origVt = {};               // fake address -> its own vtable (survives a table being reused for another fake)
  let next = 0;
  const status = { guards: 0, real: ptr(0) };
  function genuine(v) { return !v.isNull() && ctxOfVt(v.readPointer()) === null; }
  try { const v = DISPLAY.readPointer(); if (genuine(v)) status.real = v; } catch (e) { /* not mapped yet */ }

  function guard(fake, real) {
    fake = ptr(fake);
    if (fake.isNull()) return fake;
    if (real === undefined || real === null) {
      const v = DISPLAY.readPointer();
      if (!v.equals(fake) && genuine(v)) status.real = v;
      real = status.real;
    } else {
      real = ptr(real);
    }
    const key = fake.toString();
    const cur = fake.readPointer();
    let ctx = ctxOfVt(cur);
    if (ctx === null || !ctx.add(DATA_OWNER).readPointer().equals(fake)) {
      if (ctx === null) origVt[key] = cur;              // first guard of this fake, or its vtable was rewritten since
      ctx = base.add(next * CTX);
      next = (next + 1) % POOL;
      const prev = ctx.add(DATA_OWNER).readPointer();   // a stale fake on this table gets its own vtable back
      if (!prev.isNull() && !prev.equals(fake) && origVt[prev.toString()] !== undefined &&
          ctxOfVt(prev.readPointer()) !== null && ctxOfVt(prev.readPointer()).equals(ctx)) {
        prev.writePointer(origVt[prev.toString()]);
      }
    }
    ctx.add(DATA_ORIGVT).writePointer(origVt[key]);
    ctx.add(DATA_REAL).writePointer(real);
    ctx.add(DATA_FWD).writeU32(0);
    ctx.add(DATA_OWNER).writePointer(fake);
    ctx.add(DATA_TID).writeU32(Process.getCurrentThreadId());
    fake.writePointer(ctx.add(OFF_VT));
    status.guards++;
    return fake;
  }
  function forwarded(fake) {
    const ctx = ctxOfVt(ptr(fake).readPointer());
    return ctx === null ? 0 : ctx.add(DATA_FWD).readU32();
  }
  globalThis.__recDisplay = { guard: guard, forwarded: forwarded, status: status };
})();
