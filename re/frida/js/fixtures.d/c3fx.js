// Fixtures for batch c3fx (2026-10-16): showCharacterEditor 0x004385d0 (key c3fx_ce).
// Reimplementation: shim/src/re/c3fx.cpp. Registry: re/frida/registry.d/c3fx.py. Purpose: log/c3/c3fx_purpose.md.
//
// Method (c3fw's recipe for the SEH screens, log/c3/c3fw_notes.txt, with c3fv's ordered recorder):
// - 41 callees are recording stand-ins on XREC, an ordered recorder (framemask.js allows 32 stubs per key; this body
//   has 51 callees): the same TID-gated native gate as framemask.js / c3fv's REC (the A/B thread enters JavaScript,
//   every other thread jumps to the trampoline), one log for every stub, so the ORDER of all calls is compared. A value
//   in the A/B thread's stack is logged as a frame token: 0xF5000000 | offset inside the frame Image (declared by the
//   Image_ctor stand-in, the only frame object this body passes to a callee), 0xF6000000 for any other stack value.
//   The object table lives in JavaScript and is emptied when the block's epoch word (+0x0c, 0 in diff_hook's snapshot)
//   is 0, so both arms start from an empty table.
// - Run for real: View_toLocal, nearestCharControl (+ approxDistance), typeBit7Clear, thoughtFlag, cellAt,
//   buildGolferName, Random::range, _itoa; the pumps (pumpMessages, waitTicks) are modal.js models with a scripted
//   input queue; uitree.js freezes the first thread, provides the recording display (the body's slot +0x9c call) and the
//   import preset; M.display gives View_toLocal the virtual cursor.
// - Answers depend only on arguments, the row and the stub's call index n in the arm (kept in the compared block).
// - FILES: nothing touches the disk. listMatchingFiles, loadThemeFile, promptAndSaveFile, scrollTextWindow and
//   showPopup are recorders; listMatchingFiles serves the row's directory (by call index) into 0x0080b130.
// - The uninitialised frame dword [F+0x30] (hair-style counter, read first at 0x00439b12 / 0x00439b29) lies 0xc below
//   the frame Image: the Image_ctor stand-in writes the row's `style` at this - 0xc, and the reimplementation keeps the
//   same distance (struct CeFrame), so both arms read the same value.
// - Raw writes ({k: 'raw'} modal events) change the records the body reads between frames (state sweep rows), so one
//   vector reaches many drawing branches; they are logged by modal.js in both arms.
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const keep = function (x) { (globalThis.__diffKeepAlive || []).push(x); return x; };
  const w32 = function (a, v) { ptr(a).writeU32(v >>> 0); };
  const le = function (v) { v = v >>> 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; };

  // ------------------------------------------------------------------------------------------- XREC (ordered recorder)
  // Block (compared): +0 bytes used, +4 records, +8 FNV-1a over every record, +0xc epoch, +0x10 frame pointers mapped,
  // +0x14 implicit stack values, +0x18 .. +0x117 call count per stub (64), +0x120 records {addr, nwords, words...}.
  const HEAD = 0x120;
  const code = keep(__keepAlloc(Process.pageSize));
  Memory.protect(code, Process.pageSize, 'rwx');
  code.writeByteArray([0x64, 0xa1, 0x04, 0x00, 0x00, 0x00, 0xc3, 0x90, 0x64, 0xa1, 0x0c, 0x0e, 0x00, 0x00, 0xc3, 0x90]);
  const stackBase = keep(new NativeFunction(code, 'uint32', []));
  const stackLow = keep(new NativeFunction(code.add(8), 'uint32', []));
  const live = {};
  let tok = 1;
  function fnv(h, w) {
    for (let k = 0; k < 4; k++) { h ^= (w >>> (8 * k)) & 0xff; h = Math.imul(h, 0x01000193) >>> 0; }
    return h >>> 0;
  }
  function mkRec(size) {
    const blk = keep(__keepAlloc(size));
    blk.writeByteArray(new Uint8Array(size).buffer);
    blk.add(8).writeU32(0x811c9dc5);
    const R = { blk: blk, size: size, n: 0, tid: Process.getCurrentThreadId(), hi: stackBase() >>> 0, lo: stackLow() >>> 0,
                on: true, img: 0 };
    if (globalThis.__freeze) __freeze.atFlush(function () { R.on = false; });
    function sync() { if (blk.add(0xc).readU32() === 0) { R.img = 0; blk.add(0xc).writeU32(1); } }
    function bump(off) { blk.add(off).writeU32(blk.add(off).readU32() + 1); }
    R.map = function (v) {
      v = v >>> 0;
      if (R.raw || !(v >= R.lo && v < R.hi)) return v;
      sync();
      if (R.img !== 0 && v >= R.img && v < R.img + 0x2B8) { bump(0x10); return (0xF5000000 | (v - R.img)) >>> 0; }
      bump(0x14);
      return 0xF6000000;
    };
    R.declareImage = function (p) { sync(); R.img = p >>> 0; };
    R.record = function (addr, words) {
      let h = blk.add(8).readU32();
      h = fnv(h, addr >>> 0);
      words.forEach(function (w) { h = fnv(h, w >>> 0); });
      blk.add(8).writeU32(h);
      blk.add(4).writeU32(blk.add(4).readU32() + 1);
      const used = blk.readU32();
      const need = 8 + 4 * words.length;
      if (HEAD + used + need <= size) {
        const e = blk.add(HEAD + used);
        e.writeU32(addr >>> 0);
        e.add(4).writeU32(words.length);
        words.forEach(function (w, i) { e.add(8 + 4 * i).writeU32(w >>> 0); });
        blk.writeU32(used + need);
      }
    };
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
    R.strWords = strWords;
    // spec: addr, abi ('thiscall' | 'mscdecl' | 'stdcall'), nargs, ret (number | 'this' | function(self, args, n)),
    // write(self, args, n) side effects before the return, str: {i: nwords} (argument i as a C string), buf: nwords
    // (the shared text buffer 0x0051a068 at the call), peek: [addresses] whose dwords are appended
    R.stand = function (spec) {
      const idx = R.n++;
      if (idx >= 64) throw new Error('XREC: more than 64 stubs');
      const at = ptr(spec.addr), k = at.toString();
      if (live[k]) { try { Interceptor.revert(at); } catch (e) { /* gone */ } delete live[k]; Interceptor.flush(); }
      const abi = spec.abi || 'mscdecl';
      const thiscall = abi === 'thiscall';
      const types = (thiscall ? ['uint32'] : []).concat(Array(spec.nargs || 0).fill('uint32'));
      let orig = null;
      const cb = keep(new NativeCallback(function () {
        const a = Array.prototype.slice.call(arguments).map(function (x) { return x >>> 0; });
        if (!R.on) return orig.apply(null, arguments);
        sync();
        const self = thiscall ? a[0] : 0;
        const args = thiscall ? a.slice(1) : a;
        const cnt = 0x18 + 4 * idx;
        const n = blk.add(cnt).readU32();
        blk.add(cnt).writeU32(n + 1);
        if (spec.region) R.declareImage(self);
        const words = a.map(R.map);
        Object.keys(spec.str || {}).forEach(function (i) {
          strWords(i === 'this' ? self : args[+i], spec.str[i]).forEach(function (w) { words.push(w); });
        });
        if (spec.buf) strWords(0x0051A068, spec.buf).forEach(function (w) { words.push(w); });
        (spec.peek || []).forEach(function (g) { words.push(ptr(g).readU32()); });
        R.record(spec.addr, words);
        if (spec.write) spec.write(self, args, n);
        const r = spec.ret;
        if (r === 'this') return self >>> 0;
        if (typeof r === 'function') return r(self, args, n) >>> 0;
        return (r === undefined ? 0 : r) >>> 0;
      }, 'uint32', types, abi));
      const stub = keep(__keepAlloc(Process.pageSize));
      Memory.protect(stub, Process.pageSize, 'rwx');
      stub.writeByteArray([0xEB, 0xFE]);
      const tidAddr = stub.add(0x38), origAddr = stub.add(0x3c);
      const body = [0x50, 0x64, 0xa1, 0x24, 0x00, 0x00, 0x00]
        .concat([0x3b, 0x05], le(tidAddr.toUInt32()))
        .concat([0x58])
        .concat([0x0f, 0x85], le(stub.add(0x2a).sub(stub.add(0x16)).toUInt32()))
        .concat([0xe9], le(cb.sub(stub.add(0x1b)).toUInt32()));
      stub.add(0x02).writeByteArray(body);
      stub.add(0x2a).writeByteArray([0xff, 0x25].concat(le(origAddr.toUInt32())));
      tidAddr.writeU32(R.tid >>> 0);
      let tramp;
      try {
        tramp = Interceptor.replaceFast(at, stub);
      } catch (e) {
        Interceptor.revert(at);
        Interceptor.flush();
        tramp = Interceptor.replaceFast(at, stub);
      }
      orig = keep(new NativeFunction(tramp, 'uint32', types, abi));
      origAddr.writePointer(tramp);
      const token = tok++;
      live[k] = token;
      Interceptor.flush();
      stub.add(1).writeU8(0x00);
      setTimeout(function () {
        if (live[k] !== token) return;
        try { Interceptor.revert(at); } catch (e) { /* gone */ }
        delete live[k];
        Interceptor.flush();
      }, 0);
      return idx;
    };
    return R;
  }

  // ------------------------------------------------------------------------------------------------- addresses
  const BUF = 0x0051A068, NAMES = 0x0080B130, THEME = 0x00567328;
  const CHAR = 0x004D6088, CSZ = 0x230, NCHAR = 4;
  const GOLF = 0x005794C0, PREV = 0x00582DC0;
  const TYPES = 0x005849E0, COMMENTS = 0x00543D10, STORY = 0x0058DD70;
  const EDIT = 0x005A9F78;
  const rec = function (c) { return CHAR + c * CSZ; };
  const golf = function (g) { return GOLF + g * 0x100; };
  // every region the body, the real callees or the stand-ins write: saved under the freeze, restored before the resume
  // (the registry's state lists the same regions)
  const SAVES = [[CHAR, CSZ * NCHAR], [0x004E1618, CSZ], [golf(3), 0x200], [PREV, 0x100], [TYPES, 0x2C * NCHAR],
                 [COMMENTS, 0x4E2 * NCHAR], [STORY, 0x38 * 4], [0x005A5A04, 0x10], [0x0058B198, 4], [0x005AA554, 4],
                 [0x005A9F54, 4], [0x005AA55C, 4], [0x005AA6C4, 4], [BUF, 0x400], [0x005A9F6C, 0x40], [THEME, 0x40],
                 [NAMES, 100 * 16], [EDIT, 0x5A8], [0x00822D9C, 8]];

  function writeStr(addr, s, cap) {
    const p = ptr(addr);
    for (let i = 0; i < cap; i++) p.add(i).writeU8(i < s.length ? s.charCodeAt(i) & 0xff : 0);
  }
  function readStr(addr, max) {
    let s = '';
    for (let i = 0; i < max; i++) { const c = ptr(addr).add(i).readU8(); if (c === 0) break; s += String.fromCharCode(c); }
    return s;
  }
  function fill(addr, n, f) { for (let i = 0; i < n; i++) ptr(addr).add(i).writeU8(f(i) & 0xff); }

  // the root window 0x00519a60: zeroed offsets so the local point is the cursor (c3es's rootNeutral)
  const ROOT = 0x00519A60;
  function rootNeutral() {
    [[ROOT + 0x9C, 4], [ROOT + 0x1AC, 8], [ROOT + 0x1BC, 8]].forEach(function (s) { __freeze.save(ptr(s[0]), s[1]); });
    w32(ROOT + 0x9C, 0); w32(ROOT + 0x1AC, 0); w32(ROOT + 0x1B0, 0); w32(ROOT + 0x1BC, 0); w32(ROOT + 0x1C0, 0);
  }

  // ------------------------------------------------------------------------------------------------- stand-ins
  const C = { row: null, bio: null };
  const at = function (q, n) { return q[n % q.length]; };
  function stands(R) {
    const S = function (spec) { R.stand(spec); };
    S({ addr: 0x00448200, abi: 'mscdecl', nargs: 1, ret: 0 });                                  // startSoundSlot
    S({ addr: 0x004481B0, abi: 'mscdecl', nargs: 5, ret: 0 });                                  // playSound
    S({ addr: 0x00474AE0, abi: 'thiscall', nargs: 0, region: true, ret: 'this',                 // Image_ctor
        write: function (self) {
          ptr(self).sub(0xC).writeS32(C.row.style | 0);                                          // [F+0x30] (see header)
          for (let w = 0; w < 4; w++) ptr(self).add(4 * w).writeU32(0x5F000000 | 4 * w);
        } });
    S({ addr: 0x00474C40, abi: 'thiscall', nargs: 0, ret: 0x000C3F05 });                        // Image_dtor
    S({ addr: 0x00475840, abi: 'thiscall', nargs: 5, str: { 0: 8 }, ret: 0 });                  // loadImage
    S({ addr: 0x00475C60, abi: 'thiscall', nargs: 7, ret: 0 });                                 // Surface_blitThunk
    S({ addr: 0x00473CB0, abi: 'thiscall', nargs: 7, ret: 0 });                                 // Surface_drawScaled
    S({ addr: 0x00473E60, abi: 'thiscall', nargs: 4, ret: 0 });                                 // Image_draw
    S({ addr: 0x00462020, abi: 'mscdecl', nargs: 1, ret: 0 });                                  // composeGolferSprite
    S({ addr: 0x004628D0, abi: 'mscdecl', nargs: 8, ret: 0 });                                  // addSprite
    S({ addr: 0x004762D0, abi: 'thiscall', nargs: 4, ret: 0 });                                 // setDrawTarget
    S({ addr: 0x00404BC0, abi: 'mscdecl', nargs: 4, str: { 0: 10 }, ret: 0 });                  // drawCenteredText2
    S({ addr: 0x00404B70, abi: 'mscdecl', nargs: 4, str: { 0: 10 }, ret: 0 });                  // drawCenteredText
    S({ addr: 0x004049D0, abi: 'mscdecl', nargs: 4, str: { 0: 12 }, ret: 0 });                  // drawText4049d0
    S({ addr: 0x00480B00, abi: 'thiscall', nargs: 5, ret: 0 });                                 // fillWindowRect
    S({ addr: 0x0045B0D0, abi: 'mscdecl', nargs: 4, buf: 12, ret: 0 });                         // drawMessageBuffer
    S({ addr: 0x00469B00, abi: 'mscdecl', nargs: 4, ret: 0,                                     // buildGolferComment
        write: function (self, a, n) {
          // a comment made from the arguments and the call index; every third call also sets the colour word the body
          // compares with 0x80006318 (0x004394ca)
          writeStr(BUF, 'c' + (a[0] | 0) + '.' + (a[1] & 0xff) + '.' + (n % 7), 16);
          if ((n + (a[0] | 0)) % 3 === 0) w32(0x0058B198, 0x80006318);
          else if ((n + (a[0] | 0)) % 3 === 1) w32(0x0058B198, 0x80001111);
        } });
    S({ addr: 0x00432620, abi: 'mscdecl', nargs: 2, buf: 6, ret: 0 });                          // drawPanelBackdrop
    S({ addr: 0x00480C80, abi: 'thiscall', nargs: 1, ret: 0 });                                 // Window::refreshFull
    S({ addr: 0x0043D2A0, abi: 'mscdecl', nargs: 3, str: { 0: 10 },                             // listMatchingFiles
        ret: function (self, a, n) { return at(C.row.lists || [[]], n).length; },
        write: function (self, a, n) {
          const l = at(C.row.lists || [[]], n);
          for (let i = 0; i < 16; i++) writeStr(NAMES + 100 * i, i < l.length ? l[i] : '', 100);
        } });
    S({ addr: 0x0046DE70, abi: 'mscdecl', nargs: 3, buf: 24,                                    // scrollTextWindow
        ret: function (self, a, n) { return at(C.row.scroll || [-1], n); } });
    S({ addr: 0x00437FA0, abi: 'mscdecl', nargs: 3, str: { 0: 6 }, ret: 0x7FA0,                 // loadThemeFile
        write: function (self, a) {
          // the loaded character: name, profession and choice bytes from the file name (record a[1])
          const r = rec(a[1] | 0), nm = readStr(a[0], 8);
          writeStr(r + 0x10, 'L' + nm, 0x10);
          writeStr(r, 'P' + nm.slice(0, 4), 0x10);
          let h = 0; for (let i = 0; i < nm.length; i++) h += nm.charCodeAt(i);
          ptr(r + 0x22).writeU8(h % 0x14); ptr(r + 0x24).writeU8(h % 10); ptr(r + 0x25).writeU8((h >> 2) % 10);
          ptr(r + 0x27).writeU8(h % 4); ptr(r + 0x28).writeU8(h % 5);
          w32(r + 0x2C, (ptr(r + 0x2C).readU32() ^ (h & 8)) >>> 0);
        } });
    S({ addr: 0x00437910, abi: 'mscdecl', nargs: 3,                                             // promptAndSaveFile
        ret: function (self, a, n) { return at(C.row.save || [0], n); } });
    S({ addr: 0x0046D6E0, abi: 'mscdecl', nargs: 5, buf: 20, ret: 0 });                         // showPopup
    S({ addr: 0x00438390, abi: 'mscdecl', nargs: 1,                                             // pickCharEditor
        ret: function (self, a, n) { return at(C.row.pick || [-1], n); } });
    S({ addr: 0x0045B2C0, abi: 'mscdecl', nargs: 4, str: { 0: 4 }, buf: 12, ret: 0,             // textInputBox
        write: function (self, a, n) { writeStr(BUF, at(C.row.input || [''], n), 0x31); } });
    S({ addr: 0x0040CC00, abi: 'mscdecl', nargs: 5, ret: 0 });                                  // drawPanelS2
    S({ addr: 0x00479830, abi: 'thiscall', nargs: 3, ret: function (s, a, n) { return 0x11 + n; } });   // pixelRed
    S({ addr: 0x004798C0, abi: 'thiscall', nargs: 3, ret: function (s, a, n) { return 0x22 + n; } });   // pixelGreen
    S({ addr: 0x00479950, abi: 'thiscall', nargs: 3, ret: function (s, a, n) { return 0x33 + n; } });   // pixelBlue
    S({ addr: 0x00483420, abi: 'thiscall', nargs: 4,                                            // Palette::nearest
        ret: function (s, a, n) { return 0x5A00 + a[0] + 2 * a[1] + 3 * a[2] + n; } });
    S({ addr: 0x00486110, abi: 'thiscall', nargs: 6, ret: 1 });                                 // EditBox::create
    S({ addr: 0x00486B30, abi: 'thiscall', nargs: 1, ret: 0 });                                 // EditBox::setCaretActive
    S({ addr: 0x00486250, abi: 'thiscall', nargs: 1, ret: 0 });                                 // EditBox::resizeBuffer
    S({ addr: 0x00486200, abi: 'thiscall', nargs: 1, str: { 0: 12 }, ret: 0,                    // EditBox::setText
        write: function (self, a, n) {
          // the text the player ends with: the row's bio for this entry, in the compared buffer $bio, whose address
          // the EditBox's text pointer +0x574 (0x005aa4ec) then holds
          writeStr(C.bio.toUInt32(), at(C.row.bio || [''], n), 0x200);
          w32(EDIT + 0x574, C.bio.toUInt32());
        } });
    S({ addr: 0x0047B670, abi: 'thiscall', nargs: 1, ret: 0 });                                 // Window::show
    S({ addr: 0x00483BB0, abi: 'mscdecl', nargs: 0, ret: 0 });                                  // showScreen
    S({ addr: 0x00483C70, abi: 'mscdecl', nargs: 0, ret: 0 });                                  // redrawAll
    S({ addr: 0x00483D80, abi: 'mscdecl', nargs: 0, ret: 0 });                                  // leaveModal483d80
    S({ addr: 0x004860D0, abi: 'thiscall', nargs: 0, ret: 0 });                                 // EditBox::releaseState
    S({ addr: 0x00483D30, abi: 'mscdecl', nargs: 0, ret: 0x3D30 });                             // enterModal483d30
  }

  // ------------------------------------------------------------------------------------------------- rows
  // record seeds: character c, golfer g (type byte +0x10, flags +0x11, +0xae, +0xb3, skills +0xe8), the type card,
  // the comment lines, the preview golfer's thought word (+0x80, thoughtFlag)
  function seed(row) {
    for (let c = 0; c < NCHAR; c++) {
      const r = rec(c);
      fill(r, CSZ, function (i) { return (i * 7 + c * 13) & 0x3f; });
      writeStr(r, 'Prof' + c, 0x10);
      writeStr(r + 0x10, 'Name' + c, 0x10);
      writeStr(r + 0x30, 'Bio of ' + c, 0x40);
      ptr(r + 0x20).writeU8(c === 1 ? 0x05 : 0x12);
      ptr(r + 0x21).writeU8(c === 1 ? 0x89 : 0x0D);                // bit 7: typeBit7Clear 0, control 15 live
      ptr(r + 0x22).writeU8(3 + c);
      ptr(r + 0x23).writeU8(0x25 + c);
      ptr(r + 0x24).writeU8(c === 2 ? 9 : 0);
      ptr(r + 0x25).writeU8(c === 2 ? 0 : 9);
      ptr(r + 0x27).writeU8(c === 2 ? 3 : 0);
      ptr(r + 0x28).writeU8(c === 1 ? 4 : 0);
      w32(r + 0x2C, c === 3 ? 0 : 0x10 + c);
      const t = TYPES + 0x2C * c;
      fill(t, 0x2C, function (i) { return i; });
      ptr(t).writeU8(c * 3); ptr(t + 1).writeS8(c - 1); ptr(t + 2).writeU8(0xF0 | (c + 1)); ptr(t + 0x29).writeU8(0);
      ptr(t + 0x2A).writeS16(c === 2 ? 0 : 100 + c);
      for (let l = 0; l < 25; l++) writeStr(COMMENTS + 0x4E2 * c + 0x32 * l, (l % 3 === 1) ? '' : 'line' + c + '.' + l, 0x32);
    }
    for (let g = 3; g < 5; g++) {
      const p = golf(g);
      fill(p, 0x100, function (i) { return (i * 5 + g) & 0x1f; });
      ptr(p + 0x10).writeU8(g === 4 ? 0x21 : 0x40);
      ptr(p + 0x11).writeU8(g === 4 ? 0x03 : 0x04);
      ptr(p + 0xAE).writeS16(g === 4 ? 2 : 0);
      ptr(p + 0xB3).writeU8(2);
      [3, 10, 0, 5, 1, 9, 2, 7, 4, 6].forEach(function (s, k) { ptr(p + 0xE8 + k).writeU8(s); });
    }
    fill(PREV, 0x100, function (i) { return i & 0x0f; });
    ptr(PREV + 0xB3).writeU8(1);
    ptr(PREV + 0x80).writeU16(0);
    fill(STORY, 0x38 * 4, function (i) { return (i % 0x38) === 0 ? 2 + (i / 0x38) : i & 0x0f; });
    [0, 4, 10, 1, 7, 0, 3, 8, 2, 5].forEach(function (s, k) { ptr(0x005A5A04 + k).writeU8(s); });
    w32(0x0058B198, 0x80004210);
    w32(0x005A9F54, row.last === undefined ? -1 : row.last);
    w32(0x005AA554, row.count | 0);
    w32(0x005AA55C, 0x55); w32(0x005AA6C4, 0);
    writeStr(BUF, '', 0x40);
    writeStr(THEME, 'Classic', 16);
    for (let i = 0; i < 16; i++) writeStr(NAMES + 100 * i, '', 100);
    (row.init || []).forEach(function (q) { ptr(q[0]).writeU8(q[1]); });
  }

  // input helpers: a control's centre (nearestCharControl's table 0x004c7b38) and an action = [cursor], [press], [], []
  const CTL = [[210, 150], [210, 185], [210, 220], [93, 127], [93, 156], [93, 182], [93, 211], [93, 239], [48, 22],
               [258, 22], [454, 30], [454, 80], [454, 130], [454, 180], [454, 230], [328, 56], [328, 107], [328, 157],
               [762, 33], [760, 220], [580, 98]];
  const CUR = function (x, y) { return { k: 'cursor', x: x, y: y }; };
  const KEY = function (code) { return { k: 'key', a: code, code: code }; };
  const RAW = function (set) { return { k: 'raw', set: set }; };
  const IDLE = function (n) { const o = []; for (let i = 0; i < n; i++) o.push([]); return o; };
  const act = function (xy, kind) { return [[CUR(xy[0], xy[1])], [{ k: kind || 'press1', x: xy[0], y: xy[1] }], [], []]; };
  const ctl = function (k, kind) { return act(CTL[k], kind); };
  const hover = function (xy, n) { return [[CUR(xy[0], xy[1])]].concat(IDLE(n)); };
  const EXIT = ctl(19);
  const cat = function () { return [].concat.apply([], Array.prototype.slice.call(arguments)); };
  const NAMEX = 154, NAMEY = [32, 48, 68, 88];
  const CMX = 0x242, CMY = function (r) { return 0x10E + 16 * r + 4; };
  const SKX = 0x208, SKY = function (r) { return 0x40 + 16 * r + 2; };
  const R1A = rec(1), R3A = rec(3);

  const ROWS = [
    // 0 (character 1, golfer 3 non-member, bit 7 set): hover every control (each highlight), the name lines (hot 0..3),
    //   a point near no control, then exit (y 44 / 64 / 84: the first rows where (y - 0x18) / 20 and / 21 differ)
    { c: 1, g: 3, style: 5,
      script: cat(CTL.map(function (xy) { return [CUR(xy[0], xy[1])]; }),
                  NAMEY.concat([44, 64, 84]).map(function (y) { return [CUR(NAMEX, y)]; }), [[CUR(0x300, 0x240)], []], EXIT) },
    // 1 (character 2, bit 7 clear: control 15 reads as -1): a press at control 15's spot (no action), the preview flags
    //   0..2, the record flags 3..7, control 11 twice, cancel 18 (the record restored, the preview flags kept), 4 again,
//   13 / 14 / 16 left and right with their wraps, exit
    { c: 2, g: 3, style: 0,
      script: cat(ctl(15), ctl(0), ctl(1), ctl(2), ctl(0), ctl(3), ctl(4), ctl(5), ctl(6), ctl(7), ctl(3), ctl(11),
                  ctl(11), ctl(18), ctl(4), ctl(13), ctl(13, 'press2'), ctl(13, 'press2'), ctl(14), ctl(14), ctl(14, 'press2'), ctl(16),
                  ctl(16, 'press2'), ctl(16, 'press2'), EXIT) },
    // 2 (character 1, bit 7 set): control 15 left (4 -> 0 wrap) and right; hair style 10 from the seeded frame word -7
    //   (left, left, right); gender 17 twice; presets 12 (answers -1, 5, 0x13, 0x19, -3), exit
    { c: 1, g: 3, style: -7, pick: [-1, 5, 0x13, 0x19, -3],
      script: cat(ctl(15), ctl(15, 'press2'), ctl(15, 'press2'), ctl(10), ctl(10), ctl(10, 'press2'), ctl(17), ctl(12),
                  ctl(12), ctl(17), ctl(12), ctl(12), ctl(12), EXIT) },
    // 3 (character 1, non-member: *.chr): load 8 three times: 3 names picked 1 -> loadThemeFile; 5 names cancelled (-1);
    //   no names; exit
    { c: 1, g: 3, style: 1, lists: [['ann.chr', 'bob.chr', 'cy.chr'], ['d1.chr', 'd2.chr', 'd3.chr', 'd4.chr', 'd5.chr'], []],
      scroll: [1, -1], script: cat(ctl(8), ctl(8), ctl(8), EXIT) },
    // 4 (character 2, golfer 4 member: *.pro, skills from the golfer): load 8 (pick 0), save 9 refused then done (.pro
    //   popup), skill bars: left on 3, left on 10 (beep), right on 0 (beep), right on 5, left at y 0x3e (row 0); a press
    //   on control 20 (member: nothing); hovering; exit (member with +0xae != 0: the story table entry 2 is written)
    { c: 2, g: 4, style: 2, lists: [['pa.pro', 'pb.pro']], scroll: [0], save: [0, 1],
      script: cat(ctl(8), ctl(9), ctl(9), act([SKX, SKY(0)]), act([SKX, SKY(1)]), act([SKX, SKY(2)], 'press2'),
                  act([SKX, SKY(3)], 'press2'), act([SKX, 0x3E]), act([SKX + 0x18, SKY(5)]), act([SKX + 0x19, SKY(5)]),
                  act([SKX, 0xDD]), act([SKX, 0xDE]), ctl(20), EXIT) },
    // 5 (character 0, golfer 4 member: skills from 0x005a5a04, skill presses ignored), save done (.pro), exit
    { c: 0, g: 4, style: 3, save: [1],
      script: cat(act([SKX, SKY(1)]), ctl(9), EXIT) },
    // 6 (character 3): the name area: name (typed / empty), profession (typed / empty), the bits 3..6 choice left x5 and
    //   right x5, the bits 0..2 choice left x4 and right x4; exit
    { c: 3, g: 3, style: 4, input: ['Zed', '', 'Golf pro', ''],
      init: [[R3A + 0x21, 0x0D]],
      script: cat(act([NAMEX, NAMEY[0]]), act([NAMEX, NAMEY[0]]), act([NAMEX, NAMEY[1]]), act([NAMEX, NAMEY[1]]),
                  act([NAMEX, NAMEY[2]]), act([NAMEX, NAMEY[2]]), act([NAMEX, NAMEY[2]]), act([NAMEX, NAMEY[2]]),
                  act([NAMEX, NAMEY[2]]), act([NAMEX, NAMEY[2]], 'press2'), act([NAMEX, NAMEY[2]], 'press2'),
                  act([NAMEX, NAMEY[2]], 'press2'), act([NAMEX, NAMEY[2]], 'press2'), act([NAMEX, NAMEY[2]], 'press2'),
                  act([NAMEX, NAMEY[3]]), act([NAMEX, NAMEY[3]]), act([NAMEX, NAMEY[3]]), act([NAMEX, NAMEY[3]]),
                  act([NAMEX, NAMEY[3]], 'press2'), act([NAMEX, NAMEY[3]], 'press2'), act([NAMEX, NAMEY[3]], 'press2'),
                  act([NAMEX, NAMEY[3]], 'press2'), EXIT) },
    // 7 (character 3): comment lines 0, 7, 19 and 20 edited; presses just inside and outside the x bound; exit
    { c: 3, g: 3, style: 6, input: ['first', 'seventh', '', 'twentieth', 'edge', 'out'],
      script: cat(act([CMX, CMY(0)]), act([CMX, CMY(7)]), act([CMX, CMY(19)]), act([CMX, 0x10E + 16 * 20 + 2]),
                  act([CMX - 0xB1, CMY(3)]), act([CMX + 0xB2, CMY(4)]), act([CMX, 0x10D]), EXIT) },
    // 8 (character 1, non-member): bio 20 ended by a press after three frames, then by Enter; exit
    { c: 1, g: 3, style: 7, bio: ['A new bio', 'Second bio text'],
      script: cat([[CUR(CTL[20][0], CTL[20][1])], [{ k: 'press1', x: CTL[20][0], y: CTL[20][1] }], [], [], [],
                   [{ k: 'press1', x: 0x40, y: 0x40 }], [], []],
                  [[CUR(CTL[20][0], CTL[20][1])], [{ k: 'press1', x: CTL[20][0], y: CTL[20][1] }], [], [KEY(0x0D)], [], []],
                  EXIT) },
    // 9 (character 1, non-member): every tip (controls 8..20 hovered 13 frames each) and control 3's (none: below 8);
    //   the counter seeded at 9 for control 19 entered as `last`
    { c: 1, g: 3, style: 0, last: 19, count: 9, cursor: CTL[19],
      script: cat([[]], [8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 3].map(function (k) { return hover(CTL[k], 13); })
                  .reduce(function (a, b) { return a.concat(b); }, []), hover(CTL[19], 3), [[{ k: 'press1', x: 760, y: 220 }], [], []]) },
    // 10 (character 2, golfer 4 member): tip 20 (member: none), tip 19; exit
    { c: 2, g: 4, style: 0, script: cat(hover(CTL[20], 13), hover(CTL[19], 13), [[{ k: 'press1', x: 760, y: 220 }], [], []]) },
  ];

  // 11 (character 1): a state sweep: each frame writes the records the drawing reads (raw dwords, logged by modal.js),
  //   so the figure (sheet +4, body >= 0x14, thought 0 / 1 / 2), the sprite set (both genders, every flag), the
  //   status / family words and the type card kinds -1..7 all draw; exit
  const t1 = TYPES + 0x2C;
  const dw = function (b0, b1, b2, b3) { return ((b0 & 0xff) | (b1 & 0xff) << 8 | (b2 & 0xff) << 16 | (b3 & 0xff) << 24) >>> 0; };
  const SW = [];
  const sweepRec = [
    // [rec +0x20 dword (bytes 0x20..0x23), rec +0x2c dword, preview flags dword +0x10 (type, flags, x, x), preview +0x80 word,
    //  preview +0xb0 dword (status word, set byte, b3), type dword +0 (bytes 0..3), type dword +0x28 (+0x29 byte, +0x2a word)]
    [dw(0x05, 0x89, 3, 0x25), 0x00000011, dw(0, 4, 0, 0), 0x0000, dw(0x08, 0, 1, 1), dw(3, 0, 0xF2, 0), dw(0, 0xFF, 0, 0)],
    [dw(0x0D, 0x89, 0x15, 0x15), 0x00000008, dw(0, 6, 0, 0), 0x8000, dw(0x10, 0, 1, 1), dw(3, 0xFD, 0xF0, 0), dw(0, 0, 0, 0)],
    [dw(0x08, 0x85, 0x16, 0x35), 0x00000000, dw(0x20, 2, 0, 0), 0x4000, dw(0x20, 0, 1, 1), dw(0, 4, 0xF1, 0), dw(0, 0, 12, 0)],
    [dw(0x00, 0x81, 2, 0x05), 0x00000000, dw(0x20, 0, 0, 0), 0x0000, dw(0x40, 0, 1, 2), dw(7, 0, 0xF2, 0), dw(0, 0, 0xFB, 0xFF)],
    [dw(0x08, 0x01, 2, 0x15), 0x00000000, dw(0, 4, 0, 0), 0x0000, dw(0x01, 0, 1, 1), dw(1, 1, 0xF3, 0), dw(0, 0, 0, 0)],
    [dw(0x00, 0x04, 0x14, 0x25), 0x00000000, dw(0, 6, 0, 0), 0x8000, dw(0x02, 0, 1, 1), dw(2, 2, 0xF4, 0), dw(0, 0, 1, 0)],
    [dw(0x00, 0x00, 0x14, 0x35), 0x00000000, dw(0, 1, 0, 0), 0x4000, dw(0x04, 0, 1, 1), dw(2, 2, 0xF5, 0), dw(0, 0, 1, 0)],
    [dw(0x08, 0x08, 5, 0x05), 0x00000080, dw(0, 0, 0, 0), 0x0000, dw(0x7F, 0, 1, 1), dw(2, 2, 0xF6, 0), dw(0, 0, 1, 0)],
    [dw(0x1F, 0x00, 5, 0x05), 0x00000000, dw(0x20, 7, 0, 0), 0x0000, dw(0x00, 0, 1, 3), dw(2, 2, 0xF7, 0), dw(0, 0, 1, 0)],
    [dw(0x00, 0x88, 5, 0x05), 0x00000000, dw(0x20, 7, 0, 0), 0x8000, dw(0x00, 0, 1, 0), dw(9, 0x80, 0xF0, 0), dw(0, 0, 0, 0)],
  ];
  sweepRec.forEach(function (s) {
    const set = {};
    set[R1A + 0x20] = s[0]; set[R1A + 0x2C] = s[1]; set[PREV + 0x10] = s[2]; set[PREV + 0x80] = s[3];
    set[PREV + 0xB0] = s[4]; set[t1] = s[5]; set[t1 + 0x28] = s[6];
    SW.push([RAW(set)]);
    SW.push([]);
  });
  ROWS.push({ c: 1, g: 3, style: 0, script: cat([[CUR(0x300, 0x240)]], SW, EXIT) });
  // 12 (character 1, non-member): the negative-remainder arms: hair style 10 pressed right from the seeded frame word -7
  //   ((-7 + 3) % 4 through 0x00439b38); control 16 left on 0x7f (0x80 is negative, 0x00439cf7) and right on 0xfb
  //   (-6 + 4, 0x00439cf7); a non-member save (golfer -1, 0x004399f7, the .chr popup); exit
  {
    const s1 = {}, s2 = {};
    s1[R1A + 0x24] = dw(0, 9, 0x17, 0x7F);
    s2[R1A + 0x24] = dw(0, 9, 0x17, 0xFB);
    ROWS.push({ c: 1, g: 3, style: -7, save: [1],
                script: cat(ctl(10, 'press2'), [[RAW(s1)], []], ctl(16), [[RAW(s2)], []], ctl(16, 'press2'), ctl(9), EXIT) });
  }

  function key(o) {
    return function () {
      const M = __modal({ maxCalls: 6000, maxFrames: 1500, script: [] });
      const R = mkRec(0x10000);
      R.raw = !!o.raw;
      C.bio = keep(__keepAlloc(0x200));
      C.bio.writeByteArray(new Uint8Array(0x200).buffer);
      stands(R);
      const T = __uiTree({ arenaSize: 0x4000, imports: M.imports() });
      rootNeutral();
      SAVES.forEach(function (s) { __freeze.save(ptr(s[0]), s[1]); });
      M.arm();
      M.display(T.disp);
      if (__freeze.negOn) __freeze.negOn();
      const fx = Object.assign({ T: T, M: M, xrec: R.blk, bio: C.bio }, T.fx(), M.fx());
      ROWS.forEach(function (row, i) {
        Object.defineProperty(fx, 'r' + i, {
          enumerable: true, configurable: true,
          get: function () {
            fx.M.select(row.script, row.cursor || [0x40, 0x40]);
            C.row = row;
            seed(row);
            C.bio.writeByteArray(new Uint8Array(0x200).buffer);
            return row.c >>> 0;
          },
        });
      });
      return fx;
    };
  }
  F.c3fx_ce = key({});
  F.c3fx_ce_raw = key({ raw: true });      // control: the frame mapping off (a correct body reads RED)
})();
