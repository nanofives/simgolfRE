// Fixtures for batch c3fw (2026-10-15): loadGameBrowserScreen 0x0043b610 (c3fw_lgb), showThemePackSelect 0x004725b0
// (c3fw_tp), pickAProScreen 0x0043a8c0 (c3fw_pp) and courseActionMenu 0x0046f550 (c3fw_cam), in that order below.
// Reimplementation: shim/src/re/c3fw.cpp. Registry: re/frida/registry.d/c3fw.py. Purpose: log/c3/c3fw_purpose.md.
//
// Method (c3fv's recipe for the SEH + array screens, log/c3/c3fv_notes.txt): the vector constructor / destructor
// iterators ??_L 0x004a55d2 / ??_M 0x004a5713 are framemask.js stubs with `forward`: each call is logged (its array
// pointer as an object token, the element size, count, constructor and destructor) and then the real iterator runs, so
// the 15 Surfaces are really constructed by Surface_ctor 0x00473ab0 and destroyed through 0x004041f0 / Surface_free
// 0x00473ae0 in both arms and the original's frame and SEH record stay valid. The ??_L stub declares the array as a
// {base, stride} region: a pointer to element k, field f is logged as k * 0x2c + f, so the A/B compares which element
// every Surface_blit / Image_draw targets. Every other callee that draws, loads, saves or deletes is a recording
// stand-in (framemask.js, TID-gated); the pumps are modal.js models with a scripted input queue; uitree.js freezes the
// first thread and puts its recording display at 0x0083ad50 (enterModal483d30's slots +0x148 / +0x14c).
//
// FILES: nothing touches the disk. listMatchingFiles 0x0043d2a0 is stood in and serves the row's fixture directory into
// the name table 0x0080b130 (100 bytes per name); _remove 0x004a64b8, loadGame 0x0040b9b0, loadCourse 0x0040b840,
// saveGame 0x0040b4a0 and showPopup 0x0046d6e0 are recording stand-ins: a delete is a logged path, never a file
// operation. loadCourse / showPopup read their path / text from the shared buffer 0x0051a068, so their stubs copy it
// into the compared path log ($pathlog) at the call.
//
// The stand-ins' answers depend only on their arguments and on compared memory: loadGame with a title request (third
// argument 1) writes a title made from the name into 0x0051a068; a preview load (all flags 0) writes the course
// globals the details panel reads, from the name's byte sum; showPopup answers row.popup[n] (n = its call index, kept
// in the frame-mask block, which is compared); listMatchingFiles returns the row's directory.
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const keep = function (x) { (globalThis.__diffKeepAlive || []).push(x); return x; };
  const w32 = function (a, v) { ptr(a).writeU32(v >>> 0); };

  const CUR = function (x, y) { return { k: 'cursor', x: x, y: y }; };
  const PRESS = function (x, y) { return { k: 'press1', x: x, y: y }; };
  const PRESS2 = function (x, y) { return { k: 'press2', x: x, y: y }; };
  const KEY = function (code, a) { return { k: 'key', a: a === undefined ? code : a, code: code }; };
  const IDLE = function (n) { const out = []; for (let i = 0; i < n; i++) out.push([]); return out; };

  // the root window 0x00519a60: zeroed offsets so the local point is the cursor (c3es's rootNeutral)
  const ROOT = 0x00519A60;
  function rootNeutral() {
    [[ROOT + 0x9C, 4], [ROOT + 0x1AC, 8], [ROOT + 0x1BC, 8]].forEach(function (s) { __freeze.save(ptr(s[0]), s[1]); });
    w32(ROOT + 0x9C, 0); w32(ROOT + 0x1AC, 0); w32(ROOT + 0x1B0, 0); w32(ROOT + 0x1BC, 0); w32(ROOT + 0x1C0, 0);
  }

  // ---- globals the body or the stand-ins write (saved under the freeze, restored before the resume)
  const NAMES = 0x0080B130, NAME_MAX = 48;
  const BUF = 0x0051A068;
  const THEME = 0x00567328;
  const HOLES = 0x005685F0, PAR = 0x0059AAFC, YARDS = 0x0058D36C, CASH = 0x00571FD4, FUN = 0x0059AE78;
  const SK1 = 0x005A882C, SK2 = 0x0056949C, SK3 = 0x005A636C, RECORD = 0x0056A524, COURSE_IX = 0x0059BF90;
  const HOLE_TBL = 0x00575CBC, HOLE_STRIDE = 0x208, HOLE_MAX = 8;
  const COURSE_TBL = 0x00571FF4;
  const SAVES = [[NAMES, 100 * NAME_MAX], [BUF, 0x400], [0x005A9F6C, 0x40], [THEME, 0x40], [0x005A34E0, 1],
                 [0x0059E7B8, 4], [HOLES, 4], [PAR, 4], [YARDS, 4], [CASH, 4], [FUN, 4], [SK1, 4], [SK2, 4], [SK3, 4],
                 [RECORD, 4], [COURSE_IX, 4], [HOLE_TBL - 4, HOLE_STRIDE * HOLE_MAX], [COURSE_TBL, 0x2E * 4]];

  function writeStr(addr, s, cap) {
    const p = ptr(addr);
    for (let i = 0; i < cap; i++) p.add(i).writeU8(i < s.length ? s.charCodeAt(i) & 0xff : 0);
  }
  function readStr(addr, max) {
    let s = '';
    for (let i = 0; i < max; i++) { const c = ptr(addr).add(i).readU8(); if (c === 0) break; s += String.fromCharCode(c); }
    return s;
  }
  function byteSum(addr) { let h = 0; for (let i = 0; i < 64; i++) { const c = ptr(addr).add(i).readU8(); if (!c) break; h += c; } return h; }

  // the path log (compared): +0 entries, +4 bytes used, then {tag, 64 bytes of 0x0051a068} per call
  const PATHLOG = 0x800;
  function pathlog() {
    const b = keep(__keepAlloc(PATHLOG));
    b.writeByteArray(new Uint8Array(PATHLOG).buffer);
    return b;
  }
  function logBuf(b, tag) {
    const used = b.add(4).readU32();
    b.writeU32(b.readU32() + 1);
    if (8 + used + 0x44 > PATHLOG) return;
    const e = b.add(8 + used);
    e.writeU32(tag >>> 0);
    for (let i = 0; i < 0x40; i++) e.add(4 + i).writeU8(ptr(BUF).add(i).readU8());
    b.add(4).writeU32(used + 0x44);
  }

  // a preview load: the course globals the details panel reads, from the name's byte sum h, and the course type / flags
  // (row.themeFromName: the theme name too, so the exit's strcmp 0x0043ca0e sees a change)
  const S = { row: null };
  function preview(nameAddr) {
    const h = byteSum(nameAddr);
    const holes = h % 7;                                  // 0, 1 -> no hole rows; up to 6 -> 5 rows
    w32(HOLES, holes);
    w32(PAR, 54 + h % 20); w32(YARDS, 3000 + 13 * h); w32(CASH, h * 37 - 2000); w32(FUN, h % 11);
    w32(SK1, h * 3 % 1000); w32(SK2, h * 7 % 1000); w32(SK3, (h * 11) % 1000 - 50);
    w32(RECORD, h % 3 === 0 ? 0 : 60 + h % 30);
    w32(COURSE_IX, h % 4);
    // the course type byte and flags word the body saves at entry and restores on exit (0x0043b64f / 0x0043b656,
    // 0x0043c8f7 / 0x0043c8ff, 0x0043caaa / 0x0043cab8): the stand-in changes them so the restore is observable
    ptr(0x005A34E0).writeU8(0x10 + h % 5);
    w32(0x0059E7B8, (Math.imul(h, 0x01010101) ^ 0x5A5A) >>> 0);
    ptr(COURSE_TBL + (h % 4) * 0x2E).writeS8(h % 3);
    for (let i = 0; i < HOLE_MAX; i++) {
      ptr(HOLE_TBL + i * HOLE_STRIDE - 4).writeS8(((i * 3 + h) % 3) + 3 - (i === 2 ? 8 : 0));
      ptr(HOLE_TBL + i * HOLE_STRIDE).writeS16(h % 2 ? 100 + 17 * i + h : -(5 + i));
    }
    if (S.row && S.row.themeFromName) writeStr(THEME, 'TH' + readStr(nameAddr, 4), 16);
  }

  // stand-ins (framemask.js FM.stand specs)
  function stands(pl) {
    return [
      // ??_L / ??_M: logged, then the real iterator (Surface_ctor / 0x004041f0 run on the frame array)
      { addr: 0x004A55D2, abi: 'stdcall', nargs: 5, region: { arg: 0, strideArg: 1, countArg: 2 }, forward: true },
      { addr: 0x004A5713, abi: 'stdcall', nargs: 4, deref: { 0: 3 }, retire: 0, forward: true },
      { addr: 0x00474AE0, abi: 'thiscall', nargs: 0, region: { arg: 'this', size: 0x2B8 }, fill: { words: 4 }, ret: 'this' },
      { addr: 0x00474C40, abi: 'thiscall', nargs: 0, deref: { this: 4 }, retire: 'this', ret: 0xC3F01 },
      { addr: 0x00475840, abi: 'thiscall', nargs: 5, str: { 0: 8 }, ret: 0 },             // loadImage
      { addr: 0x00473BF0, abi: 'thiscall', nargs: 7, deref: { this: 3 }, ret: 0 },        // Surface_blit (+ the element's vtable)
      { addr: 0x004762D0, abi: 'thiscall', nargs: 4, ret: 0 },                            // setDrawTarget
      { addr: 0x00475C60, abi: 'thiscall', nargs: 7, ret: 0 },                            // Surface_blitThunk
      { addr: 0x00473E60, abi: 'thiscall', nargs: 4, ret: 0 },                            // Image_draw
      { addr: 0x00404B70, abi: 'mscdecl', nargs: 4, str: { 0: 12 }, ret: 0 },             // drawCenteredText
      { addr: 0x004049D0, abi: 'mscdecl', nargs: 4, str: { 0: 12 }, ret: 0 },             // drawText4049d0
      { addr: 0x00404BC0, abi: 'mscdecl', nargs: 4, str: { 0: 8 }, ret: 0 },              // drawCenteredText2
      { addr: 0x00480B00, abi: 'thiscall', nargs: 5, ret: 0 },                            // fillWindowRect
      { addr: 0x00480C80, abi: 'thiscall', nargs: 1, ret: 0 },                            // Window::refreshFull
      { addr: 0x00473F60, abi: 'thiscall', nargs: 5, ret: 0 },                            // Surface_stretchBlit
      { addr: 0x0040CA10, abi: 'mscdecl', nargs: 5, ret: 0 },                             // blitPanel
      { addr: 0x0043D2A0, abi: 'mscdecl', nargs: 3, str: { 0: 8, 2: 6 }, ret: function () { return S.row.names.length; },
        write: function () {                                                             // listMatchingFiles
          for (let i = 0; i < NAME_MAX; i++) writeStr(NAMES + 100 * i, i < S.row.names.length ? S.row.names[i] : '', 100);
        } },
      { addr: 0x0040B9B0, abi: 'mscdecl', nargs: 4, str: { 0: 8 }, ret: 0x0B9B,          // loadGame
        write: function (self, a) {
          if (a[2] === 1) writeStr(BUF, 'T-' + readStr(a[0], 6), 16);                    // the title request (0x0043bb4c)
          else if (a[1] === 0 && a[2] === 0 && a[3] === 0) preview(a[0]);                // the list preview (0x0043c894)
          else if (S.row.themeFromName) writeStr(THEME, 'TL' + readStr(a[0], 4), 16);
        } },
      { addr: 0x0040B840, abi: 'mscdecl', nargs: 1, ret: 0x0B84,                          // loadCourse (path in 0x0051a068)
        write: function (self, a) {
          logBuf(pl, 0x0B840000 | a[0]);
          const nm = BUF + 20;                                                           // past the championship directory
          if (a[0] === 1) preview(nm); else if (S.row.themeFromName) writeStr(THEME, 'TC' + readStr(nm, 4), 16);
        } },
      { addr: 0x0046D6E0, abi: 'mscdecl', nargs: 5,                                       // showPopup (text in 0x0051a068)
        ret: function (self, a, n) { const p = S.row.popup || [1]; return p[n % p.length]; },
        write: function () { logBuf(pl, 0x0D6E0000); } },
      { addr: 0x004A64B8, abi: 'mscdecl', nargs: 1, str: { 0: 16 }, ret: 0 },             // _remove: logged, nothing deleted
      { addr: 0x0040B4A0, abi: 'mscdecl', nargs: 1, str: { 0: 6 }, ret: 0 },              // saveGame
      { addr: 0x004658B0, abi: 'mscdecl', nargs: 1, ret: 0 },                             // loadThemes
    ];
  }

  function scene(o) {
    const pl = pathlog();
    const M = __modal({ maxCalls: o.maxCalls || 1500, maxFrames: o.maxFrames || 600, script: [] });
    const FM = __frameMask.begin({ size: 0x8000, raw: !!o.raw });
    (o.stands ? o.stands(pl) : stands(pl)).forEach(function (sp) { FM.stand(sp); });
    const T = __uiTree({ arenaSize: 0x4000, imports: M.imports() });
    rootNeutral();
    (o.saves || SAVES).forEach(function (s) { __freeze.save(ptr(s[0]), s[1]); });
    M.arm();
    M.display(T.disp);
    if (__freeze.negOn) __freeze.negOn();
    return Object.assign({ T: T, M: M, FM: FM, pathlog: pl }, T.fx(), M.fx(), FM.fx());
  }
  // rows: the getter selects the script and cursor, sets the theme name and the entry globals, returns `resume`
  function rows(fx, list) {
    list.forEach(function (row, i) {
      Object.defineProperty(fx, 'r' + i, {
        enumerable: true, configurable: true,
        get: function () {
          fx.M.select(row.script, row.cursor || [0x40, 0x40]);
          S.row = row;
          writeStr(THEME, row.theme || 'Classic', 16);
          ptr(0x005A34E0).writeU8(row.type === undefined ? 2 : row.type);
          w32(0x0059E7B8, row.flags === undefined ? 0x1234 : row.flags);
          for (let k = 0; k < NAME_MAX; k++) writeStr(NAMES + 100 * k, '', 100);
          writeStr(BUF, '', 0x40);
          return row.resume >>> 0;
        },
      });
    });
    return fx;
  }

  // button centres (the approxDistance origins 0x0043c366..0x0043c408) and list rows (0x0043c7c9: (y - 0x74) / 16)
  const B0 = [0x279, 0x22B], B1 = [0x301, 0x22C], B2 = [0x310, 0x78], B3 = [0x310, 0x190], B4 = [0x2C0, 0x22B];
  const ROWY = function (r) { return 0x74 + 16 * r + 5; };
  const LX = 0x200;
  const at = function (b) { return CUR(b[0], b[1]); };
  const click = function (b) { return [at(b), PRESS(b[0], b[1])]; };
  const pick = function (r) { return [CUR(LX, ROWY(r)), PRESS(LX, ROWY(r))]; };   // the pointer moves there too
  const many = function (n) { const out = []; for (let i = 0; i < n; i++) out.push('save' + String.fromCharCode(97 + i % 26) + i + '.sve'); return out; };
  const NAMES5 = ['alpha.sve', '&auto1.sve', 'beta.sve', 'gamma.sve', '&auto22.sve'];

  const ROWS = [
    // 0 (resume 0, saves): saveGame at entry, an autosave title, Esc -> back, the browsing snapshot reloaded
    { resume: 0, names: NAMES5, script: [[], [KEY(0x1B)]] },
    // 1 (resume 1): pick name 2, hover load, press it -> loadGame(name, 0, 0, 0), returns 1
    { resume: 1, names: NAMES5, script: [pick(2), [], [at(B0)], [], click(B0)] },
    // 2 (resume 0): pick an autosave, the preview changes the theme -> loadThemes(1) after the load (flag 1)
    { resume: 0, names: NAMES5, themeFromName: true, script: [pick(1), [], click(B0)] },
    // 3 (championship): pick -> loadCourse(1), load -> loadCourse(0) and the theme reload
    { resume: 0, champ: 1, names: ['links.cse', 'dunes.cse', 'pines.cse'], themeFromName: true,
      script: [pick(0), [], pick(2), [], click(B0)] },
    // 4 (resume 1): pick, delete -> popup "no" (1), delete -> popup "yes" (0) -> _remove from the saves directory, Esc
    { resume: 1, names: NAMES5, popup: [1, 0], script: [pick(3), [], click(B4), [], click(B4), [], [KEY(0x1B)]] },
    // 5 (championship, resume 1): pick, delete yes -> _remove from the championship directory, back button
    { resume: 1, champ: 1, names: ['links.cse', 'dunes.cse'], popup: [0], script: [pick(1), [], click(B4), [], click(B1)] },
    // 6 (40 names): scroll down twice, up once, a click below row 16 (scroll + 4), a pick, a right-button click on
    //    scroll-down, then (more...) gone at the end; Esc
    { resume: 1, names: many(40), script: [click(B3), [], click(B3), [], click(B2), [], pick(17), [], pick(15),
                                           [], [at(B3), PRESS2(B3[0], B3[1])], [], click(B3), [], click(B3), [],
                                           click(B3), [], [KEY(0x1B)]] },
    // 7 (3 names): a click on an empty row (sel >= n: deselect), a pick, then an empty row again; a press left of the
    //    list (x <= 0x140) and one above it (y < 0x74); Esc
    { resume: 1, names: ['one.sve', 'two.sve', 'three.sve'],
      script: [pick(9), [], pick(1), [], pick(10), [], [PRESS(0x100, ROWY(1))], [], [PRESS(LX, 0x50)], [], [KEY(0x1B)]] },
    // 8 (empty directory, resume 0): no scroll bar, load pressed with no selection -> the out-of-range exit loads the
    //    browsing snapshot, returns 0
    { resume: 0, names: [], script: [[], click(B0)] },
    // 9 (resume 1, 2 names): load with no selection -> returns 0 with no load; a right-button press on load
    { resume: 1, names: ['a.sve', 'b.sve'], script: [[at(B0), PRESS2(B0[0], B0[1])]] },
    // 10: a non-Esc key reaches the list click at the last press position (0x0043c5ce -> 0x0043c7ab); a press away from
    //     every button with no list hit; Esc
    { resume: 1, names: NAMES5, script: [pick(4), [], [KEY(0x0D)], [], [CUR(0x100, 0x300)], [PRESS(0x100, 0x300)], [],
                                         [KEY(0x1B)]] },
    // 11: tips: dwell on load with no selection (empty text), on back, on scroll-up (no text) past 0x1f frames
    { resume: 1, names: NAMES5,
      script: [[at(B0)]].concat(IDLE(33), [[at(B1)]], IDLE(33), [[at(B2)]], IDLE(33), [[KEY(0x1B)]]) },
    // 12: tips with a selection: load (text), delete, scroll-down; Esc
    { resume: 1, names: NAMES5,
      script: [pick(0), [], [at(B0)]].concat(IDLE(32), [[at(B4)]], IDLE(33), [[at(B3)]], IDLE(32), [[KEY(0x1B)]]) },
    // 13: hover edges: each button's distance threshold from both sides (approxDistance = (min + 2 max) / 2), cursor
    //     moves between buttons (old one redrawn), Esc
    { resume: 1, names: NAMES5,
      script: [[CUR(B0[0] + 0x1A, B0[1])], [CUR(B0[0] + 0x19, B0[1])], [CUR(B1[0], B1[1] - 0x18)], [CUR(B1[0], B1[1] - 0x17)],
               [CUR(B2[0] - 0x14, B2[1])], [CUR(B2[0] - 0x13, B2[1])], [CUR(B3[0], B3[1] + 0x14)], [CUR(B3[0], B3[1] + 0x13)],
               [CUR(B4[0] - 0x17, B4[1])], [CUR(B4[0] - 0x16, B4[1])], [at(B0)], [at(B4)], [CUR(0x40, 0x40)],
               [KEY(0x1B)]] },
    // 14 (championship, resume 1): Esc -> back with no snapshot reload; flags and type restored
    { resume: 1, champ: 1, names: ['links.cse'], type: 0x81, flags: 0x7fff0001, script: [[KEY(0x1B)]] },
    // 15 (resume 0): pick, load: the theme unchanged -> no loadThemes; flags restored before the load
    { resume: 0, names: NAMES5, type: 4, flags: 0, script: [pick(4), [], click(B0)] },
    // 16 (championship, resume 0): load with no selection -> the out-of-range exit with the snapshot reload skipped
    //    (0x0043ca9e taken), returns 0
    { resume: 0, champ: 1, names: ['links.cse', 'dunes.cse'], script: [[], click(B0)] },
    // 17 (championship, resume 0): Esc -> back, the snapshot reload skipped by the championship test (0x0043cac9)
    { resume: 0, champ: 1, names: ['links.cse'], type: 0x7f, flags: -1, script: [[KEY(0x1B)]] },
    // 18 (18 names, resume 1): a click on row 17 stores sel = scroll + 17 = 19 before the bound test (0x0043c7e0) and
    //    takes the scroll path; the stale selection (>= n) then draws details and makes load take the out-of-range exit
    //    (0x0043c90f taken), returns 0
    { resume: 1, names: many(18), script: [click(B3), [], pick(17), [], click(B0)] },
  ];

  function key(o) {
    return function () { return rows(scene(o), o.rows || ROWS); };
  }
  F.c3fw_lgb = key({});
  F.c3fw_lgb_raw = key({ raw: true });       // control: the frame mask off (a correct body reads RED)

  // ====================================================================================================================
  // showThemePackSelect 0x004725b0
  // ====================================================================================================================
  // listThemes 0x004724c0 is stood in: it writes the row's theme names into the table 0x0056e928 (0x32 bytes each) and
  // returns their count. FindFirstFileA (the import the body calls through the IAT slot 0x004ba090) is stood in at the
  // routine the slot points to, TID-gated like every framemask stub (game threads reach the real one): it logs the
  // probe path (its text and its frame token), answers row.found(n) for its n-th call (a fake handle or
  // INVALID_HANDLE_VALUE; the body never closes them) and writes row.col5 into the word 0x26 bytes below the path: in
  // the original that is [esp+0x32], the sixth column word the body reads for column 5 (0x00472e59) and never writes,
  // so both arms read the same value there. Nothing touches the disk.
  const THEMES = 0x0056E928, THEME_STRIDE = 0x32, THEME_MAX = 20;
  const TP_SAVES = [[BUF, 0x400], [THEME, 0x40], [THEMES, THEME_STRIDE * THEME_MAX]];
  const TP = { row: null };
  function tpStands() {
    const findFirst = ptr(0x004BA090).readPointer().toUInt32();
    return [
      { addr: 0x004724C0, abi: 'mscdecl', nargs: 0, ret: function () { return TP.row.themes.length; },
        write: function () {                                                             // listThemes
          for (let i = 0; i < THEME_MAX; i++)
            writeStr(THEMES + THEME_STRIDE * i, i < TP.row.themes.length ? TP.row.themes[i] : '', THEME_STRIDE);
        } },
      { addr: findFirst, abi: 'stdcall', nargs: 2, str: { 0: 12 },
        ret: function (self, a, n) { return TP.row.found(n) ? 0x7E000000 + n : 0xFFFFFFFF; },
        write: function (self, a) { ptr(a[0]).sub(0x26).writeS16(TP.row.col5 | 0); } },
      { addr: 0x004A55D2, abi: 'stdcall', nargs: 5, region: { arg: 0, strideArg: 1, countArg: 2 }, forward: true },
      { addr: 0x004A5713, abi: 'stdcall', nargs: 4, deref: { 0: 3 }, retire: 0, forward: true },
      { addr: 0x00474AE0, abi: 'thiscall', nargs: 0, region: { arg: 'this', size: 0x2B8 }, fill: { words: 4 }, ret: 'this' },
      { addr: 0x00474C40, abi: 'thiscall', nargs: 0, deref: { this: 4 }, retire: 'this', ret: 0xC3F02 },
      { addr: 0x00475840, abi: 'thiscall', nargs: 5, str: { 0: 8 }, ret: 0 },             // loadImage
      { addr: 0x00473BF0, abi: 'thiscall', nargs: 7, deref: { this: 3 }, ret: 0 },        // Surface_blit
      { addr: 0x004762D0, abi: 'thiscall', nargs: 4, ret: 0 },                            // setDrawTarget
      { addr: 0x00475C60, abi: 'thiscall', nargs: 7, ret: 0 },                            // Surface_blitThunk
      { addr: 0x00473E60, abi: 'thiscall', nargs: 4, ret: 0 },                            // Image_draw
      { addr: 0x00473CB0, abi: 'thiscall', nargs: 7, ret: 0 },                            // Surface_drawScaled
      { addr: 0x00404B70, abi: 'mscdecl', nargs: 4, str: { 0: 8 }, ret: 0 },              // drawCenteredText
      { addr: 0x004049D0, abi: 'mscdecl', nargs: 4, str: { 0: 8 }, ret: 0 },              // drawText4049d0
      { addr: 0x00404BC0, abi: 'mscdecl', nargs: 4, str: { 0: 8 }, ret: 0 },              // drawCenteredText2
      { addr: 0x00480C80, abi: 'thiscall', nargs: 1, ret: 0 },                            // Window::refreshFull
      { addr: 0x0040CA10, abi: 'mscdecl', nargs: 5, ret: 0 },                             // blitPanel
    ];
  }
  // probe answers: bit s of mask[theme] = suffix s found (probe n is theme n / 6, suffix n % 6)
  const masks = function (m) { return function (n) { const t = Math.floor(n / 6); return t < m.length && ((m[t] >>> (n % 6)) & 1) === 1; }; };
  // buttons (approxDistance centres 0x00472f10..0x00472fa7) and theme rows (0x00473143: (y - 0x5b) / 32)
  const T0 = [0x267, 0x22B], T1 = [0x2ED, 0x22C], T2 = [0x30A, 0x63], T3 = [0x30A, 0x1E7];
  const TROW = function (r) { return 0x5B + 32 * r + 9; };
  const tpick = function (r) { return [CUR(0x100, TROW(r)), PRESS(0x100, TROW(r))]; };
  const THEMES3 = ['Standard', 'Celebrity', 'Holiday'];
  const TP_ROWS = [
    // 0: the current theme is row 1; every suffix of theme 0 found but the fifth, theme 1 all six (column 5 drawn from
    //    the seeded word), theme 2 none; Esc -> back: the default theme name copied (0x00473197)
    { themes: THEMES3, cur: 'Celebrity', col5: 0x1F0, found: masks([0x2F, 0x3F, 0]), script: [[], [KEY(0x1B)]] },
    // 1: hover load past 0x1e frames (tip with the load text), then press it -> 0x0047320f
    { themes: THEMES3, cur: 'Holiday', col5: 0x150, found: masks([0x01, 0x12, 0x24]),
      script: [[at(T0)]].concat(IDLE(33), [[PRESS(T0[0], T0[1])]]) },
    // 2: no current theme: tips on load (empty), back, both scroll buttons; a press on button 2 and on 3 (redraw); a
    //    theme row press selects it (redraw); a press above the list (idx < 0) leaves through 0x0047325f
    { themes: THEMES3, cur: 'Nothing', col5: -7, found: masks([0x3F, 0x00, 0x15]),
      script: [[at(T0)]].concat(IDLE(32), [[at(T1)]], IDLE(32), [[at(T2)]], IDLE(32), [[at(T3)]], IDLE(32),
                                [click(T2)], [], [click(T3)], [], tpick(2), [], [[CUR(0x100, 0x20), PRESS(0x100, 0x20)]]) },
    // 3: a press below the last theme (idx >= count) -> 0x0047325f
    { themes: THEMES3, cur: 'Standard', col5: 0x77, found: masks([0x3F, 0x3F, 0x3F]), script: [tpick(5)] },
    // 4: no themes: no probes, no rows; Esc
    { themes: [], cur: 'Standard', col5: 0, found: masks([]), script: [[KEY(0x0D)]] },
    // 5: a press just above the first row ((y - 0x5b) / 32 rounds to 0: theme 0 selected), then a right-button press on
    //    load -> 0x0047320f
    { themes: THEMES3, cur: 'Holiday', col5: 0x2A0, found: masks([0x20, 0x20, 0x20]),
      script: [[CUR(0x100, 0x50), PRESS(0x100, 0x50)], [], [at(T0), PRESS2(T0[0], T0[1])]] },
    // 6: twelve themes with assorted probe masks, the current one in the middle; hover moves between all four buttons
    //    (each move redraws the old one normal and the new one highlighted); then back pressed
    { themes: ['t00', 't01', 't02', 't03', 't04', 't05', 't06', 't07', 't08', 't09', 't10', 't11'], cur: 't06', col5: 0x1234,
      found: masks([0x3F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x3E, 0x2A, 0x15, 0x30, 0x0F]),
      script: [[at(T0)], [at(T1)], [at(T2)], [at(T3)], [at(T0)], [CUR(0x40, 0x40)], [], click(T1)] },
    // 7: hover edges: each button's threshold from both sides; a key exits as back
    { themes: THEMES3, cur: 'Celebrity', col5: 0x10, found: masks([0x01, 0x02, 0x04]),
      script: [[CUR(T0[0] + 0x1A, T0[1])], [CUR(T0[0] + 0x19, T0[1])], [CUR(T1[0], T1[1] - 0x18)], [CUR(T1[0], T1[1] - 0x17)],
               [CUR(T2[0] - 0x14, T2[1])], [CUR(T2[0] - 0x13, T2[1])], [CUR(T3[0], T3[1] + 0x14)], [CUR(T3[0], T3[1] + 0x13)],
               [CUR(0x40, 0x40)], [KEY(0x41)]] },
    // 8: presses where the row arithmetic rounds: y - 0x5b = -31 (row 0), 62 (row 1, the last pixels of it), 95 (row 2);
    //    then -32 (row -1: leaves through 0x0047325f)
    { themes: THEMES3, cur: 'Standard', col5: 0x99, found: masks([0x10, 0x08, 0x04]),
      script: [[CUR(0x100, 0x5B - 31), PRESS(0x100, 0x5B - 31)], [], [CUR(0x100, 0x5B + 62), PRESS(0x100, 0x5B + 62)], [],
               [CUR(0x100, 0x5B + 95), PRESS(0x100, 0x5B + 95)], [], [CUR(0x100, 0x5B - 32), PRESS(0x100, 0x5B - 32)]] },
  ];
  function tpRows(fx, list) {
    list.forEach(function (row, i) {
      Object.defineProperty(fx, 'r' + i, {
        enumerable: true, configurable: true,
        get: function () {
          fx.M.select(row.script, row.cursor || [0x40, 0x40]);
          TP.row = row;
          writeStr(THEME, row.cur, 16);
          for (let k = 0; k < THEME_MAX; k++) writeStr(THEMES + THEME_STRIDE * k, '', THEME_STRIDE);
          writeStr(BUF, '', 0x40);
          return 0;
        },
      });
    });
    return fx;
  }
  function tpKey(o) {
    return function () {
      return tpRows(scene(Object.assign({ stands: tpStands, saves: TP_SAVES }, o)), o.rows || TP_ROWS);
    };
  }
  F.c3fw_tp = tpKey({});
  F.c3fw_tp_raw = tpKey({ raw: true });      // control: the frame mask off

  // ====================================================================================================================
  // pickAProScreen 0x0043a8c0
  // ====================================================================================================================
  // The browser's recipe. listMatchingFiles serves the row's directory; loadThemeFile 0x00437fa0 (the preview of a
  // pressed row and the final load) is a recorder that writes the pro's card from the name's byte sum h: the portrait
  // selectors 0x004d60a9 (bit 7) / 0x004d60aa, the name at 0x004d6098 and the ten skill bytes at 0x005a5a04;
  // buildGolferComment 0x00469b00 appends a comment made from its call index to 0x0051a068; showPopup answers
  // row.popup[n]; _remove only logs its path.
  const PRO_NAME = 0x004D6098, PRO_SKILLS = 0x005A5A04;
  const PP_SAVES = [[NAMES, 100 * NAME_MAX], [BUF, 0x400], [0x005A9F6C, 0x40], [THEME, 0x40], [PRO_NAME, 0x20],
                    [PRO_SKILLS, 0x10], [0x0057956C, 4]];
  const PP = { row: null };
  function ppCard(nameAddr) {
    const h = byteSum(nameAddr);
    writeStr(PRO_NAME, 'Pro' + readStr(nameAddr, 6), 0x11);
    ptr(0x004D60A9).writeU8(h & 1 ? 0x80 | (h & 0x7f) : h & 0x7f);
    ptr(0x004D60AA).writeS8((h % 7) - 2);
    for (let i = 0; i < 10; i++) ptr(PRO_SKILLS + i).writeS8(((h * (i + 3)) % 23) - 6);
  }
  function ppStands(pl) {
    return [
      { addr: 0x004A55D2, abi: 'stdcall', nargs: 5, region: { arg: 0, strideArg: 1, countArg: 2 }, forward: true },
      { addr: 0x004A5713, abi: 'stdcall', nargs: 4, deref: { 0: 3 }, retire: 0, forward: true },
      { addr: 0x00474AE0, abi: 'thiscall', nargs: 0, region: { arg: 'this', size: 0x2B8 }, fill: { words: 4 }, ret: 'this' },
      { addr: 0x00474C40, abi: 'thiscall', nargs: 0, deref: { this: 4 }, retire: 'this', ret: 0xC3F03 },
      { addr: 0x00475840, abi: 'thiscall', nargs: 5, str: { 0: 8 }, ret: 0 },             // loadImage
      { addr: 0x00473BF0, abi: 'thiscall', nargs: 7, deref: { this: 3 }, ret: 0 },        // Surface_blit
      { addr: 0x004762D0, abi: 'thiscall', nargs: 4, ret: 0 },                            // setDrawTarget
      { addr: 0x00475C60, abi: 'thiscall', nargs: 7, ret: 0 },                            // Surface_blitThunk
      { addr: 0x00473E60, abi: 'thiscall', nargs: 4, ret: 0 },                            // Image_draw
      { addr: 0x00473CB0, abi: 'thiscall', nargs: 7, ret: 0 },                            // Surface_drawScaled
      { addr: 0x00404B70, abi: 'mscdecl', nargs: 4, str: { 0: 8 }, ret: 0 },              // drawCenteredText
      { addr: 0x004049D0, abi: 'mscdecl', nargs: 4, str: { 0: 8 }, ret: 0 },              // drawText4049d0
      { addr: 0x00404BC0, abi: 'mscdecl', nargs: 4, str: { 0: 8 }, ret: 0 },              // drawCenteredText2
      { addr: 0x00404AD0, abi: 'mscdecl', nargs: 4, str: { 0: 12 }, ret: 0 },             // drawTextSurface
      { addr: 0x00480B00, abi: 'thiscall', nargs: 5, ret: 0 },                            // fillWindowRect
      { addr: 0x00480C80, abi: 'thiscall', nargs: 1, ret: 0 },                            // Window::refreshFull
      { addr: 0x0043D2A0, abi: 'mscdecl', nargs: 3, str: { 0: 8, 2: 6 }, ret: function () { return PP.row.names.length; },
        write: function () {                                                             // listMatchingFiles
          for (let i = 0; i < NAME_MAX; i++) writeStr(NAMES + 100 * i, i < PP.row.names.length ? PP.row.names[i] : '', 100);
        } },
      { addr: 0x00437FA0, abi: 'mscdecl', nargs: 3, str: { 0: 8 }, ret: 0x7FA0,           // loadThemeFile
        write: function (self, a) { ppCard(a[0]); } },
      { addr: 0x00469B00, abi: 'mscdecl', nargs: 4, ret: 0,                               // buildGolferComment
        write: function (self, a, n) { const t = readStr(BUF, 0x30); writeStr(BUF, t + 'cmt' + n, 0x40); } },
      { addr: 0x0046D6E0, abi: 'mscdecl', nargs: 5,                                       // showPopup (text in 0x0051a068)
        ret: function (self, a, n) { const q = PP.row.popup || [1]; return q[n % q.length]; },
        write: function () { logBuf(pl, 0x0D6E0000); } },
      { addr: 0x004A64B8, abi: 'mscdecl', nargs: 1, str: { 0: 16 }, ret: 0 },             // _remove: logged only
    ];
  }
  const PROS5 = ['ace.pro', 'birdie.pro', 'chip.pro', 'divot.pro', 'eagle.pro'];
  const PP_ROWS = [
    // 0: Esc at once -> back: the theme name stays the one set at entry (0x0043b59c)
    { names: PROS5, script: [[], [KEY(0x1B)]] },
    // 1: press row 2 (preview: the card), hover load, press it -> loadThemeFile(name, 0, 1), returns 1
    { names: PROS5, script: [pick(2), [], [at(B0)], [], click(B0)] },
    // 2: load with no selection -> 0x0043b542, returns 0 with the saved theme restored
    { names: PROS5, script: [[], click(B0)] },
    // 3: pick, delete -> no, delete -> yes (_remove from the championship directory), Esc
    { names: PROS5, popup: [1, 0], script: [pick(3), [], click(B4), [], click(B4), [], [KEY(0x1B)]] },
    // 4 (40 names): scroll down twice and up once; a press on row 17 selects an unseen row (no row bound,
    //    0x0043b3e5) and previews it; scroll down three more times (the limit), Esc
    { names: many(40), script: [click(B3), [], click(B3), [], click(B2), [], pick(17), [], click(B3), [], click(B3), [],
                                click(B3), [], click(B3), [], click(B3), [], [KEY(0x1B)]] },
    // 5: a press past the list (deselect), left of it (x <= 0x140), above it (y < 0x74); pick; Esc
    { names: ['one.pro', 'two.pro'], script: [pick(7), [], [PRESS(0x100, ROWY(1))], [], [PRESS(LX, 0x50)], [], pick(1), [],
                                              [KEY(0x1B)]] },
    // 6: tips with no selection on load (empty), back, scroll up (no text), delete; Esc
    { names: PROS5, script: [[at(B0)]].concat(IDLE(32), [[at(B1)]], IDLE(32), [[at(B2)]], IDLE(32), [[at(B4)]], IDLE(32),
                                                [[KEY(0x1B)]]) },
    // 7: a pick, the load tip, then a non-Esc key while load is hot -> load (0x0043b248), returns 1
    { names: PROS5, script: [pick(0), [], [at(B0)]].concat(IDLE(32), [[KEY(0x0D)]]) },
    // 8: a pick, then a non-Esc key with no hot button -> the press at the last press position (0x0043b3b1); hover
    //    threshold edges; Esc
    { names: PROS5, script: [pick(4), [], [CUR(0x40, 0x40)], [KEY(0x0D)], [], [CUR(B0[0] + 0x1A, B0[1])], [CUR(B0[0] + 0x19, B0[1])],
                             [CUR(B1[0], B1[1] - 0x18)], [CUR(B1[0], B1[1] - 0x17)], [CUR(B2[0] - 0x14, B2[1])],
                             [CUR(B2[0] - 0x13, B2[1])], [CUR(B3[0], B3[1] + 0x14)], [CUR(B3[0], B3[1] + 0x13)],
                             [CUR(B4[0] - 0x17, B4[1])], [CUR(B4[0] - 0x16, B4[1])], [CUR(0x40, 0x40)], [KEY(0x1B)]] },
    // 9 (empty directory): delete with no selection, answered yes (name slot -1); a right-button press on back
    { names: [], popup: [0], script: [click(B4), [], [at(B1), PRESS2(B1[0], B1[1])]] },
  ];
  function ppRows(fx, list) {
    list.forEach(function (row, i) {
      Object.defineProperty(fx, 'r' + i, {
        enumerable: true, configurable: true,
        get: function () {
          fx.M.select(row.script, row.cursor || [0x40, 0x40]);
          PP.row = row;
          writeStr(THEME, row.theme || 'MyTheme', 16);
          for (let k = 0; k < NAME_MAX; k++) writeStr(NAMES + 100 * k, '', 100);
          writeStr(BUF, '', 0x40);
          writeStr(PRO_NAME, 'Nobody', 0x11);
          ptr(0x004D60A9).writeU8(0); ptr(0x004D60AA).writeU8(0);
          return 0;
        },
      });
    });
    return fx;
  }
  function ppKey(o) {
    return function () {
      return ppRows(scene(Object.assign({ stands: ppStands, saves: PP_SAVES }, o)), o.rows || PP_ROWS);
    };
  }
  F.c3fw_pp = ppKey({});
  F.c3fw_pp_raw = ppKey({ raw: true });      // control: the frame mask off

  // ====================================================================================================================
  // courseActionMenu 0x0046f550
  // ====================================================================================================================
  // The 16 course records (0x00571fd8, 0x2e each: name +0, site +0x1c, holes +0x1d, sound kind +0x20, years +0x22 /
  // +0x23) are written per row: the site bytes are a permutation of 0..15, so the body's site table (one int per site,
  // filled from the records at 0x0046f733) is fully written in both arms. Three frame slots the original reads before
  // any write ([F+0x24] blink, [F+0x28] dwell, [F+0x30] previous hot button) are seeded by the ??_L stub at the array
  // pointer - 0x50 / - 0x4c / - 0x44 (the array is [F+0x74]); the reimplementation keeps them at the same distance
  // (struct CamFrame). Recorders: every drawing callee, Cache482fd0 ctor / Cache483010 dtor, seedCourseTable,
  // textInputBox (writes row.save into 0x0051a068), saveGame, startMessage / drawWindow / drawPanelBackdrop (their text
  // from 0x0051a068 copied into $pathlog), loadGameBrowserScreen (answers row.lgb[n]), playSound, Random::range
  // (answers n-th call % its argument). Real: tierPrice, appendNumber, appendCourseTitle, appendDate, _itoa,
  // View_toLocal, approxDistance, enterModal483d30; the pumps (waitTicks, pumpMessages, pumpAndRedraw) are modal.js.
  const RECS = 0x00571FD8, DATE = 0x00834170, FLAGS = 0x0059E7B8, CHEAT = 0x005A6364, OWNED = 0x00543CFC;   // CASH (0x00571fd4) is the loadGameBrowserScreen section's
  const CAM_SAVES = [[CASH, 0x300], [BUF, 0x400], [0x008392AC, 0x40], [DATE, 4], [FLAGS, 4], [CHEAT, 4], [OWNED, 4],
                     [0x004C1E90, 0x82 * 16]];   // the site definitions (row.patch writes one)
  const CAM = { row: null };
  function camStands(pl) {
    return [
      { addr: 0x004A55D2, abi: 'stdcall', nargs: 5, region: { arg: 0, strideArg: 1, countArg: 2 }, forward: true,
        write: function (self, a) {                                                     // seed [F+0x24] / 0x28 / 0x30
          const base = ptr(a[0]);
          base.sub(0x50).writeS32(CAM.row.blink | 0);
          base.sub(0x4C).writeS32(CAM.row.dwell | 0);
          base.sub(0x44).writeS32(CAM.row.prev === undefined ? -1 : CAM.row.prev);
        } },
      { addr: 0x004A5713, abi: 'stdcall', nargs: 4, deref: { 0: 3 }, retire: 0, forward: true },
      { addr: 0x00482FD0, abi: 'thiscall', nargs: 0, region: { arg: 'this', size: 0x58 }, fill: { words: 4 }, ret: 'this' },
      { addr: 0x00483010, abi: 'thiscall', nargs: 0, deref: { this: 4 }, retire: 'this', ret: 0x3010 },
      { addr: 0x00474AE0, abi: 'thiscall', nargs: 0, region: { arg: 'this', size: 0x2B8 }, fill: { words: 4 }, ret: 'this' },
      { addr: 0x00474C40, abi: 'thiscall', nargs: 0, deref: { this: 4 }, retire: 'this', ret: 0xC3F04 },
      { addr: 0x00475840, abi: 'thiscall', nargs: 5, str: { 0: 8 }, ret: 0 },             // loadImage
      { addr: 0x00473BF0, abi: 'thiscall', nargs: 7, deref: { this: 3 }, ret: 0 },        // Surface_blit
      { addr: 0x00475C90, abi: 'thiscall', nargs: 5, ret: 0 },                            // Surface_fillRegion
      { addr: 0x00473E60, abi: 'thiscall', nargs: 4, ret: 0 },                            // Image_draw
      { addr: 0x004762D0, abi: 'thiscall', nargs: 4, ret: 0 },                            // setDrawTarget
      { addr: 0x00404B70, abi: 'mscdecl', nargs: 4, str: { 0: 10 }, ret: 0 },             // drawCenteredText
      { addr: 0x004049D0, abi: 'mscdecl', nargs: 4, str: { 0: 6 }, ret: 0 },              // drawText4049d0
      { addr: 0x00404BC0, abi: 'mscdecl', nargs: 4, str: { 0: 10 }, ret: 0 },             // drawCenteredText2
      { addr: 0x00473F60, abi: 'thiscall', nargs: 5, ret: 0 },                            // Surface_stretchBlit
      { addr: 0x00480C80, abi: 'thiscall', nargs: 1, ret: 0 },                            // Window::refreshFull
      { addr: 0x0040CC00, abi: 'mscdecl', nargs: 5, ret: 0 },                             // drawPanelS2
      { addr: 0x0040CEF0, abi: 'mscdecl', nargs: 5, ret: 0 },                             // drawFrameR3
      { addr: 0x0040D320, abi: 'mscdecl', nargs: 4, ret: 0, write: function () { logBuf(pl, 0x0D3200); } },   // drawWindow
      { addr: 0x0040CB00, abi: 'mscdecl', nargs: 3, ret: 0, write: function () { logBuf(pl, 0x0CB000); } },   // startMessage
      { addr: 0x00432620, abi: 'mscdecl', nargs: 2, ret: 0, write: function () { logBuf(pl, 0x326200); } },   // drawPanelBackdrop
      { addr: 0x0045B2C0, abi: 'mscdecl', nargs: 4, str: { 0: 5 }, ret: 0,                // textInputBox
        write: function (self, a, n) { logBuf(pl, 0x5B2C00); const q = CAM.row.save || ['']; writeStr(BUF, q[n % q.length], 0x20); } },
      { addr: 0x0040B4A0, abi: 'mscdecl', nargs: 1, str: { 0: 6 }, ret: 0 },              // saveGame
      { addr: 0x0043B610, abi: 'mscdecl', nargs: 2, ret: function (self, a, n) { const q = CAM.row.lgb || [0]; return q[n % q.length]; } },
      { addr: 0x0046F2B0, abi: 'mscdecl', nargs: 0, ret: 0 },                             // seedCourseTable
      { addr: 0x004481B0, abi: 'mscdecl', nargs: 5, ret: 0 },                             // playSound
      { addr: 0x0045C1E0, abi: 'thiscall', nargs: 1, ret: function (self, a, n) { return (n + 2) % (a[0] | 1); } },   // Random::range
    ];
  }
  // records: perm[c] = site of record c; kind[c] (+0x20), owned[c] (+0x22, 0xff = not bought), y7[c] (+0x23)
  function camRecords(row) {
    for (let c = 0; c < 16; c++) {
      const r = RECS + 0x2E * c;
      for (let q = 0; q < 0x2E; q++) ptr(r).add(q).writeU8(0);
      writeStr(r, 'Course' + c, 0x1C);
      ptr(r + 0x1C).writeS8(row.perm[c]);
      ptr(r + 0x1D).writeU8(9 + (c % 10));
      ptr(r + 0x20).writeS8(row.kind ? row.kind[c] : c % 4);
      ptr(r + 0x22).writeU8(row.owned ? row.owned[c] : 0xFF);
      ptr(r + 0x23).writeU8(row.y7 ? row.y7[c] : 0xFF);
    }
  }
  const ID = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];
  const REV = [15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0];
  const SITE = [[251, 13], [302, 64], [346, 115], [384, 166], [416, 220], [437, 277], [454, 337], [465, 398], [469, 458],
                [463, 520], [466, 13], [509, 64], [546, 115], [580, 166], [606, 220], [623, 277]];   // 0x004e3dc0 (x, y)
  const onSite = function (k) { return CUR(SITE[k][0] + 0x19, SITE[k][1] + 0x19); };
  const BACK = [0x300, 0x22D], SAVE = [0x2F8, 0x1DC], LOAD = [0x306, 0x1A9];
  const OWN3 = [0xFF, 3, 0xFF, 0xFF, 0x7F, 0xFF, 1, 0xFF, 0xFF, 0xFF, 2, 0xFF, 0xFF, 0x80, 0xFF, 0xFF];
  const Y7 = [0xFF, 5, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x81, 0xFF, 0xFF, 3, 0xFF, 0xFF];
  const CAM_ROWS = [
    // 0 (no game yet: date 0): the back button at once -> -1
    { date: 0, cash: 100, perm: ID, blink: 0, dwell: 0, prev: -1, script: [[at(BACK)], [PRESS(BACK[0], BACK[1])]] },
    // 1 (a game running, a negative date: 0x0047051e): hover save past 0x14 frames (its tip), press it: textInputBox gives a name -> saveGame and
    //    the message; then the reset / save spot again with an empty name; back
    { date: -0x1235, cash: 5000, perm: REV, blink: 7, dwell: 3, prev: 3, owned: OWN3, y7: Y7, save: ['mysave', ''],
      script: [[at(SAVE)]].concat(IDLE(23), [[PRESS(SAVE[0], SAVE[1])], [], [PRESS(SAVE[0], SAVE[1])], [], [at(BACK)],
                                  [PRESS(BACK[0], BACK[1])]]) },
    // 2: load: the browser answers 0 (continue), then 1 -> -1
    { date: 0x1000, cash: 50, perm: ID, blink: -9, dwell: 0x30, prev: 2, lgb: [0, 1],
      script: [[at(LOAD)], [PRESS(LOAD[0], LOAD[1])], [], [PRESS(LOAD[0], LOAD[1])]] },
    // 3 (date 0): the reset spot is button 0 -> seedCourseTable; then buy site 5 (affordable, kind 1: a random sound)
    { date: 0, cash: 100000, perm: ID, blink: 0x10, dwell: 0, prev: -1,
      script: [[at(SAVE)], [PRESS(SAVE[0], SAVE[1])], [onSite(5)], [], [PRESS(SITE[5][0] + 0x19, SITE[5][1] + 0x19)]] },
    // 4: site 9 too dear -> the warning window and pumpAndRedraw; then the cheat word 0x11 buys it anyway (kind 2)
    { date: 0x400, cash: 10, perm: REV, blink: 5, dwell: 0, prev: -1,
      script: [[onSite(9)], [], [PRESS(SITE[9][0] + 0x19, SITE[9][1] + 0x19)], [PRESS(0x20, 0x20)],   // the press ends pumpAndRedraw(0)
               [{ k: 'raw', set: { 0x005A6364: 0x11 } }], [PRESS(SITE[9][0] + 0x19, SITE[9][1] + 0x19)]] },
    // 5: an owned site ignored, a right press on a site ignored, a press far from every site ignored; buy site 11
    //    (record 11: kind 3); the unlimited-money flag set (both its text paths)
    { date: 0x7FF, cash: 99999, perm: ID, blink: 8, dwell: 0, prev: -1, owned: OWN3, y7: Y7, flags: 0x1000000,
      script: [[onSite(1)], [], [PRESS(SITE[1][0] + 0x19, SITE[1][1] + 0x19)], [], [PRESS2(SITE[1][0] + 0x19, SITE[1][1] + 0x19)],
               [CUR(0x20, 0x200)], [], [PRESS(0x20, 0x200)], [onSite(11)], [], [PRESS(SITE[11][0] + 0x19, SITE[11][1] + 0x19)]] },
    // 6: the pointer over every site in turn (each one's details: owned / unowned, blinking with the seeded counter),
    //    then over the definitions' own x / y, then back (dwell seeded so the back tip shows on the first frames)
    { date: 0x5555, cash: 4000, perm: REV, blink: 6, dwell: 0x13, prev: 3, owned: OWN3, y7: Y7,
      // (and 0x1e left of site 0's spot: a distance of exactly 0x1e is not nearer than the start value 0x1e, 0x0047041f)
      script: SITE.map(function (s, k) { return [onSite(k)]; }).concat([[CUR(SITE[0][0] + 0x19 - 0x1E, SITE[0][1] + 0x19)], [],
                                                                         [CUR(0x100, 0x100)], [at(BACK)], [], [], [PRESS(BACK[0], BACK[1])]]) },
    // 7: buy a site whose record has sound kind 7 (no sound), date 0, the site table reversed
    { date: 0, cash: 100000, perm: REV, blink: 0, dwell: 0, prev: 0, kind: [7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7],
      script: [[onSite(2)], [], [PRESS(SITE[2][0] + 0x19, SITE[2][1] + 0x19)]] },
    // 8: buy with sound kind 0 (the fixed sound 0x33)
    { date: 0x10, cash: 100000, perm: ID, blink: 1, dwell: 0, prev: -1, kind: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
      script: [[onSite(15)], [], [PRESS(SITE[15][0] + 0x19, SITE[15][1] + 0x19)]] },
    // 9 (date 0): the reset spot's tip (button 0 held past 0x14 frames); site 12's definition y patched to 0x140, so its
    //    plate takes the fourth placement (x >= 0xc8 and y >= 0x12c, 0x004700d6), which no stock site reaches; back
    { date: 0, cash: 300, perm: ID, blink: 3, dwell: 0, prev: -1, patch: [[0x004C1E90 + 0x82 * 12 + 0x3C, 0x140]],
      script: [[at(SAVE)]].concat(IDLE(24), [[at(BACK)], [PRESS(BACK[0], BACK[1])]]) },
  ];
  function camRows(fx, list) {
    list.forEach(function (row, i) {
      Object.defineProperty(fx, 'r' + i, {
        enumerable: true, configurable: true,
        get: function () {
          fx.M.select(row.script, row.cursor || [0x40, 0x40]);
          CAM.row = row;
          camRecords(row);
          (row.patch || []).forEach(function (q) { ptr(q[0]).writeS16(q[1]); });
          w32(CASH, row.cash); w32(DATE, row.date); w32(FLAGS, row.flags || 0); w32(CHEAT, 0); w32(OWNED, 0x10);
          writeStr(BUF, '', 0x40);
          return 0;
        },
      });
    });
    return fx;
  }
  function camKey(o) {
    return function () {
      return camRows(scene(Object.assign({ stands: camStands, saves: CAM_SAVES, maxCalls: 3000, maxFrames: 1000 }, o)),
                     o.rows || CAM_ROWS);
    };
  }
  F.c3fw_cam = camKey({});
  F.c3fw_cam_raw = camKey({ raw: true });    // control: the frame mask off
})();
