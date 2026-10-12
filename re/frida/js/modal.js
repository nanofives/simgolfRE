// Modal-loop driver for one A/B key (batch c3el, round 30, 2026-10-12). Shared helper like heapstandin.js /
// importstandin.js / freeze.js / uitree.js; diff_hook.py should load it after uitree.js (until it does, the batches that
// use it prepend it to their fixture fragment, and the guard below makes a second definition a no-op).
//
// Why: the game's dialogs and screens (runScrollText, textInputBox, showPopup, Window::runModal, every show* / select*
// screen) are loops that call the message / redraw pumps until the player presses a key or clicks. Run as they are on
// the A/B thread they never end: the pumps read the real clock (waitTicks / pumpAndRedraw loop until timeGetTime() says
// so), PeekMessageA on the A/B thread finds no message, and nothing ever writes the input globals the loops wait on.
// For the length of one key this helper replaces the pumps, ON THE A/B THREAD ONLY, by deterministic models that
//   - deliver a SCRIPT of input events at the message-pump visits (frame i of the script at visit i), each event through
//     the game's own handler for it: the slots +0x74..+0x9c of the root window's vtable 0x004ba2fc (handlers
//     0x00465580..0x00465840, the code Window::mouseDispatch* / Window::key 0x0047c678 reach), so the input globals are
//     written by original code exactly as a dispatched message writes them;
//   - reproduce each pump's effect on the input globals from its C3 reimplementation (addresses cited per model);
//   - keep time on a VIRTUAL CLOCK that advances a fixed step per frame, so every timed loop runs a fixed number of
//     iterations;
//   - record every pump call and every delivered event in one block ($modal, a state region): the call sequence, its
//     arguments and results, the script cursor and the clock are compared between the arms;
//   - BOUND the run: after opts.maxCalls pump calls (or opts.maxFrames frames inside one pump) the block's abort word is
//     set and the next pump call faults at a fixed address (a read of 0x00000b0d) before entering JavaScript. diff_hook's
//     NativeFunction converts the fault into a JavaScript error, so a loop that never sees its exit input ends the key as
//     ERROR (`access violation accessing 0xb0d`) within a known number of iterations instead of hanging the game.
//
//   const M = __modal({ script: [[], [{ k: 'key', a: 0x1b, code: 0x1b }]] });   // BEFORE the freeze: patches the pumps
//   __importStandIn({ module: 'golf_clean.exe', imports: Object.assign(M.imports(), mine) });   // or __uiTree({imports})
//   __freeze.thread();                                   // (or __uiTree, which freezes)
//   M.arm();                                             // AFTER the freeze: input globals saved and set neutral
//   return Object.assign(fx, M.fx());                    // fx.modal = the block
//
// Registry side: state = MODAL_STATE + your regions, where (same addresses as __modal.STATE below)
//   MODAL_STATE = [('$modal', 0, 0x1000), (0x00822b98, 0, 8), (0x00822d58, 0, 0x40), (0x004d3834, 0, 4),
//                  (0x0083ad4c, 0, 4), (0x0083afcc, 0, 4), (0x00838208, 0, 4), (0x0083821c, 0, 4), (0x0083ab9c, 0, 4)]
//
// THE CONTRACT (what a fixture can rely on; log/c3/c3el_notes.txt section 1 is the long form):
//  1. Stood-in pumps (default: all of PUMPS below; opts.stand = [names] narrows it, opts.keep = [names] leaves some
//     real). Calls from any other thread jump straight to the pump's existing code (its SG_HOOK detour or the original),
//     without entering JavaScript, eax and the stack untouched: the game's own threads are never affected. A function
//     under test must not be in the stood-in set (stand a pump only when testing its callers).
//  2. Message-pump VISITS: pumpMessages (also inside the waitTicks(_, pump != 0), pumpAndRedraw and pumpLight models) and
//     every pump named in opts.visits (default ['screenFlip']: Window::runModal's loop calls no other pump). Visit i
//     delivers script frame i (frames past the end deliver nothing, or the last frame again with opts.repeatLast) and
//     advances the clock by opts.frameMs (default 10). Events are delivered nowhere else. In the real game the messages
//     are dispatched inside the jgld display slots these pumps reach (+0x124 0x1006b080 calls slot +0x128 and
//     WaitMessage; +0x128 0x1006b0f0, +0x12c, +0x134, +0x13c, +0x150 call PeekMessageA / TranslateMessage /
//     DispatchMessageA; pumpMessages itself peeks only 0x200..0x200, 0x0045c03e..0x0045c04d), filtered by the phase word
//     0x0083ad4c the pumps set; delivering a frame at a visit is the model's choice, the same in both arms.
//  3. Events ({k: kind, ...}):
//        press1  (x, y[, ctrl, shift])  slot +0x74 0x00465580: [0x822d58]/[0x822d5c] = 2x / scaleX(2), 2y / scaleX(2),
//                                       [0x822d60]/[0x822d64] = x, y, modifiers from GetAsyncKeyState(0x11 / 0x10),
//                                       [0x822d68] = 1, [0x822d74] = 1, [0x822d78] = 0
//        press2  (x, y[, ctrl, shift])  slot +0x80 0x00465680: the same with [0x822d68] = 2
//        press1b (x, y)                 slot +0x98 0x004657e0: position, [0x822d68] = [0x822d78] = [0x822d74] = 1
//        press2b (x, y)                 slot +0x9c 0x00465840: position, [0x822d68] = 2, [0x822d78] = [0x822d74] = 1
//        point   (x, y)                 slot +0x78 0x00465610: [0x822d8c]/[0x822d90] = x, y (+ timeGetTime test)
//        unpoint                        slot +0x7c 0x00465650: [0x822d8c] = -1 (+ timeGetTime test)
//        point2  (x, y) / unpoint2      slots +0x84 0x00465720 / +0x88 0x00465740
//        key     (a, code)              slot +0x8c 0x00465750: [0x822b98] = code; [0x822b9c] = a unless code is
//                                       0x60..0x69 (then 0); [0x838208] = (code == 0x1000d)
//        char    (c)                    slot +0x90 0x004657b0: [0x822b98] = [0x83821c] = c
//        char0                          slot +0x94 0x004657d0: [0x83821c] = 0
//        slot    (slot, args[])         any other slot of the root window's vtable (thiscall, stack args)
//        call    (addr, args[], abi, self)  any function (e.g. Window::key 0x0047c5d0 on a modal window)
//        raw     (set: {addr: dword})   direct writes (inputs that no handler writes, e.g. the quit flag 0x0083afcc)
//        cursor  (x, y)                 the virtual cursor (block +0x60 / +0x64): what M.display's slot +0x2c returns
//                                       (jgld Display::getCursor 0x10066780: GetCursorPos + ScreenToClient)
//     M.display(inner) puts a CURSOR DISPLAY into [0x0083ad50] (after __uiTree saved it): slot +0x2c returns the virtual
//     cursor, slot +0x38 opts.origin (default 0, 0), each logged as a {0xD15C0000 | slot, x, y, 0} record WITHOUT the
//     pointer arguments (they point into the caller's frame, which differs between the arms; uitree.js's recorder logs
//     them, so a function that passes frame pointers to the display needs this); every other slot is forwarded to
//     `inner` on the A/B thread, and foreign threads reach the real display through recdisplay.js's guard.
//     Kinds name the handler slot by what it writes, not by a Windows message: which message reaches which slot goes
//     through jgld's window procedure and Window::mouseDispatch23c..258 and is not traced here. The root window is
//     0x00519a60 (opts.root); the handlers do not read ecx. GetAsyncKeyState and timeGetTime reach the import stand-in
//     through M.imports() (ctrl / shift from the event, the clock); a fixture must pass those imports to its ONE import
//     stand-in call (directly or through __uiTree's opts.imports), or the handlers read the real keyboard.
//  4. Models (each cites the C3 reimplementation it follows; `rec` marks a call the real body makes that the model
//     leaves out: the redraws, display slots and gfxFlush / movie tails; a key that needs them keeps the pump real):
//        pumpMessages 0x0045c030 (c3dw): visit; [0x822d74] == 0 -> [0x822d68] = 0; frameTick483cd0 rec;
//          [0x822d74] = 0; returns opts.ret.pumpMessages (frameTick's result; default 0)
//        flushRedraw 0x0045ae70 (c3dw): [0x4d3834] != 0 -> clear it, redrawAll rec, [0x822b9c] = [0x822b98] = 0;
//          else redrawAll rec; pumpInput483c30 rec; both key words 0 -> 0; else [0x4d3834] = 1, returns 1
//        waitTicks(ticks, pump) 0x0045bf80 (c3dy): ticks == 0 -> 0; limit = the 32-bit (ticks * 1000) / 100;
//          per iteration pumpInput483c30 rec, and with pump != 0 a visit and drainAndRedraw (0 when a key word is
//          set): a 0 or a non-zero [0x822d68] runs waitRedrawReady (clears the key words until drain says 1,
//          pumpInput483cf0 rec, [0x822d68] = 0) and returns 1; the clock advances (by the visit, or by frameMs
//          when pump == 0) until clock - start >= limit (unsigned), then 0
//        pumpAndRedraw(limit) 0x0045c0c0 (c3dy): [0x822d94] = [0x822d68] = 0; loop {visit; break on [0x822d68],
//          flushRedraw-model != 0, [0x83afcc]; limit == 0 repeats; (signed) clock - start < limit repeats};
//          [0x822d94] = [0x822b9c], [0x822d7c] = [0x822b98]; flushRedraw-model != 0 -> [0x822d94] / [0x822d7c] =
//          the key words read and cleared (setDirtyFlagB / A); enterModal483d30 rec; returns 1
//        pumpLight 0x0045c150 (c3dw): [0x822d68] = 0; loop {pumpInput483bd0 rec; pumpMessages-model; [0x822d68] != 0
//          repeats; flushRedraw-model == 0 ends}; returns 0
//        pumpInput483bd0 0x00483bd0 / pumpInput483c30 0x00483c30 / pumpInputDisplay483da0 0x00483da0 (c3dw):
//          [0x0083ad4c] = 0 (each body's last store); returns opts.ret.<name> (default 0)
//        screenFlip 0x00483ba0 (c3dl): visit; returns opts.ret.screenFlip (the display slot +0x124 result, default 0)
//     opts.ret.<name> may be a function(ctx) for any pump (then the model's own effects still run first).
//  5. The block (opts.size bytes, default 0x1000, every byte compared):
//        +0x00 pump calls (from the code under test)   +0x04 visits (the script cursor)   +0x08 events delivered
//        +0x0c virtual clock (ms, starts at opts.clock0, default 0x10000)   +0x10 abort word   +0x14 maxCalls
//        +0x18 FNV-1a hash over every record (so an overflowing log is still compared)   +0x1c log bytes used
//        +0x20 call counter per pump (PUMPS order, 16 words)   +0x60 / +0x64 virtual cursor x, y (opts.cursor)
//        +0x80 records of 16 bytes: pump call {id, arg0, arg1, result}; event {0xE0000000 | kind << 8 | visit,
//              arg0, arg1, handler result}; abort {0xAB000000 | reason, calls, visits, 0}
//  6. Bounds: opts.maxCalls (default 200) pump calls from the code under test; opts.maxFrames (default 2000) visits in
//     total. Reaching either sets the abort word (and logs an abort record); the pump that reached it returns normally,
//     the NEXT pump call faults before entering JavaScript. A key that is meant to exit must do so well inside the
//     bounds; an ERROR row with `0xb0d` is the bound, nothing else.
//  7. Lifetime: the patches revert from setTimeout(0) after the key's rpc call, with a per-install token (a later key's
//     install is never undone by an earlier key's timer). A new __modal call first reverts a leftover install.
//     M.arm() must run with the first thread frozen (__freeze.thread()): it saves every input global (restored before
//     the resume) and sets them neutral (all 0, [0x822d8c] = -1) unless opts.globals / M.arm(globals) says otherwise.
//  8. Per-vector scripts: M.rows(fx, [{script, globals: {addr: dword}, ret}, ...]) defines getters fx.r0..rN that
//     select that row's script, write its globals and return `ret` (the vector's argument): use ("$r0",) style vectors.
//     The getter runs before diff_hook's snapshot, so both arms start from the same script cursor and globals.
//     M.select(script[, cursor]) is the same reset for a fixture that writes its own getters.
(function () {
  if (globalThis.__modal) return;
  const keep = function (x) { (globalThis.__diffKeepAlive || []).push(x); return x; };
  const EXE = 'golf_clean.exe';

  // id, address, stack arguments (all cdecl: `ret` with no immediate)
  const PUMPS = [
    ['waitTicks', 0x0045bf80, 2],
    ['pumpMessages', 0x0045c030, 0],
    ['pumpAndRedraw', 0x0045c0c0, 1],
    ['flushRedraw', 0x0045ae70, 0],
    ['pumpLight', 0x0045c150, 0],
    ['pumpInput483bd0', 0x00483bd0, 0],
    ['pumpInput483c30', 0x00483c30, 0],
    ['pumpInputDisplay483da0', 0x00483da0, 0],
    ['screenFlip', 0x00483ba0, 0],
  ];
  const PUMP_ID = {};
  PUMPS.forEach(function (p, i) { PUMP_ID[p[0]] = i; });

  // input globals: [address, neutral value]
  const K98 = 0x00822b98, K9C = 0x00822b9c, D68 = 0x00822d68, D74 = 0x00822d74, D7C = 0x00822d7c, D94 = 0x00822d94;
  const L34 = 0x004d3834, PH = 0x0083ad4c, QUIT = 0x0083afcc;
  const GLOBALS = [
    [K98, 0], [K9C, 0],
    [0x00822d58, 0], [0x00822d5c, 0], [0x00822d60, 0], [0x00822d64, 0], [D68, 0], [0x00822d6c, 0], [0x00822d70, 0],
    [D74, 0], [0x00822d78, 0], [D7C, 0], [0x00822d80, 0], [0x00822d84, 0], [0x00822d88, 0], [0x00822d8c, 0xffffffff],
    [0x00822d90, 0], [D94, 0],
    [L34, 0], [PH, 0], [QUIT, 0], [0x00838208, 0], [0x0083821c, 0], [0x0083ab9c, 0],
  ];
  const STATE = [['$modal', 0, 0x1000], [K98, 0, 8], [0x00822d58, 0, 0x40], [L34, 0, 4], [PH, 0, 4], [QUIT, 0, 4],
                 [0x00838208, 0, 4], [0x0083821c, 0, 4], [0x0083ab9c, 0, 4]];

  // handler slots of the root window's vtable 0x004ba2fc (read from the image at load: .rdata, not written at run time)
  const VT = 0x004ba2fc;
  const KINDS = {
    press1: [0x74, 2], point: [0x78, 2], unpoint: [0x7c, 2], press2: [0x80, 2], point2: [0x84, 2], unpoint2: [0x88, 2],
    key: [0x8c, 2], char: [0x90, 1], char0: [0x94, 1], press1b: [0x98, 2], press2b: [0x9c, 2],
  };
  const KIND_CODE = { press1: 1, point: 2, unpoint: 3, press2: 4, point2: 5, unpoint2: 6, key: 7, char: 8, char0: 9,
                      press1b: 10, press2b: 11, slot: 12, call: 13, raw: 14, cursor: 15 };
  const slotFn = {};
  function handler(slot, nargs) {
    const k = slot + ':' + nargs;
    if (!slotFn[k]) {
      const at = ptr(VT).add(slot).readPointer();
      slotFn[k] = keep(new NativeFunction(at, 'uint32', ['pointer'].concat(Array(nargs).fill('uint32')), 'thiscall'));
    }
    return slotFn[k];
  }
  // build the handler NativeFunctions at load time (no allocation while a key has the first thread frozen)
  Object.keys(KINDS).forEach(function (n) { handler(KINDS[n][0], KINDS[n][1]); });
  const callFn = {};
  function anyFn(addr, nargs, abi) {
    const k = addr + ':' + nargs + ':' + abi;
    if (!callFn[k]) {
      callFn[k] = keep(new NativeFunction(ptr(addr), 'uint32',
        (abi === 'thiscall' ? ['pointer'] : []).concat(Array(nargs).fill('uint32')), abi));
    }
    return callFn[k];
  }

  // current key's state (null between keys: a stub reached by the A/B thread outside a key forwards nothing, see cb)
  const S = { blk: null, opts: null, script: null, ev: null };
  const u32 = function (a) { return ptr(a).readU32(); };
  const w32 = function (a, v) { ptr(a).writeU32(v >>> 0); };

  function fnv(h, w) {
    for (let k = 0; k < 4; k++) { h ^= (w >>> (8 * k)) & 0xff; h = Math.imul(h, 0x01000193) >>> 0; }
    return h >>> 0;
  }
  function record(a, b, c, d) {
    const blk = S.blk;
    let h = blk.add(0x18).readU32();
    [a, b, c, d].forEach(function (w) { h = fnv(h, w >>> 0); });
    blk.add(0x18).writeU32(h);
    const used = blk.add(0x1c).readU32();
    if (0x80 + used + 16 <= S.size) {
      const e = blk.add(0x80 + used);
      e.writeU32(a >>> 0); e.add(4).writeU32(b >>> 0); e.add(8).writeU32(c >>> 0); e.add(12).writeU32(d >>> 0);
      blk.add(0x1c).writeU32(used + 16);
    }
  }
  function abort(reason) {
    if (S.blk.add(0x10).readU32()) return;
    S.blk.add(0x10).writeU32(reason);
    record(0xAB000000 | reason, S.blk.readU32(), S.blk.add(4).readU32(), 0);
  }

  // ---- event delivery
  function deliver(ev, visit) {
    const k = ev.k;
    const code = KIND_CODE[k];
    if (code === undefined) throw new Error('modal: unknown event kind ' + k);
    let a0 = 0, a1 = 0, r = 0;
    S.ev = ev;                                     // GetAsyncKeyState reads ctrl / shift from it
    try {
      if (KINDS[k]) {
        const sl = KINDS[k];
        const args = k === 'key' ? [ev.a | 0, ev.code | 0] : k === 'char' || k === 'char0' ? [ev.c | 0]
          : [ev.x | 0, ev.y | 0];
        a0 = args[0]; a1 = args.length > 1 ? args[1] : 0;
        r = handler(sl[0], sl[1]).apply(null, [ptr(S.opts.root)].concat(args));
      } else if (k === 'slot') {
        const args = ev.args || [];
        a0 = ev.slot; a1 = args.length ? args[0] : 0;
        r = handler(ev.slot, args.length).apply(null, [ptr(S.opts.root)].concat(args));
      } else if (k === 'call') {
        const abi = ev.abi || 'mscdecl', args = ev.args || [];
        a0 = ev.addr; a1 = args.length ? args[0] : 0;
        const f = anyFn(ev.addr, args.length, abi);
        r = abi === 'thiscall' ? f.apply(null, [ptr(ev.self)].concat(args)) : f.apply(null, args);
      } else if (k === 'cursor') {
        a0 = ev.x | 0; a1 = ev.y | 0;
        S.blk.add(0x60).writeS32(a0); S.blk.add(0x64).writeS32(a1);
      } else {                                     // raw
        Object.keys(ev.set).forEach(function (a) { w32(+a, ev.set[a]); a0 = +a; a1 = ev.set[a]; });
      }
    } finally {
      S.ev = null;
    }
    S.blk.add(8).writeU32(S.blk.add(8).readU32() + 1);
    record(0xE0000000 | (code << 8) | (visit & 0xff), a0, a1, r);
  }

  // one message-pump visit: deliver frame `visit` of the script, advance the clock
  function visit() {
    const blk = S.blk;
    const v = blk.add(4).readU32();
    blk.add(4).writeU32(v + 1);
    if (v + 1 >= S.opts.maxFrames) abort(2);
    let frame = v < S.script.length ? S.script[v] : (S.opts.repeatLast && S.script.length ? S.script[S.script.length - 1] : null);
    if (frame && !Array.isArray(frame)) frame = [frame];
    (frame || []).forEach(function (ev) { deliver(ev, v); });
    blk.add(0xc).writeU32(blk.add(0xc).readU32() + S.opts.frameMs);
  }
  const clock = function () { return S.blk.add(0xc).readU32(); };
  const aborted = function () { return S.blk.add(0x10).readU32() !== 0; };

  // ---- models (section 4 of the header)
  function retOf(name, dflt, ctx) {
    const r = S.opts.ret[name];
    if (r === undefined) return dflt;
    return typeof r === 'function' ? r(ctx) : r;
  }
  function mPumpMessages() {
    visit();
    if (u32(D74) === 0) w32(D68, 0);                                      // 0x0045c08a..0x0045c094
    const r = retOf('pumpMessages', 0, {});                               // frameTick483cd0 0x0045c09e
    w32(D74, 0);                                                          // 0x0045c0a3
    return r;
  }
  function mFlushRedraw() {
    if (u32(L34) !== 0) {                                                 // 0x0045ae75
      w32(L34, 0); w32(K9C, 0); w32(K98, 0);                              // 0x0045ae79, 0x0045ae88, 0x0045ae92
    }
    if (u32(K98) === 0 && u32(K9C) === 0) return 0;                       // 0x0045aeaf / 0x0045aeb6
    w32(L34, 1);                                                          // 0x0045aec2
    return 1;
  }
  function mDrain() { return (u32(K9C) !== 0 || u32(K98) !== 0) ? 0 : 1; }   // drainAndRedraw 0x0045af0f / 0x0045af18
  function mWaitRedrawReady() {
    let r = mDrain();                                                     // 0x0045aed1
    while (r === 0) { w32(K9C, 0); w32(K98, 0); r = mDrain(); }           // 0x0045aedc..0x0045aeef
    w32(D68, 0);                                                          // 0x0045aef6
    return 0;
  }
  function mWaitTicks(ticks, pump) {
    if ((ticks | 0) === 0) return 0;                                      // 0x0045bf87
    const prod = Math.imul(ticks | 0, 1000);                              // 0x0045bf8c..0x0045bfa0
    const limit = (prod / 100 | 0) >>> 0;                                 // signed /100, truncation (0x0045bfa3..0x0045bfad)
    const start = clock();
    if (((clock() - start) >>> 0) >= limit) return 0;                     // 0x0045bfbb
    do {
      if (pump !== 0) {                                                   // 0x0045bfc8
        mPumpMessages();                                                  // 0x0045bfca
        if (mDrain() === 0 || u32(D68) !== 0) { mWaitRedrawReady(); return 1; }   // 0x0045bfcf..0x0045bff8
      } else {
        S.blk.add(0xc).writeU32(clock() + S.opts.frameMs);
      }
      if (aborted()) return 0;
    } while (((clock() - start) >>> 0) < limit);                          // 0x0045bfe7
    return 0;
  }
  function mPumpAndRedraw(limit) {
    const start = clock();
    w32(D94, 0); w32(D68, 0);                                             // 0x0045c0d4 / 0x0045c0d9
    for (;;) {
      mPumpMessages();                                                    // 0x0045c0de
      const now = clock();
      if (u32(D68) !== 0) break;                                          // 0x0045c0ee
      if (mFlushRedraw() !== 0) break;                                    // 0x0045c0f7
      if (u32(QUIT) !== 0) break;                                         // 0x0045c100
      if (aborted()) break;
      if ((limit | 0) === 0) continue;                                    // 0x0045c104
      if (((now - start) | 0) < (limit | 0)) continue;                    // 0x0045c10a (signed)
      break;
    }
    w32(D94, u32(K9C)); w32(D7C, u32(K98));                               // 0x0045c10c..0x0045c11c
    if (mFlushRedraw() !== 0) {                                           // 0x0045c122
      const b = u32(K9C); w32(K9C, 0); w32(D94, b);                       // setDirtyFlagB 0x0045ae50
      const a = u32(K98); w32(K98, 0); w32(D7C, a);                       // setDirtyFlagA 0x0045ae30
    }
    return 1;                                                             // 0x0045c148
  }
  function mPumpLight() {
    w32(D68, 0);                                                          // 0x0045c150
    for (;;) {
      mPumpMessages();                                                    // 0x0045c15f
      if (aborted()) break;
      if (u32(D68) !== 0) continue;                                       // 0x0045c16b
      if (mFlushRedraw() === 0) break;                                    // 0x0045c174
    }
    return 0;
  }
  const MODELS = {
    waitTicks: function (a) { return mWaitTicks(a[0] | 0, a[1] | 0); },
    pumpMessages: function () { return mPumpMessages(); },
    pumpAndRedraw: function (a) { return mPumpAndRedraw(a[0] | 0); },
    flushRedraw: function () { return mFlushRedraw(); },
    pumpLight: function () { return mPumpLight(); },
    pumpInput483bd0: function () { w32(PH, 0); return retOf('pumpInput483bd0', 0, {}); },
    pumpInput483c30: function () { w32(PH, 0); return retOf('pumpInput483c30', 0, {}); },
    pumpInputDisplay483da0: function () { w32(PH, 0); return retOf('pumpInputDisplay483da0', 0, {}); },
    screenFlip: function () { return retOf('screenFlip', 0, {}); },
  };

  // ---- the stubs (hand-encoded, as importstandin.js's), one per pump, built at load time
  //   +0x00 50                    push eax              eax is preserved on the forward path (pumpInputDisplay483da0
  //   +0x01 64 a1 24 00 00 00     mov eax, fs:[0x24]     passes its entry eax through when there is no display)
  //   +0x07 3b 05 <TID>           cmp eax, [TID]        TID = 0 between keys: every thread forwards
  //   +0x0d 58                    pop eax
  //   +0x0e 0f 85 <rel FWD>       jne FWD               other threads: the existing code (detour or original)
  //   +0x14 a1 <BLKCELL>          mov eax, [BLKCELL]    the current key's block
  //   +0x19 83 78 10 00           cmp dword [eax + 0x10], 0
  //   +0x1d 0f 85 <rel FAULT>     jne FAULT             the bound was reached: end the key here
  //   +0x23 e9 <rel cb>           jmp cb                the model (JavaScript)
  //   +0x28 ff 25 <ORIG>          FWD: jmp [ORIG]       filled with replaceFast's result
  //   +0x2e a1 0d 0b 00 00        FAULT: mov eax, [0x00000b0d]
  //   +0x33 cc                    int3
  // Data per stub: +0x40 TID, +0x44 ORIG. BLKCELL (one for all stubs) holds the block address, so the abort word is
  // the block's +0x10: part of the compared state, restored by diff_hook between the arms like the rest of the block.
  const STUB_SZ = 0x60;
  const stubs = keep(Memory.alloc(Process.pageSize));
  Memory.protect(stubs, Process.pageSize, 'rwx');
  const BLKCELL = stubs.add(Process.pageSize - 4);
  const DUMMY = keep(Memory.alloc(0x40));                     // a block-shaped dummy with abort 0 (between keys)
  DUMMY.writeByteArray(new Array(0x40).fill(0));
  BLKCELL.writePointer(DUMMY);
  const le = function (v) { v = v >>> 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; };
  const CB = [];
  PUMPS.forEach(function (p, i) {
    const name = p[0], nargs = p[2];
    const st = stubs.add(i * STUB_SZ);
    const TID = st.add(0x40), ORIG = st.add(0x44);
    const types = Array(nargs).fill('uint32');
    let origFn = null, origAt = null;
    const cb = keep(new NativeCallback(function () {
      const a = Array.prototype.slice.call(arguments);
      if (S.blk === null) {                                   // no key active (should not happen: TID is 0 then)
        const o = ORIG.readPointer();
        if (origFn === null || !o.equals(origAt)) { origAt = o; origFn = new NativeFunction(o, 'uint32', types, 'mscdecl'); }
        return origFn.apply(null, a);
      }
      const blk = S.blk;
      const n = blk.readU32() + 1;
      blk.writeU32(n);
      blk.add(0x20 + 4 * i).writeU32(blk.add(0x20 + 4 * i).readU32() + 1);
      if (S.visits[name] && name !== 'pumpMessages') visit();
      let r = MODELS[name](a);
      if (OVERRIDE_AFTER[name] && S.opts.ret[name] !== undefined) r = retOf(name, r, { args: a, model: r });
      r = r >>> 0;
      record(i, a.length > 0 ? a[0] : 0, a.length > 1 ? a[1] : 0, r);
      if (n >= S.opts.maxCalls) abort(1);
      return r;
    }, 'uint32', types, 'mscdecl'));
    CB.push(cb);
    const rel = function (target, next) { return target.sub(st.add(next)).toUInt32(); };
    const bytes = [0x50, 0x64, 0xa1, 0x24, 0x00, 0x00, 0x00]
      .concat([0x3b, 0x05], le(TID.toUInt32()))
      .concat([0x58])
      .concat([0x0f, 0x85], le(rel(st.add(0x28), 0x14)))
      .concat([0xa1], le(BLKCELL.toUInt32()))
      .concat([0x83, 0x78, 0x10, 0x00])
      .concat([0x0f, 0x85], le(rel(st.add(0x2e), 0x23)))
      .concat([0xe9], le(rel(cb, 0x28)))
      .concat([0xff, 0x25], le(ORIG.toUInt32()))
      .concat([0xa1, 0x0d, 0x0b, 0x00, 0x00, 0xcc]);
    if (bytes.length !== 0x34) throw new Error('modal: stub length ' + bytes.length);
    st.writeByteArray(bytes);
    const back = new Uint8Array(st.readByteArray(bytes.length));
    if (back.some(function (b, k) { return b !== bytes[k]; })) throw new Error('modal: stub write failed');
    TID.writeU32(0); ORIG.writeU32(0);
  });
  // models whose result opts.ret.<name> replaces after the model ran (the others read opts.ret themselves)
  const OVERRIDE_AFTER = { waitTicks: 1, pumpAndRedraw: 1, flushRedraw: 1, pumpLight: 1 };

  // ---- the cursor display (M.display): a display object whose slots +0x2c (jgld Display::getCursor 0x10066780: GetCursorPos
  // 0x100667a3 + ScreenToClient 0x100667c0 into *x, *y, `ret 8`) and +0x38 (two dwords into *pair, `ret 4`) are answered from
  // the block (the virtual cursor; opts.origin), logged WITHOUT their pointer arguments (they point into the caller's frame,
  // which differs between the arms), and whose other slots are forwarded, on the A/B thread, to an inner display (uitree.js's
  // recording display). Calls from other threads go to the real display through recdisplay.js's guard (no JavaScript).
  // Stack-argument counts per slot: jgld's Display vtable 0x1011d640 (the table uitree.js uses, log/c3/alt/c3ef_dispslots.py).
  const DISP_NARGS = [
    1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 4, 2, 1, 2, 1, 0, 1, 1, 0, 0, 3, 1, 3, 5, 4, 1, 0, 2, 0, 1, 1, 2, 2, 1, 1, 1,
    1, 1, 1, 3, 3, 1, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 1, 1, 1, 3, 1, 2, 1, 1, 1, 1, 6, 4, 2, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1];
  const D = { inner: ptr(0), fwd: {} };
  const CURSOR_VT = keep(Memory.alloc(0x200));
  for (let s = 0; s < 0x200; s += 4) {
    const n = s / 4 < DISP_NARGS.length ? DISP_NARGS[s / 4] : 0;
    const types = ['pointer'].concat(Array(n).fill('uint32'));
    const slot = s;
    CURSOR_VT.add(s).writePointer(keep(new NativeCallback(function (self) {
      const a = Array.prototype.slice.call(arguments, 1);
      if (slot === 0x2c && S.blk) {
        const x = S.blk.add(0x60).readS32(), y = S.blk.add(0x64).readS32();
        ptr(a[0]).writeS32(x); ptr(a[1]).writeS32(y);
        record(0xD15C0000 | slot, x, y, 0);
        return 0;
      }
      if (slot === 0x38 && S.blk) {
        const o = S.opts.origin || [0, 0];
        ptr(a[0]).writeS32(o[0] | 0); ptr(a[0]).add(4).writeS32(o[1] | 0);
        record(0xD15C0000 | slot, o[0] | 0, o[1] | 0, 0);
        return 0;
      }
      const inner = D.inner;
      if (inner.isNull()) return 0;
      const target = inner.readPointer().add(slot).readPointer();
      const k = target.toString();
      if (!D.fwd[k]) D.fwd[k] = new NativeFunction(target, 'uint32', types, 'thiscall');
      return D.fwd[k].apply(null, [inner].concat(a));
    }, 'uint32', types, 'thiscall')));
  }

  const live = {};                // address string -> token of the install that patched it
  let nextToken = 1;
  function unpatch(i, token) {
    const at = ptr(PUMPS[i][1]), k = at.toString();
    if (live[k] === undefined || (token !== undefined && live[k] !== token)) return;
    delete live[k];
    stubs.add(i * STUB_SZ).add(0x40).writeU32(0);             // TID 0: every thread forwards, even mid-revert
    Interceptor.revert(at);
  }
  // the key is over: every thread forwards (the patches themselves revert from the install's timer)
  function quiesce(blk) {
    if (S.blk !== blk) return;
    PUMPS.forEach(function (p, i) { stubs.add(i * STUB_SZ).add(0x40).writeU32(0); });
    BLKCELL.writePointer(DUMMY);
    S.blk = null;
  }

  globalThis.__modal = function (opts) {
    opts = opts || {};
    // a leftover install of a previous key (its timer has not fired yet)
    PUMPS.forEach(function (p, i) { unpatch(i); });
    Interceptor.flush();
    const o = {
      root: opts.root || 0x00519a60, frameMs: opts.frameMs || 10, maxCalls: opts.maxCalls || 200,
      maxFrames: opts.maxFrames || 2000, repeatLast: !!opts.repeatLast, ret: opts.ret || {},
      clock0: opts.clock0 === undefined ? 0x10000 : opts.clock0, globals: opts.globals || {},
      cursor: opts.cursor || [0, 0],
    };
    S.visits = {};
    (opts.visits || ['screenFlip']).forEach(function (n) { S.visits[n] = 1; });
    const size = opts.size || 0x1000;
    const blk = keep(Memory.alloc(size));
    const reset = function (script) {
      blk.writeByteArray(new Array(size).fill(0));
      blk.add(0xc).writeU32(o.clock0);
      blk.add(0x14).writeU32(o.maxCalls);
      blk.add(0x18).writeU32(0x811c9dc5);
      blk.add(0x60).writeS32(o.cursor[0] | 0); blk.add(0x64).writeS32(o.cursor[1] | 0);
      if (script !== undefined) S.script = script;
    };
    S.blk = blk; S.opts = o; S.size = size; S.script = opts.script || [];
    reset();
    BLKCELL.writePointer(blk);
    const token = nextToken++;
    const tid = Process.getCurrentThreadId();
    let names = opts.stand || PUMPS.map(function (p) { return p[0]; });
    if (opts.keep) names = names.filter(function (n) { return opts.keep.indexOf(n) < 0; });
    names.forEach(function (n) {
      const i = PUMP_ID[n];
      if (i === undefined) throw new Error('modal: unknown pump ' + n);
      const at = ptr(PUMPS[i][1]);
      const st = stubs.add(i * STUB_SZ);
      st.add(0x40).writeU32(0);
      let orig;
      try {
        orig = Interceptor.replaceFast(at, st);
      } catch (e) {                    // another helper's stand-in (uitree / calleeStand) still sits there: take it off
        Interceptor.revert(at);
        Interceptor.flush();
        orig = Interceptor.replaceFast(at, st);
      }
      st.add(0x44).writePointer(orig);
      st.add(0x40).writeU32(tid);
      live[at.toString()] = token;
    });
    Interceptor.flush();
    setTimeout(function () {
      quiesce(blk);
      PUMPS.forEach(function (p, i) { unpatch(i, token); });
      Interceptor.flush();
    }, 0);

    const M = {
      block: blk, PUMPS: PUMPS.map(function (p) { return p[0]; }),
      // imports the event handlers reach, for the fixture's ONE import stand-in call
      imports: function () {
        return {
          'USER32.dll!GetAsyncKeyState': {
            impl: function (a) {
              const vk = a[0].toUInt32(), ev = S.ev || {};
              return (vk === 0x11 && ev.ctrl) || (vk === 0x10 && ev.shift) ? 0x8000 : 0;
            },
          },
          'WINMM.dll!timeGetTime': { impl: function () { return S.blk ? clock() : 0; } },
        };
      },
      // with the first thread frozen: save every input global and set it neutral (or to g[addr])
      arm: function (g) {
        if (!globalThis.__freeze) throw new Error('modal: freeze.js not loaded');
        const over = Object.assign({}, o.globals, g || {});
        GLOBALS.forEach(function (e) { __freeze.save(ptr(e[0]), 4); w32(e[0], e[1]); });
        Object.keys(over).forEach(function (a) { __freeze.save(ptr(+a), 4); w32(+a, over[a]); });
        __freeze.atFlush(function () { quiesce(blk); });
        return M;
      },
      // per-vector rows: getters r0..rN (header item 8)
      rows: function (fx, rows) {
        rows.forEach(function (row, i) {
          Object.defineProperty(fx, 'r' + i, {
            enumerable: true, configurable: true,
            get: function () {
              reset(row.script || []);
              Object.keys(row.globals || {}).forEach(function (a) { __freeze.save(ptr(+a), 4); w32(+a, row.globals[a]); });
              const r = row.ret === undefined ? 0 : row.ret;
              return typeof r === 'function' ? r(fx) : r;
            },
          });
        });
        return fx;
      },
      fx: function () { return { modal: blk }; },
      // the cursor display over an inner display (uitree.js's T.disp): built once per key, put into [0x0083ad50] (the caller
      // saved it: __uiTree does), guarded for foreign threads (they reach `real`, by default the display 0x0083ad50 held at
      // modal.js load or the last genuine one recdisplay.js saw). Returns the object.
      display: function (inner, real) {
        const d = keep(Memory.alloc(0x40));
        for (let k = 4; k < 0x40; k += 4) d.add(k).writeU32(0xd15c0000 + k);
        d.writePointer(CURSOR_VT);
        D.inner = ptr(inner);
        if (globalThis.__recDisplay) __recDisplay.guard(d, real);
        ptr(0x0083ad50).writePointer(d);
        return d;
      },
      // a display slot +0x2c body for a recording display that logs no pointer argument itself: *x, *y = the virtual cursor
      getCursor: function (self, args) {
        ptr(args[0]).writeS32(blk.add(0x60).readS32());
        ptr(args[1]).writeS32(blk.add(0x64).readS32());
        return 0;
      },
      script: function (s) { S.script = s; },
      // reset the block (counters, clock, cursor, log) and select a script: what a row getter of a fixture's own does
      // before diff_hook's snapshot (M.rows is the ready-made form)
      select: function (script, cursor) {
        reset(script || []);
        if (cursor) { blk.add(0x60).writeS32(cursor[0] | 0); blk.add(0x64).writeS32(cursor[1] | 0); }
      },
    };
    return M;
  };
  globalThis.__modal.STATE = STATE;
  globalThis.__modal.GLOBALS = GLOBALS.map(function (g) { return g[0]; });
  globalThis.__modal.PUMPS = PUMPS;
})();
