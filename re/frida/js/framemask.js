// Frame masks for recording stand-ins (batch c3es, round 32, 2026-10-13). Shared helper like heapstandin.js /
// importstandin.js / freeze.js / uitree.js / modal.js; diff_hook.py should load it after modal.js (until it does, the
// batches that use it prepend it to their fixture fragment, and the guard below makes a second definition a no-op).
//
// Why: a screen keeps its working objects in its own FRAME (showThemePackSelect: two arrays of Surface records built by
// the vector constructor iterator, an Image, column tables; showShortcutsScreen: an Image and two Surfaces under an EH
// frame) and hands pointers to them to its callees. A recording stand-in that logs those pointers as values reads RED
// between two correct arms, because the original's frame and the reimplementation's are laid out differently; masking
// the pointer out (c3ep's / c3er's `mask`) makes the arms agree but no longer compares WHICH object each call targets,
// which is most of what such a screen does. This helper logs a pointer that falls in the A/B thread's stack as a
// layout-independent token instead:
//
//   token = 0xF5000000 | (ordinal << 16) | offset
//
// where `ordinal` is the index of the OBJECT the pointer falls in, in the order the objects were declared during the
// arm, and `offset` is the byte offset inside that object. An object is declared when a stood-in constructor (or the
// vector constructor iterator, or any stub with a `region` spec) receives it: the declaration gives the base and the
// size (or the element size and the count of an array, i.e. a {base, stride} pair: the offset of element i, field f is
// i * stride + f). Both arms construct the same objects in the same order (that order is itself compared: it is the
// order of the constructor calls), so the same object gets the same ordinal however the two frames place it, and a
// pointer to the wrong object, or to the right array one element off, reads RED. A stack pointer inside no declared
// object becomes an IMPLICIT object of its own (token prefix 0xF6, numbered in order of first appearance); the block
// counts them, so a key whose every frame pointer was declared shows 0 there.
//
//   const FM = __frameMask.begin({ size: 0x4000 });        // BEFORE the freeze (allocates)
//   FM.stand({ addr: 0x00473ab0, abi: 'thiscall', nargs: 0, region: { arg: 'this', size: 0x2c }, ret: 'this' });
//   FM.stand({ addr: 0x00473bf0, abi: 'thiscall', nargs: 7, ret: 0 });      // its `this` and args are mapped
//   return Object.assign(fx, FM.fx());                     // fx.fmlog = the block
//
// Registry side: add ('$fmlog', 0, <size>) to the key's state.
//
// THE CONTRACT
//  1. What counts as a frame pointer: a value in [DeallocationStack, StackBase) of the thread that called begin() (the
//     A/B thread; TEB +0xe0c / +4 read by a three-instruction native helper). Everything else is logged as it is.
//  2. Arms: the object table lives in JavaScript, but whether an arm has started is read from the block: word +4 (the
//     epoch) is 0 in diff_hook's snapshot, so the first stand-in call of every arm finds it 0, empties the table and sets
//     it to 1. diff_hook restores the block between the arms, so each arm starts from an empty table and both arms
//     number their objects from 0. Nothing a stub returns depends on JavaScript state that survives an arm.
//  3. Stubs (FM.stand(spec), installed with Interceptor.replaceFast before the freeze; other threads reach the existing
//     code, i.e. the SG_HOOK detour or the original, through the trampoline): spec =
//       addr, abi ('thiscall' | 'mscdecl' | 'stdcall'), nargs (stack arguments; `this` is extra for thiscall)
//       ret:     number | 'this' | function(self, args, n, FM) (n = this stub's call index in the arm, from the block)
//       region:  {arg: 'this' | i, size: n} or {arg, strideArg: i, countArg: j}: declare an object (before logging)
//       retire:  'this' | i: the object starting at that pointer is taken out of the table (after logging)
//       fill:    {arg, words}: after declaring, write `words` dwords 0x5F000000 | ordinal << 16 | 4k into the object
//                (a stood-in constructor leaves the object's first words known in both arms)
//       deref:   {i: nwords}: also log nwords dwords read at argument i (mapped the same way)
//       str:     {i: nwords}: also log the first nwords dwords of the C string at argument i
//       write:   function(self, args, n, FM): side effects (an output argument), run before `ret`
//       forward: true: call the original after logging and return its result (ret ignored)
//     A record is {addr, nwords, words...}: `this`, the arguments, then deref / str words, every word mapped. Every
//     record also enters a running FNV-1a hash (+0xc), so a log that overflows is still compared.
//  4. FM.map(v) maps one value with the current table (fixtures may use it in their own recorders); FM.region(p, size)
//     and FM.retire(p) are the explicit forms.
//  5. Controls: opts.raw = true turns the mapping off (every pointer logged as its value). A correct reimplementation
//     must then read RED wherever it passes a frame pointer; that is the evidence the mask is what makes it GREEN.
//  6. Block (opts.size bytes, default 0x4000, every byte compared):
//        +0x00 'FMK1'  +0x04 epoch  +0x08 records  +0x0c FNV-1a  +0x10 log bytes used  +0x14 objects declared
//        +0x18 implicit objects  +0x1c frame pointers mapped  +0x20 overflows (ordinal > 0xff or offset > 0xffff)
//        +0x24 objects retired  +0x28 .. +0xa7 call counter per stub (32, in FM.stand order)  +0xc0 records
//  7. Lifetime: the stubs revert from setTimeout(0) with a per-install token; a stub reached on the A/B thread after the
//     key (the block was released by the freeze flush, or no key is active) forwards to the original.
(function () {
  if (globalThis.__frameMask) return;
  const keep = function (x) { (globalThis.__diffKeepAlive || []).push(x); return x; };

  // native helpers: StackBase (TEB +4) and DeallocationStack (TEB +0xe0c) of the calling thread
  const code = keep(Memory.alloc(Process.pageSize));
  Memory.protect(code, Process.pageSize, 'rwx');
  code.writeByteArray([0x64, 0xa1, 0x04, 0x00, 0x00, 0x00, 0xc3, 0x90,          // mov eax, fs:[4]; ret
                       0x64, 0xa1, 0x0c, 0x0e, 0x00, 0x00, 0xc3, 0x90]);        // mov eax, fs:[0xe0c]; ret
  const stackBase = keep(new NativeFunction(code, 'uint32', []));
  const stackLow = keep(new NativeFunction(code.add(8), 'uint32', []));

  const HEAD = 0xc0, NSTUB = 32;
  const S = { blk: null, size: 0, lo: 0, hi: 0, raw: false, tid: 0, objs: [], nstub: 0 };

  function fnv(h, w) {
    for (let k = 0; k < 4; k++) { h ^= (w >>> (8 * k)) & 0xff; h = Math.imul(h, 0x01000193) >>> 0; }
    return h >>> 0;
  }
  function bump(off, by) { S.blk.add(off).writeU32((S.blk.add(off).readU32() + (by === undefined ? 1 : by)) >>> 0); }
  // a new arm (epoch 0 in the snapshot diff_hook restored): empty the object table
  function sync() {
    if (S.blk.add(4).readU32() === 0) {
      S.objs = [];
      S.blk.add(4).writeU32(1);
    }
  }
  function isStack(v) { return v >= S.lo && v < S.hi; }
  function find(v) {
    for (let i = S.objs.length - 1; i >= 0; i--) {
      const o = S.objs[i];
      if (o.live && v >= o.base && v < o.base + o.size) return o;
    }
    return null;
  }
  function declare(base, size, implicit) {
    const o = { base: base >>> 0, size: Math.max(1, size >>> 0), ord: S.objs.length, live: true, implicit: !!implicit };
    S.objs.push(o);
    bump(implicit ? 0x18 : 0x14);
    return o;
  }
  function map(v) {
    v = v >>> 0;
    if (S.raw || S.blk === null || !isStack(v)) return v;
    sync();
    let o = find(v);
    if (o === null) o = declare(v, 1, true);
    bump(0x1c);
    const off = v - o.base;
    if (o.ord > 0xff || off > 0xffff) bump(0x20);
    return ((o.implicit ? 0xF6000000 : 0xF5000000) | ((o.ord & 0xff) << 16) | (off & 0xffff)) >>> 0;
  }
  function retire(v) {
    v = v >>> 0;
    if (S.blk === null || !isStack(v)) return;
    sync();
    for (let i = S.objs.length - 1; i >= 0; i--) {
      const o = S.objs[i];
      if (o.live && o.base === v) { o.live = false; bump(0x24); return; }
    }
  }
  function record(addr, words) {
    const blk = S.blk;
    let h = blk.add(0xc).readU32();
    h = fnv(h, addr >>> 0);
    words.forEach(function (w) { h = fnv(h, w >>> 0); });
    blk.add(0xc).writeU32(h);
    bump(8);
    const used = blk.add(0x10).readU32();
    const need = 8 + 4 * words.length;
    if (HEAD + used + need <= S.size) {
      const e = blk.add(HEAD + used);
      e.writeU32(addr >>> 0);
      e.add(4).writeU32(words.length);
      words.forEach(function (w, i) { e.add(8 + 4 * i).writeU32(w >>> 0); });
      blk.add(0x10).writeU32(used + need);
    }
  }
  function strWords(p, n) {
    const out = new Array(n).fill(0);
    if (p === 0) return out;
    let end = false;
    for (let q = 0; q < 4 * n; q++) {
      const c = end ? 0 : ptr(p).add(q).readU8();
      if (c === 0) end = true;
      out[q >> 2] |= c << (8 * (q & 3));
    }
    return out.map(function (x) { return x >>> 0; });
  }

  // A per-stand hand-encoded TID gate (modal.js / importstandin.js's method): the A/B thread jumps to the JS callback,
  // every game thread jumps straight to the trampoline without entering JavaScript or taking Frida's JS lock. This is
  // essential when a stub sits on a function the game's render thread calls every frame (setDrawTarget, blitThunk,
  // drawScaled): a JS-side TID check would keep that thread inside the callback, and the freeze could never reach a
  // golf .text safe point (first thread never stopped). The entry parks on a self-jump (EB FE) while the body is being
  // written, and a single atomic byte write at +1 (FE -> 00) activates it, so a game thread that hits the hot function
  // during the install cannot execute a half-written stub (replaceFast may activate before the body is filled). Layout:
  //   +0x00 EB FE            spin (flipped to EB 00 = fall through to +0x02 on activate)
  //   +0x02 50               push eax
  //   +0x03 64 a1 24 00 00 00  mov eax, fs:[0x24]        ; current TID
  //   +0x09 3b 05 <&tid>      cmp eax, [tid]
  //   +0x0f 58               pop eax
  //   +0x10 0f 85 <rel FWD>   jne FWD
  //   +0x16 e9 <rel cb>       jmp cb                     ; A/B thread -> JS
  //   +0x2a ff 25 <&orig> FWD: jmp [orig]                ; game threads -> the trampoline
  //   +0x38 tid   +0x3c orig
  const le = function (v) { v = v >>> 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; };
  const live = {};
  let nextToken = 1;
  function stand(spec) {
    if (S.blk === null) throw new Error('framemask: stand() before begin()');
    if (S.nstub >= NSTUB) throw new Error('framemask: more than ' + NSTUB + ' stubs in one key');
    const idx = S.nstub++;
    const at = ptr(spec.addr), k = at.toString();
    if (live[k]) { try { Interceptor.revert(at); } catch (e) { /* gone */ } delete live[k]; Interceptor.flush(); }
    const abi = spec.abi || 'mscdecl';
    const thiscall = abi === 'thiscall';
    const types = (thiscall ? ['uint32'] : []).concat(Array(spec.nargs || 0).fill('uint32'));
    const tid = S.tid;
    const blk = S.blk;
    let orig = null;
    const cb = keep(new NativeCallback(function () {
      const a = Array.prototype.slice.call(arguments).map(function (x) { return x >>> 0; });
      if (S.blk !== blk) return orig.apply(null, arguments);   // key over (the gate already filtered other threads)
      sync();
      const self = thiscall ? a[0] : 0;
      const args = thiscall ? a.slice(1) : a;
      const at_ = function (sel) { return sel === 'this' ? self : args[sel]; };
      const cnt = 0x28 + 4 * idx;
      const n = blk.add(cnt).readU32();
      blk.add(cnt).writeU32(n + 1);
      let decl = null;
      if (spec.region) {
        const r = spec.region, base = at_(r.arg);
        if (isStack(base)) {
          const size = r.size !== undefined ? r.size : (args[r.strideArg] >>> 0) * (args[r.countArg] >>> 0);
          decl = declare(base, size, false);
        }
      }
      if (decl !== null && spec.fill) {
        for (let w = 0; w < spec.fill.words; w++)
          ptr(decl.base).add(4 * w).writeU32((0x5F000000 | ((decl.ord & 0xff) << 16) | (4 * w)) >>> 0);
      }
      const words = a.map(map);
      Object.keys(spec.deref || {}).forEach(function (i) {
        const p = at_(i === 'this' ? 'this' : +i);
        for (let w = 0; w < spec.deref[i]; w++) words.push(p === 0 ? 0 : map(ptr(p).add(4 * w).readU32()));
      });
      Object.keys(spec.str || {}).forEach(function (i) {
        strWords(at_(i === 'this' ? 'this' : +i), spec.str[i]).forEach(function (w) { words.push(w); });
      });
      record(spec.addr, words);
      if (spec.retire !== undefined) retire(at_(spec.retire));
      if (spec.write) spec.write(self, args, n, FM);
      if (spec.forward) return orig.apply(null, a) >>> 0;
      const r = spec.ret;
      if (r === 'this') return self >>> 0;
      if (typeof r === 'function') return r(self, args, n, FM) >>> 0;
      return (r === undefined ? 0 : r) >>> 0;
    }, 'uint32', types, abi));
    // the native gate, installed hot-patch-safe: entry spins (EB FE) until the whole body, tid and orig are written.
    const stub = keep(Memory.alloc(Process.pageSize));
    Memory.protect(stub, Process.pageSize, 'rwx');
    stub.writeByteArray([0xEB, 0xFE]);                 // spin at +0 before anything downstream can run
    const tidAddr = stub.add(0x38), origAddr = stub.add(0x3c);
    const body = [0x50, 0x64, 0xa1, 0x24, 0x00, 0x00, 0x00]                 // +0x02
      .concat([0x3b, 0x05], le(tidAddr.toUInt32()))                         // cmp eax,[tid]
      .concat([0x58])
      .concat([0x0f, 0x85], le(stub.add(0x2a).sub(stub.add(0x16)).toUInt32()))   // jne FWD (+0x2a), next +0x16
      .concat([0xe9], le(cb.sub(stub.add(0x1b)).toUInt32()));               // jmp cb, next +0x1b
    stub.add(0x02).writeByteArray(body);
    stub.add(0x2a).writeByteArray([0xff, 0x25].concat(le(origAddr.toUInt32())));   // FWD: jmp [orig]
    tidAddr.writeU32(tid >>> 0);
    let tramp;
    try {
      tramp = Interceptor.replaceFast(at, stub);
    } catch (e) {                    // another helper's stand-in still sits there: take it off
      Interceptor.revert(at);
      Interceptor.flush();
      tramp = Interceptor.replaceFast(at, stub);
    }
    orig = keep(new NativeFunction(tramp, 'uint32', types, abi));
    origAddr.writePointer(tramp);
    const token = nextToken++;
    live[k] = token;
    Interceptor.flush();
    stub.add(1).writeU8(0x00);                         // atomic flip: EB 00 falls through to the gate at +0x02
    setTimeout(function () {
      if (live[k] !== token) return;
      try { Interceptor.revert(at); } catch (e) { /* gone */ }
      delete live[k];
      Interceptor.flush();
    }, 0);
    return idx;
  }

  const FM = {
    map: map, stand: stand, retire: retire,
    region: function (p, size) { if (S.blk !== null && isStack(p >>> 0)) { sync(); declare(p, size, false); } },
    fx: function () { return { fmlog: S.blk }; },
    block: function () { return S.blk; },
    // a probe's view of the table (not compared)
    objects: function () { return S.objs.map(function (o) { return Object.assign({}, o); }); },
  };

  globalThis.__frameMask = {
    begin: function (opts) {
      opts = opts || {};
      const size = opts.size || 0x4000;
      const blk = keep(Memory.alloc(size));
      blk.writeByteArray(new Uint8Array(size).buffer);
      blk.writeU32(0x314b4d46);                                  // 'FMK1'
      blk.add(0xc).writeU32(0x811c9dc5);
      S.blk = blk; S.size = size; S.raw = !!opts.raw; S.objs = []; S.nstub = 0;
      S.tid = Process.getCurrentThreadId();
      S.hi = stackBase() >>> 0; S.lo = stackLow() >>> 0;
      // a usable range is non-empty, does not start at 0 and is at most 64 MB (a thread's reserved stack)
      if (S.lo === 0 || !(S.lo < S.hi) || S.hi - S.lo > 0x4000000)
        throw new Error('framemask: no stack range (0x' + S.lo.toString(16) + ', 0x' + S.hi.toString(16) + ')');
      // the key is over (the freeze flush, or the next begin): every stub forwards
      if (globalThis.__freeze) __freeze.atFlush(function () { if (S.blk === blk) S.blk = null; });
      return FM;
    },
    HEAD: HEAD,
  };
})();
