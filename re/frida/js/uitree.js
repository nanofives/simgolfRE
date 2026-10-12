// UI object-tree builder for one A/B key (batch c3ef, round 29, 2026-10-12). Shared helper like heapstandin.js /
// importstandin.js / freeze.js; diff_hook.py should load it after freeze.js (until it does, the batches that use it
// prepend it to their fixture fragment, and the guard below makes a second definition a no-op).
//
// Why: the golf_clean.exe widget classes (View / Window, Button, EditBox, ListBox, ListModel, the TextView composite of
// ListBox + three field views + a panel over one shared Window and one shared ListModel virtual base) could only be A/B'd
// on synthetic objects (c3di wrote every field by hand). Their constructors, resets and destructors need REAL objects:
// vbtable offsets, sub-object vtables, embedded Timers, HotLists and node lists exactly as the game lays them out. This
// helper builds them with the game's own constructors inside a heap stand-in arena, so:
//   - every object, every node a list allocates and every block a function under test allocates lives in ONE arena
//     ($heap, a state region): both arms see the same tree at the same addresses, and every write either arm makes to
//     it is compared;
//   - frees are only recorded ($freelog, compared): nothing the tree owns is released for real;
//   - the window-tree globals are pointed at the tree (or zeroed) while the game's first thread is frozen, and written
//     back before the resume: nothing is linked into the live z-list, focus / hover / capture / tooltip globals, and the
//     live display is replaced by a RECORDING display whose slots pop exactly what jgld's Display slots pop;
//   - the Win32 imports these chains reach (ReleaseCapture, timer kill / set) go through the import stand-in ($implog,
//     compared).
//
//   const T = __uiTree({ arenaSize: 0x10000 });          // stand-ins + recording display + freeze + neutral globals
//   const tv = T.make('TextView');                       // operator new (arena) + TextView::ctor 0x0048ce00
//   T.append(tv.add(0x1460 + 0x774), T.str('row'), 0);   // ListModel::append 0x00489890 on its shared ListModel
//   T.globals({ hover: [tv, 0], capture: tv });          // window globals that point into the tree
//   return T.fx({ tv: tv });                             // fx for diff_hook: heap, freelog, implog, rec, disp + yours
//
// Registry side (re/frida/registry.d/<ID>.py): state = UI_STATE(arena) + your regions, where UI_STATE is
//   [('$heap', 0, 0x10 + arena), ('$freelog', 0, 0x404), ('$implog', 0, 0x400), ('$rec', 0, REC_SIZE)]
//   + [(g, 0, 4) for g in GLOBALS] + [(0x0083a2d8, 0, 0x200)]           (GLOBALS = T.GLOBALS below, same order)
//
// The constructors are called at their original addresses: a constructor that some batch hooks runs through that
// hook (a C3 reimplementation). For a key whose input must be built by original code only, run it with those hooks off
// (SIMGOLF_HOOKS_OFF), as for any hooked callee.
//
// Order inside __uiTree (each step's reason):
//   1. heap stand-in (malloc 0x004a504f, operator new 0x004a57f1, free 0x004a5007, operator delete 0x004a4ffc, realloc
//      0x004a6980) and import stand-in, BEFORE the freeze: both patch code / IAT slots the first thread may be running.
//   2. recording display, recorder, NativeFunctions: allocated before the freeze (no allocation while it is down).
//   3. __freeze.thread(); every global in GLOBALS saved and set to a neutral value, the z-list emptied.
//   4. the caller builds (constructors write only object fields and the list cookie 0x00839650, which is in GLOBALS).
// The stand-ins and the freeze revert from setTimeout(0), after the key's rpc call, newest first.
(function () {
  if (globalThis.__uiTree) return;
  const EXE = 'golf_clean.exe';
  const keep = function (x) { globalThis.__diffKeepAlive.push(x); return x; };

  // Stack-argument count of every slot of jgld.dll's Display vtable 0x1011d640 (86 slots), from each slot body's
  // `ret N` (log/c3/alt/c3ef_dispslots.py). The recording display pops the same, so a call through any slot keeps the
  // caller's stack balanced (thiscall: the callee pops).
  const DISP_NARGS = [
    1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 4, 2, 1, 2, 1, 0, 1, 1, 0, 0, 3, 1, 3, 5, 4, 1, 0, 2, 0, 1, 1, 2, 2, 1, 1, 1,
    1, 1, 1, 3, 3, 1, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 1, 1, 1, 3, 1, 2, 1, 1, 1, 1, 6, 4, 2, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1];

  // Window-tree globals (addresses from the disassembly; c3di's map in log/c3/c3di_notes.txt, extended by c3ef):
  // saved at the freeze, set to the neutral value, restored before the resume. The registry's UI_STATE lists the same
  // addresses as 4-byte regions, plus the z-list array.
  const GLOBALS = [
    [0x0083ab94, 0],           // z-list count (listRemove 0x0047e539; layoutViewTree compares it with 0x80, 0x0047a74c)
    [0x0083ab90, 0],           // draw-list count (rebuildDrawList 0x0047e45d; 0x0047f8ef returns at once when 0)
    [0x0083aae0, 0],           // rebuildDrawList 0x0047e465
    [0x0083aaec, 0],           // rebuildDrawList 0x0047e47f
    [0x0083aaa0, 0],           // tooltip image (drawTooltip 0x0047cf6d)
    [0x0083aabc, 0],           // tooltip callback (`call eax` 0x0047cdb9 when non-zero)
    [0x0083aac0, 0],           // tooltip owner (layoutNode 0x00479f9b, Window::detach 0x0047b83b)
    [0x0083aac4, 0],           // tooltip text (Tooltip::hide 0x00480360..0x00480377)
    [0x0083aa98, 0],           // capture window (0x00479b20 -> ReleaseCapture)
    [0x0083aaf0, 0],           // layoutNode 0x00479fa8
    [0x0083ab30, 0],           // drag capture (layoutNode 0x00479f78)
    [0x0083ab34, 0],           // second drag capture (layoutNode 0x00479f8d)
    [0x0083ab40, 0],           // hover pair (layoutNode 0x00479f41..0x00479f72)
    [0x0083ab44, 0],
    [0x0083ab54, 0x0bad0054],  // modal top: no window (Window::detach 0x0047b8b4 then never reaches the modal pop)
    [0x0083ac18, 0x0ac18000],  // copied into +0x56c by View4804a0::ctor 0x0048055f and Window::ctorInit 0x0048069f
    [0x00839650, 0],           // list cookie: saved and cleared by ListModel::ctor 0x0048917c, restored by sub-dtors
    [0x0083ab2c, 0],           // written by layoutViewTree 0x0047aab1
    [0x0083aa9c, 0],           // default parent window layoutViewTree reads when it has none (0x0047a6f9)
    [0x0083a2c8, 0], [0x0083a2cc, 0], [0x0083a2d0, 0], [0x0083a2d4, 0],   // drawTooltip 0x0047cdb0 writes them
    [0x0083ad50, null],        // display: the recording display (or 0 with opts.display === false)
    [0x0083ad4c, undefined],   // input phase written by pumpInputDisplay483da0 0x00483da0: saved, value kept
  ];
  const ZLIST = 0x0083a2d8, ZLIST_BYTES = 0x200;     // 0x80 window pointers (layoutViewTree's limit, 0x0047a74c)
  const REC_SIZE = 0x1000;                           // recorder: +0 count, 16-byte entries from +0x10 (255 entries)

  // Constructors: [allocation size, constructor, extra stack arguments]. Sizes are the game's own operator-new sizes
  // where it allocates the class (log/c3/alt/c3ef_allocsizes.py: Image 0x2b8 0x0048077b, EditBox 0x5dc 0x004a2660,
  // TextView 0x2114 0x0049132b, Scrollbar 0x13b4 0x0047a978), otherwise the end of the last field the constructor and
  // its parts write (the parts with build = 1: the class stands alone with its own Window and ListModel bases).
  const KINDS = {
    View:        [0x580, 0x004804a0, []],     // View4804a0::ctor (last field +0x570, 0x004804e7)
    Image:       [0x2b8, 0x00474ae0, []],
    EditBox:     [0x5dc, 0x00486070, []],
    Button:      [0x630, 0x00488500, []],     // last field +0x62c (0x004885b0)
    ButtonObj:   [0x6d0, 0x004a13f0, []],     // ctorObj4a13f0: the 0x6d0 stride of its two copies in buildLabeledControl
    Scrollbar:   [0x13b4, 0x00496030, []],    // buildLabeledControl
    TextView:    [0x2114, 0x0048ce00, []],
    ListModel:   [0xf8, 0x00489150, []],      // last field +0xf4 (0x00489211)
    ListBox:     [0x6cc, 0x00489cb0, [1]],    // ListModel base at +0x5d4
    Field4a2250: [0x6fc, 0x004a2250, [1]],    // ListModel base at +0x604
    Field49d5a0: [0x694, 0x0049d5a0, [1]],    // ListModel base at +0x59c
    Field49ebb0: [0x68c, 0x0049ebb0, [1]],    // ListModel base at +0x594
    PanelSub:    [0x750, 0x004a3110, [1]],    // ListModel base at +0x658
    Composite:   [0x86c, 0x004a0740, [1]],    // viewCtor4a0740: Window base +0x1fc, ListModel base +0x774
  };

  // original (trampoline) address of an exe function that has an SG_HOOK, else the address itself
  const origCache = {};
  const FH_BUF = keep(Memory.alloc(0x10));
  let findHook = null;
  function origAddr(addr) {
    if (origCache[addr]) return origCache[addr];
    if (findHook === null) {
      findHook = keep(new NativeFunction(Process.getModuleByName('winmm.dll').getExportByName('SimGolfShim_FindHook'),
                                         'int', ['uint32', 'pointer', 'pointer', 'pointer'], 'stdcall'));
    }
    const pd = FH_BUF, po = FH_BUF.add(4), pi = FH_BUF.add(8);
    const found = findHook(addr, pd, po, pi);
    const o = (found && pi.readS32() === 1) ? po.readPointer() : ptr(addr);
    origCache[addr] = o;
    return o;
  }
  const nf = {};
  function fn(addr, ret, args, abi) {
    const k = addr + ':' + ret + ':' + args.join(',') + ':' + abi;
    if (!nf[k]) nf[k] = keep(new NativeFunction(ptr(addr), ret, args, abi));
    return nf[k];
  }

  // Recording callbacks write to the CURRENT key's recorder only (the recorder of a finished key is never written).
  const S = { rec: null };
  function log(tag, self, a0, a1) {
    const rec = S.rec;
    if (rec === null) return;
    const n = rec.readU32();
    rec.writeU32(n + 1);
    if (n >= (REC_SIZE - 0x10) / 16) return;
    const e = rec.add(0x10 + n * 16);
    e.writeU32(tag >>> 0);
    e.add(4).writeU32(ptr(self).toUInt32());
    e.add(8).writeU32(a0 >>> 0);
    e.add(12).writeU32(a1 >>> 0);
  }

  // The recording display: one vtable shared by every key (0x200 bytes: the 86 jgld slots, then spares that take no
  // argument). Each slot records {0xD15P0000 | slot, this, arg0, arg1 ^ (arg2 << 16)} and returns the key's value for
  // that slot (S.dispRet[slot]: a number or function(self, args)), default 0, except slot +0x20 (getHwnd) 0xbeef.
  S.dispRet = {};
  // Calls on any thread other than the key's A/B thread are forwarded to the REAL display (the value 0x0083ad50 held at
  // the freeze, S.realDisp) and not recorded: the game's multimedia-timer callback 0x00486f90 runs on the winmm thread and
  // calls the display's slot +0x20 (0x00486fca) through 0x0083ad50, which would otherwise write into the compared
  // recorder mid-arm and hand that game timer the fake HWND (found 2026-10-12: two intermittent REDs whose recorded state
  // differed between the arms of one vector).
  S.abTid = 0;
  S.realDisp = ptr(0);
  const realSlot = {};
  function forwardReal(slot, types, args) {
    const d = S.realDisp;
    if (d.isNull()) return 0;
    const target = d.readPointer().add(slot).readPointer();
    const k = target.toString() + ':' + types.length;
    if (!realSlot[k]) realSlot[k] = new NativeFunction(target, 'uint32', types, 'thiscall');
    return realSlot[k].apply(null, [d].concat(args));
  }
  const DISP_VT = keep(Memory.alloc(0x200));
  for (let s = 0; s < 0x200; s += 4) {
    const n = s / 4 < DISP_NARGS.length ? DISP_NARGS[s / 4] : 0;
    const types = ['pointer'];
    for (let i = 0; i < n; i++) types.push('uint32');
    const slot = s;
    DISP_VT.add(s).writePointer(keep(new NativeCallback(function (self) {
      const a = Array.prototype.slice.call(arguments, 1);
      if (Process.getCurrentThreadId() !== S.abTid) return forwardReal(slot, types, a);
      log(0xD1500000 | slot, self, a.length > 0 ? a[0] : 0, a.length > 1 ? (a[1] ^ ((a.length > 2 ? a[2] : 0) << 16)) : 0);
      const r = S.dispRet[slot];
      if (r === undefined) return slot === 0x20 ? 0xbeef : 0;
      return (typeof r === 'function' ? r(self, a) : r) >>> 0;
    }, 'uint32', types, 'thiscall')));
  }

  // Stack-argument count of every slot of jgld.dll's derived Surface vtable 0x1011d0b0 (60 slots, same method), the class
  // the display's slot +0x7c creates (Surface_createFull 0x00474e01 then calls its slot +4 with four arguments and
  // Widget_applyPalette 0x00478a0c its slot +0xec with one).
  const SURF_NARGS = [
    1, 4, 0, 2, 0, 2, 0, 2, 0, 1, 0, 1, 1, 1, 8, 7, 3, 2, 3, 2, 3, 4, 3, 2, 9, 6, 4, 4, 1, 4, 1, 1, 4, 3, 4, 4, 4, 4,
    4, 4, 4, 4, 1, 0, 1, 3, 4, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1];
  // A recording vtable: slot s records {tag | s, this, arg0, arg1 ^ (arg2 << 16)} and returns retFor(s) (a number or a
  // function(self, args); undefined = 0), looked up at call time. nargs: an array per slot index, or function(slot).
  function recVtable(tag, nargs, retFor) {
    const vt = keep(Memory.alloc(0x200));
    for (let s = 0; s < 0x200; s += 4) {
      const n = typeof nargs === 'function' ? nargs(s) : (s / 4 < nargs.length ? nargs[s / 4] : 0);
      const types = ['pointer'];
      for (let i = 0; i < n; i++) types.push('uint32');
      const slot = s;
      vt.add(s).writePointer(keep(new NativeCallback(function (self) {
        const a = Array.prototype.slice.call(arguments, 1);
        log((tag | slot) >>> 0, self, a.length > 0 ? a[0] : 0, a.length > 1 ? (a[1] ^ ((a.length > 2 ? a[2] : 0) << 16)) : 0);
        const r = retFor(slot);
        if (r === undefined) return 0;
        return (typeof r === 'function' ? r(self, a) : r) >>> 0;
      }, 'uint32', types, 'thiscall')));
    }
    return vt;
  }
  // The recording Surface's per-key slot results (S.surfRet), shared vtable.
  S.surfRet = {};
  const SURF_VT = recVtable(0x5F000000, SURF_NARGS, function (s) { return S.surfRet[s]; });
  // Recording objects for arguments the functions under test call through (an `owner` whose slot +0x16c is a callback,
  // c3ef layoutViewTree): one shared vtable per (nargs signature), results per key in S.objRet[tag][slot].
  S.objRet = {};
  const OBJ_VT = {};
  function objVtable(tag, nargsBySlot) {
    const key = tag + ':' + JSON.stringify(nargsBySlot);
    if (!OBJ_VT[key]) {
      OBJ_VT[key] = recVtable(tag, function (s) { return nargsBySlot[s] || 0; },
                              function (s) { return (S.objRet[tag] || {})[s]; });
    }
    return OBJ_VT[key];
  }
  // built at load time, not during a key (no NativeCallback is created while the first thread is frozen): the owner
  // object layoutViewTree calls back through slot +0x16c with the window (0x0047a8f2, `push esi`)
  objVtable(0x0B000000, { 0x16c: 1 });

  // Import preset: what the window teardown / timer chains reach (c3di, c3cy, c3dl). Fixtures add their own.
  function importPreset() {
    return {
      'USER32.dll!ReleaseCapture': { args: 0, ret: 1 },
      'WINMM.dll!timeKillEvent': { ret: 0 },
      'USER32.dll!KillTimer': { ret: 1 },
      'WINMM.dll!timeSetEvent': function (a) { return (0x7100 | (a[0].toUInt32() & 0xff)) >>> 0; },
      'USER32.dll!SetTimer': function (a) { return (0x7200 | (a[2].toUInt32() & 0xff)) >>> 0; },
    };
  }

  // Function stand-ins (calleeStand, c3ea): for one key, replace a function that is not under test with a recorder on
  // the A/B thread (other threads go to the original). spec: {addr, abi ('thiscall' | 'mscdecl' | 'stdcall'), nargs
  // (stack arguments), ret: number | function(self, args)}. Reverted from setTimeout(0) with a per-install token.
  const live = {};
  let nextToken = 1;
  function standIn(spec, reverts) {
    const at = ptr(spec.addr);
    const abi = spec.abi || 'thiscall';
    const types = (abi === 'thiscall' ? ['pointer'] : []).concat(Array(spec.nargs || 0).fill('uint32'));
    const orig = keep(new NativeFunction(at, 'uint32', types, abi));
    const tid = Process.getCurrentThreadId();
    const cb = keep(new NativeCallback(function () {
      const a = Array.prototype.slice.call(arguments);
      if (Process.getCurrentThreadId() !== tid) return orig.apply(null, a);
      const self = abi === 'thiscall' ? a.shift() : ptr(0);
      log(spec.addr, self, a.length > 0 ? a[0] : 0, a.length > 1 ? a[1] : 0);
      const r = typeof spec.ret === 'function' ? spec.ret(self, a) : (spec.ret || 0);
      return r >>> 0;
    }, 'uint32', types, abi));
    const k = at.toString();
    if (live[k]) { Interceptor.revert(at); delete live[k]; }
    Interceptor.replace(at, cb);
    live[k] = nextToken++;
    reverts.push([at, live[k]]);
  }

  globalThis.__uiTree = function (opts) {
    opts = opts || {};
    const arenaSize = opts.arenaSize || 0x10000;
    // 1. stand-ins
    const h = __heapStandIn({ module: EXE, alloc: [0xa504f, 0xa57f1], free: [0xa5007, 0xa4ffc], realloc: [0xa6980],
                              arenaSize: arenaSize, fill: opts.fill === undefined ? 0xcd : opts.fill,
                              failMask: opts.failMask, failSize: opts.failSize, exhaust: opts.exhaust });
    const imports = importPreset();
    Object.keys(opts.imports || {}).forEach(function (k) { imports[k] = opts.imports[k]; });
    const s = __importStandIn({ module: EXE, imports: imports });
    const reverts = [];
    (opts.stand || []).forEach(function (sp) { standIn(sp, reverts); });
    if (reverts.length) {
      Interceptor.flush();
      setTimeout(function () {
        reverts.forEach(function (r) {
          const k = r[0].toString();
          if (live[k] === r[1]) { Interceptor.revert(r[0]); delete live[k]; }
        });
        Interceptor.flush();
      }, 0);
    }
    // 2. recorder and display (fixture-owned, outside the arena)
    const rec = Memory.alloc(REC_SIZE);
    for (let k = 0; k < REC_SIZE; k += 4) rec.add(k).writeU32(0);
    const disp = Memory.alloc(0x40);
    disp.writePointer(DISP_VT);
    for (let k = 4; k < 0x40; k += 4) disp.add(k).writeU32(0xd15b0000 + k);
    S.rec = rec;
    S.abTid = Process.getCurrentThreadId();
    S.dispRet = opts.dispRet || {};
    S.surfRet = opts.surfRet || {};
    S.objRet = opts.objRet || {};
    const opNew = fn(0x004a57f1, 'pointer', ['uint32'], 'mscdecl');
    // 3. freeze, globals
    __freeze.thread();
    __freeze.atFlush(function () { S.rec = null; S.dispRet = {}; S.surfRet = {}; S.objRet = {}; S.abTid = 0; });
    // negative-control builds: arm every exported <Batch>NegCtl switch for this key only (freeze.js; a normal build has
    // none, so this does nothing there)
    __freeze.negOn();
    S.realDisp = ptr(0x0083ad50).readPointer();     // read with the first thread down, before the swap below
    __freeze.save(ptr(ZLIST), ZLIST_BYTES);
    for (let k = 0; k < ZLIST_BYTES; k += 4) ptr(ZLIST).add(k).writeU32(0);
    GLOBALS.forEach(function (g) {
      const p = ptr(g[0]);
      if (g[1] === undefined) { __freeze.save(p, 4); return; }
      const v = g[1] === null ? (opts.display === false ? 0 : disp.toUInt32()) : g[1];
      __freeze.setU32(p, v);
    });

    const T = {
      arena: h.arena, freelog: h.log, implog: s.log, rec: rec, disp: disp, heap: h,
      // a block from the arena through the game's operator new (the stand-in carves it)
      alloc: function (n) {
        const p = opNew(n);
        if (p.isNull()) throw new Error('uitree: arena allocation of ' + n + ' bytes failed');
        return p;
      },
      // NativeFunction to code at addr (through the hook when one is installed there)
      call: function (addr, ret, args, abi) { return fn(addr, ret, args, abi || 'thiscall'); },
      // NativeFunction to the ORIGINAL code at addr: the MinHook trampoline when an SG_HOOK is installed there
      // (SimGolfShim_FindHook), else addr. For building inputs with a function that is itself under test.
      orig: function (addr, ret, args, abi) {
        const o = origAddr(addr);
        return fn(o.toUInt32(), ret, args, abi || 'thiscall');
      },
      // from now on every allocation request of exactly n bytes returns NULL, in both arms (arena header +0xc is in the
      // $heap state region); call it after the build
      failSize: function (n) { h.arena.add(0xc).writeU32(n >>> 0); },
      // allocate a class and run its constructor; extra overrides the constructor's stack arguments
      make: function (kind, extra) {
        const k = KINDS[kind];
        if (!k) throw new Error('uitree: unknown kind ' + kind);
        const p = T.alloc(k[0]);
        const args = extra || k[2];
        const ctor = fn(k[1], 'pointer', ['pointer'].concat(args.map(function () { return 'uint32'; })), 'thiscall');
        const r = ctor.apply(null, [p].concat(args));
        if (!r.equals(p)) throw new Error('uitree: ' + kind + ' constructor returned ' + r + ' for ' + p);
        return p;
      },
      size: function (kind) { return KINDS[kind][0]; },
      // make the allocation call `n` calls from now (0 = the next one) return NULL, in both arms: the arena header's
      // call counter (+4) and fail mask (+8) are part of the $heap state region, so each arm starts from the same count
      failAfter: function (n) {
        const k = h.arena.add(4).readU32() + (n | 0);
        if (k >= 32) throw new Error('uitree: failAfter beyond call 31 (' + k + ')');
        h.arena.add(8).writeU32((h.arena.add(8).readU32() | (1 << k)) >>> 0);
      },
      // a NUL-terminated string in the arena
      str: function (text) {
        const p = T.alloc(text.length + 1);
        p.writeUtf8String(text);
        return p;
      },
      // a recording jgld Surface in the arena (0x40 bytes, fields junk-free zero): the object the display's slot +0x7c
      // returns in reality; slot results per key from opts.surfRet
      surface: function () {
        const p = T.alloc(0x40);
        for (let k = 4; k < 0x40; k += 4) p.add(k).writeU32(0);
        p.writePointer(SURF_VT);
        return p;
      },
      // a recording object in the arena whose vtable records every slot; nargsBySlot gives each called slot's stack
      // argument count (its vtable must have been built at load time, see objVtable above); results per key from
      // opts.objRet[tag]
      recObj: function (tag, nargsBySlot, size) {
        const p = T.alloc(size || 0x20);
        for (let k = 4; k < (size || 0x20); k += 4) p.add(k).writeU32(0);
        const key = tag + ':' + JSON.stringify(nargsBySlot);
        if (!OBJ_VT[key]) throw new Error('uitree: recording vtable ' + key + ' was not built at load time');
        p.writePointer(OBJ_VT[key]);
        return p;
      },
      // ListModel::append 0x00489890 (`ret 8`): adds one row through S3List_add (its nodes come from the arena)
      append: function (model, text, x) {
        return fn(0x00489890, 'int', ['pointer', 'pointer', 'int'], 'thiscall')(model, text, x | 0);
      },
      // window globals that point into the tree; names: hover [ab40, ab44], drag [ab30, ab34], capture, tipOwner,
      // tipText, tipCallback, aaf0, modal, ac18, cookie, z (an array of windows put in the z-list, count set)
      globals: function (g) {
        const P = function (v) { return v === undefined || v === null || v === 0 ? 0 : ptr(v).toUInt32(); };
        const set = function (addr, v) { ptr(addr).writeU32(v >>> 0); };    // saved at step 3
        if (g.hover) { set(0x0083ab40, P(g.hover[0])); set(0x0083ab44, P(g.hover[1])); }
        if (g.drag) { set(0x0083ab30, P(g.drag[0])); set(0x0083ab34, P(g.drag[1])); }
        if ('capture' in g) set(0x0083aa98, P(g.capture));
        if ('tipOwner' in g) set(0x0083aac0, P(g.tipOwner));
        if ('tipText' in g) set(0x0083aac4, P(g.tipText));
        if ('aaf0' in g) set(0x0083aaf0, P(g.aaf0));
        if ('modal' in g) set(0x0083ab54, typeof g.modal === 'number' ? g.modal : P(g.modal));
        if ('ac18' in g) set(0x0083ac18, g.ac18);
        if ('cookie' in g) set(0x00839650, g.cookie);
        if (g.z) {
          if (g.z.length > 0x80) throw new Error('uitree: z-list longer than 0x80');
          g.z.forEach(function (w, i) { ptr(ZLIST).add(4 * i).writeU32(P(w)); });
          set(0x0083ab94, g.z.length);
        }
      },
      // the fixture object diff_hook receives: the tree's regions plus the caller's named pointers
      fx: function (named) {
        const out = { heap: h.arena, freelog: h.log, implog: s.log, rec: rec, disp: disp };
        Object.keys(named || {}).forEach(function (k) { out[k] = named[k]; });
        return out;
      },
      log: log,
    };
    return T;
  };
  globalThis.__uiTree.GLOBALS = GLOBALS.map(function (g) { return g[0]; });
  globalThis.__uiTree.KINDS = KINDS;
  globalThis.__uiTree.REC_SIZE = REC_SIZE;
  globalThis.__uiTree.DISP_NARGS = DISP_NARGS;
})();
