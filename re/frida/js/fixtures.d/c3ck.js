// Fixtures for batch c3ck (jgld.dll): libpng 1.0.5's chunk readers, the Palette methods and windowProc.
// Reimplementations: shim/src/re/c3ck.cpp. Registry: re/frida/registry.d/c3ck.py.
// Chunk bytes and their CRCs: log/c3/c3ck_gen.py (pasted below as C3CK_DATA).
//
// jgld.dll is relocated at run time (log/c3/alt/c3ck_probe.py read base 0x038f0000), so every module
// address below is built from Process.getModuleByName('jgld.dll').base.
//
// WHAT MAKES THE SEVEN CHUNK READERS A/B-ABLE. libpng reads bytes only through the png_struct's
// read_data_fn at +0x50 and reports warnings only through the handler at +0x44, and both the png_struct
// and the png_info belong to the CALLER. Every png_struct here is ours and carries c3ch's 35-byte
// __cdecl reader (it copies `length` bytes out of a buffer of ours with `rep movsb`, advances a cursor
// and counts the call) plus c3bq's one-byte `ret` in the warning and error slots. The cursor and the
// counter are compared state, so the sequence of lengths each arm asked for is evidence, and the four
// bytes that close every chunk are the real crc32 of exactly the bytes the reader will have served, so
// both sides of png_crc_error 0x10079620's comparison are reached by design and not by luck.
//
// WHAT MAKES THE PALETTE METHODS A/B-ABLE. They write only the palette object the caller owns. Each
// block here is ours and carries jgld's own Palette vtable VA 0x1011db10 at +0, so the virtual call
// Palette::setDefault makes (slot 2) reaches the real Palette::release 0x1006a380. The one exception is
// Palette::getColor 0x1006add0, which writes the three module bytes at VA 0x10128734: the running game
// enters it (log/c3/alt/c3ck_probe.txt: 30 entries in 20 s, all on the process's first thread), so that
// key suspends that thread for its vectors.
//
// WHAT MAKES windowProc A/B-ABLE. Two module globals decide what it does: the event sink pointer VA
// 0x1012872c (message 0x401) and the command handler VA 0x10128728 (message 0x4a through onCommand).
// Both are swapped for recording objects of ours with the first thread suspended and written back when
// the key ends (c3cd's pattern, and c3cd's probe measured that the first thread is the only one that
// reads them). The display pointer VA 0x10128420 is NOT swapped: the game's render thread reads it
// thousands of times a second, so instead every key-down vector carries a virtual key inside onKey
// 0x10069c70's 0x60..0x6f window (0x10069cb0) and every key-up vector has down = 0 (0x10069caa), which
// are the two ways past the `while (display->pumpOne())` loop at 0x10069cbc. The window handle is the
// invalid 0x1, so DefWindowProcA, BeginPaint, InvalidateRect, GetDC and PeekMessageA all fail without
// touching the game's window; the probe called DefWindowProcA(1, msg, ...) twice per message and got 0
// both times for every message a vector sends to the default arm.

// ---- log/c3/c3ck_gen.py output ----------------------------------------------------------------
const C3CK_DATA = {
  ihdr: ['00000001000000010100000000168ad7a9', '00000001000000010200000000f48f08dc',
         '00000007000000030400000001e9bbb903', '00000010000000100800000000bed92b95',
         '7fffffff000000011000000001832eae59', '000000030000000508020000008b524add',
         '000000090000000910020000016980531f', '00000004000000040103000000177e8715',
         '00000004000000040203000001827c68f6', '00000004000000040403000000df9e0865',
         '000000ff000000ff08030000002832de75', '000000020000000208040000012bf97e11',
         '00000002000000021004000000a9cb3761', '00000006000000060806000000648d6460',
         '00000006000000061006000001e6bf2d10', '000000017fffffff08000000017dc71850'],
  iend: ['00000000',
         'a5a5a5a5',
         '11b8b2cf7f',
         '11365b80f855d4de',
         '11365b80a5caeff26fbbf3',
         '11365b80a5caef14493b5248',
         '11365b80a5caef1439535067b7',
         '11365b80a5caef14395e83a8cdf2173c9a53122f',
         '11365b80a5caef14395e83a8cdf2173c61e5e522ca',
         '11365b80a5caef14395e83a8cdf2173c6186abd0f51a3f0d9360f7',
         '11365b80a5caef14395e83a8cdf2173c6186abd0f51a3f6489aed3f81d42678c1f494016',
         '11365b32a5acf9'],
  phys: ['000000010000000100344ef64a',
         '123456787fffffff0198555dbc',
         '0000000000000000ffcb0bfb23',
         'ffffffff0000000002b4543b76',
         '0000004800000048014aa56934',
         '800000008000000000941f01d6',
         '00000000',
         '23d5c03b5a',
         '23486d92b7dc0126b5b3e7fd',
         '23486d92b7dc01264b7038880d30',
         '23486d92b7dc01264b7095badf04294ea06dcb6e',
         '000000050000000703c529e7c5',
         '0a0b0c0d0102030409ddecd90e',
         '23486d9286bd4193'],
  time: ['07cf0c1f173b3be12f54b0',
         '0000000000000038c97adb',
         'ffffffffffffffff41d9ed',
         '07d201010000002744b7a4',
         '04d205060708096f14b2cc',
         '01020304050607d541cd2d',
         '00000000',
         '3521148e0b',
         '355a7fa4c9ee277106d9',
         '355a7fa4c9ee13381408b244',
         '355a7fa4c9ee13385d82a7ccf1160fcba407',
         '07b201020304052b4598c2',
         '07ea0a090c22385e0689e5',
         '355a7ff8aa82bf'],
  sbit: ['05a2681b02',
         '08797cc21a',
         '0407bbd14258',
         '01021369e737',
         '05060731b429dc',
         '08080892f52f35',
         '01020304b63cfbcd',
         'ff00807fa1dd88ae',
         '00000000',
         '0909e9c71112',
         '0102030405e2ae3c51',
         '01020304050607083fca88c5',
         '020406d46fc8e8',
         '10cfb5ffe9'],
  unknown: ['00000000',
            'a5a5a5a5',
            '597e9d1da8d9',
            '597ea3c8eddbc20817',
            '597ea3c8ed12375c1c9c0fb8',
            '597ea3c8ed12375c8128be5e2f',
            '597ea3c8ed12375c81a6cbf0153a5f8448f1d25a',
            '597ea3c8ed12375c81a6cbf0153a5f84a9cef3185c53aa92',
            '597ea3c8ed12375c81a6cbf0153a5f84a9cef3183d6287acd1f61b40658aafdc6d87b4',
            '597ea3c8ed12375c81a6cbf0153a5f84a9cef3183d6287acd1f61b40658aafd43ac946f7',
            '59c0b506dd',
            '597ea3c8ed12375c81a6cbf0832dcebd'],
};
// ---- end of generator output -------------------------------------------------------------------

(function () {
  const C3CK = {
    JGLD: null,
    va(a) {
      if (!C3CK.JGLD) C3CK.JGLD = Process.getModuleByName('jgld.dll').base;
      return C3CK.JGLD.add(a - 0x10000000);
    },

    // A per-offset word pattern, so a store one field too wide or a field left unwritten changes the
    // hash of the compared region.
    fill(p, n, seed) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(((seed << 16) + i) >>> 0); },

    bytes(p, hex) {
      const n = hex.length / 2;
      const a = new Uint8Array(n);
      for (let i = 0; i < n; i++) a[i] = parseInt(hex.substr(i * 2, 2), 16);
      if (n) p.writeByteArray(a.buffer);
      return n;
    },

    page: null,
    next: 0,
    code(bytes) {
      if (!C3CK.page) {
        C3CK.page = Memory.alloc(4 * Process.pageSize);
        Memory.protect(C3CK.page, 4 * Process.pageSize, 'rwx');
        C3CK.next = 0;
      }
      const p = C3CK.page.add(C3CK.next);
      C3CK.next += (bytes.length + 0xf) & ~0xf;
      p.writeByteArray(bytes);
      return p;
    },

    // A one-byte `ret` for the png_struct's warning and error slots (c3bq's trick): png_warning
    // 0x10079040 calls +0x44 instead of writing to a C stream, and png_error 0x10078fe0 calls +0x40
    // before falling into png_default_error, which no vector reaches.
    retStub: null,
    ret() {
      if (!C3CK.retStub) C3CK.retStub = C3CK.code([0xC3]);
      return C3CK.retStub;
    },

    // c3ch's reader, one per control block with the block's address baked in:
    //   void __cdecl reader(png_structp, png_bytep data, png_size_t length)
    //   { memcpy(data, ctl->src + ctl->cursor, length); ctl->cursor += length;
    //     ctl->calls++; ctl->lastLen = length; }
    // ctl: cursor +0, src +4, calls +8, lastLen +0xc. After the three pushes the arguments sit at
    // [esp+0x10] (png_ptr), [esp+0x14] (data) and [esp+0x18] (length).
    reader(ctl) {
      const c = ctl.toUInt32();
      return C3CK.code([
        0x53, 0x56, 0x57,                    // push ebx / esi / edi
        0x8B, 0x7C, 0x24, 0x14,              // mov edi, [esp+0x14]       data
        0x8B, 0x4C, 0x24, 0x18,              // mov ecx, [esp+0x18]       length
        0xBB, c & 0xff, (c >>> 8) & 0xff, (c >>> 16) & 0xff, (c >>> 24) & 0xff,   // mov ebx, ctl
        0x8B, 0x73, 0x04,                    // mov esi, [ebx+4]          src
        0x03, 0x33,                          // add esi, [ebx]            + cursor
        0x01, 0x0B,                          // add [ebx], ecx            cursor += length
        0xFF, 0x43, 0x08,                    // inc dword [ebx+8]         calls++
        0x89, 0x4B, 0x0C,                    // mov [ebx+0xc], ecx        lastLen = length
        0xF3, 0xA4,                          // rep movsb
        0x5F, 0x5E, 0x5B,                    // pop edi / esi / ebx
        0xC3,
      ]);
    },

    // One key's worth of png_structs, png_infos, control blocks, source buffers and zbufs.
    // spec[i] = { src, mode, flags, name, zbufSize, colorType, channels, valid }
    pngSet(spec, seed) {
      const n = spec.length;
      const PNG = 0x200, INFO = 0x100, CTL = 0x10, SRC = 0x100, ZBUF = 0x40;
      const out = { n: n };
      out.pngBlk = Memory.alloc(n * PNG);
      out.infoBlk = Memory.alloc(n * INFO);
      out.ctlBlk = Memory.alloc(n * CTL);
      out.srcBlk = Memory.alloc(n * SRC);
      out.zbufBlk = Memory.alloc(n * ZBUF);
      C3CK.fill(out.pngBlk, n * PNG, seed);
      C3CK.fill(out.infoBlk, n * INFO, seed + 1);
      C3CK.fill(out.ctlBlk, n * CTL, seed + 2);
      C3CK.fill(out.zbufBlk, n * ZBUF, seed + 3);
      for (let i = 0; i < n * SRC; i++) out.srcBlk.add(i).writeU8(0);
      out.png = [];
      out.info = [];
      spec.forEach((s, i) => {
        const png = out.pngBlk.add(i * PNG);
        const info = out.infoBlk.add(i * INFO);
        const ctl = out.ctlBlk.add(i * CTL);
        const src = out.srcBlk.add(i * SRC);
        C3CK.bytes(src, s.src);
        ctl.writeU32(0);                                   // cursor
        ctl.add(4).writePointer(src);                      // src
        ctl.add(8).writeU32(0);                            // calls
        ctl.add(0xc).writeU32(0);                          // lastLen
        png.add(0x40).writePointer(C3CK.ret());            // error_fn
        png.add(0x44).writePointer(C3CK.ret());            // warning_fn
        png.add(0x50).writePointer(C3CK.reader(ctl));      // read_data_fn
        png.add(0x68).writeU32(s.mode);                    // mode
        png.add(0x6c).writeU32(s.flags);                   // flags
        png.add(0xac).writePointer(out.zbufBlk.add(i * ZBUF));   // zbuf
        png.add(0xb0).writeU32(s.zbufSize === undefined ? 8 : s.zbufSize);
        png.add(0x110).writeU32(0);                        // crc, the value the CRC bytes assume
        png.add(0x11c).writeU32(s.name);                   // chunk_name
        if (s.colorType !== undefined) png.add(0x126).writeU8(s.colorType);
        if (s.channels !== undefined) png.add(0x12a).writeU8(s.channels);
        info.add(8).writeU32(s.valid === undefined ? 0 : s.valid);
        out.png.push(png);
        out.info.push(info);
      });
      return out;
    },

    keys(out, set, prefix) {
      out[prefix + 'Png'] = set.pngBlk;
      out[prefix + 'Info'] = set.infoBlk;
      out[prefix + 'Ctl'] = set.ctlBlk;
      for (let i = 0; i < set.n; i++) {
        out[prefix + 'P' + i] = set.png[i];
        out[prefix + 'I' + i] = set.info[i];
      }
    },
  };

  // --------------------------------------------------------------------------- the Palette blocks
  //
  // Sixteen palettes of our own at a 0x1000 stride, each pre-filled with a per-offset pattern of its
  // own and carrying jgld's Palette vtable VA 0x1011db10 at +0, so slot 2 of setDefault's virtual call
  // (0x1006a627) reaches the real Palette::release 0x1006a380 and zeroes +0x810 and +0x814. The applied
  // id at +0x814 is seeded ABOVE 0xffff so that even if release did not run, the id Palette::update
  // 0x1006a3d0 draws (a non-zero 16-bit value, 0x1006a40b) could not accidentally equal it and skip
  // Palette::apply 0x1006a4a0. The compared region of each palette is +0 to +0x810: everything from
  // +0x810 on is the generation id, the applied id and the QueryPerformanceCounter value, which differ
  // between two runs by construction.
  function palettes(seed) {
    const N = 16, STRIDE = 0x1000;
    const blk = Memory.alloc(N * STRIDE);
    C3CK.fill(blk, N * STRIDE, seed);
    const out = { blk: blk, p: [] };
    const vt = C3CK.va(0x1011db10);
    for (let i = 0; i < N; i++) {
      const p = blk.add(i * STRIDE);
      p.writePointer(vt);                       // +0x000 vtable
      p.add(0x80c).writePointer(ptr(0));        // +0x80c owner, untouched by these four methods
      p.add(0x810).writeU32(0x4321 + i);        // +0x810 generation id
      p.add(0x814).writeU32(0x7f000000 + i);    // +0x814 applied id
      p.add(0x818).writeU32(0);
      p.add(0x81c).writeU32(0);
      out.p.push(p);
    }
    return out;
  }

  // Suspend the process's first thread and schedule the undo (c3cd's helper). The resume runs from
  // setTimeout(0), i.e. after diff_hook's run() has returned all of the key's vectors, and the undo
  // runs first so the global is already back when the thread moves again. If the suspend cannot be
  // taken the fixture THROWS, so the A/B is recorded as ERROR instead of running against live state.
  // Shared frozen-thread helper (re/frida/js/freeze.js): suspends the process's first thread at a safe point;
  // `undo` runs at the flush, before the thread is resumed.
  function freezeFirstThread(undo) {
    __freeze.thread();
    __freeze.atFlush(undo);
  }

  // __thiscall recording stub for one event-sink vtable slot (c3cd's): writes `tag` and its `n` dword
  // arguments into the sink's record area, advances the cursor at sink+4 and returns its first stack
  // argument. With n = 0 it is a plain `xor eax,eax; ret`.
  function sinkStub(tag, n) {
    if (n === 0) return [0x33, 0xC0, 0xC3];
    const b = [0x8B, 0x41, 0x04, 0x03, 0xC1,
               0xC7, 0x00, tag & 0xff, (tag >>> 8) & 0xff, (tag >>> 16) & 0xff, (tag >>> 24) & 0xff];
    for (let i = 1; i <= n; i++) b.push(0x8B, 0x54, 0x24, 4 * i, 0x89, 0x50, 4 * i);
    b.push(0x8B, 0x51, 0x04, 0x83, 0xC2, 4 * (n + 1), 0x89, 0x51, 0x04);
    b.push(0x8B, 0x44, 0x24, 0x04);
    b.push(0xC2, 4 * n, 0x00);
    return b;
  }

  const NAME = (a, b, c, d) => (a | (b << 8) | (c << 16) | (d << 24)) >>> 0;
  const IHDR = NAME(0x49, 0x48, 0x44, 0x52);   // critical
  const IEND = NAME(0x49, 0x45, 0x4e, 0x44);   // critical
  const PHYS = NAME(0x70, 0x48, 0x59, 0x73);   // ancillary
  const OFFS = NAME(0x6f, 0x46, 0x46, 0x73);   // ancillary
  const TIME = NAME(0x74, 0x49, 0x4d, 0x45);   // ancillary
  const SBIT = NAME(0x73, 0x42, 0x49, 0x54);   // ancillary
  const ANC = [NAME(0x62, 0x4b, 0x47, 0x44), NAME(0x67, 0x41, 0x4d, 0x41),
               NAME(0x73, 0x52, 0x47, 0x42), NAME(0x70, 0x43, 0x41, 0x4c)];

  Object.assign(globalThis.DIFF_FIXTURES, {
    // ================================================== png_handle_IHDR 0x100796e0 (16 vectors)
    //
    // Sixteen valid image headers: every one of the five bit depths, all five legal colour types, both
    // interlace methods and the two extreme widths, so the channel table at 0x100799be, the pixel-depth
    // product and the rowbytes shift all take different values. Every png_struct starts with mode 0
    // (anything else reaches png_error 0x100796ff) and carries flag 0x400, which is what makes a
    // disagreeing CRC on a CRITICAL chunk a warning at 0x100795e0 instead of the fatal png_chunk_error
    // that longjmps. Eight of the sixteen carry a CRC that disagrees; png_handle_IHDR ignores the
    // result either way (0x10079750), which is exactly what the A/B shows.
    c3ck_ihdr() {
      const out = {};
      const set = C3CK.pngSet(C3CK_DATA.ihdr.map((src, i) => ({
        src: src, mode: 0, flags: 0x400, name: IHDR, zbufSize: [4, 8, 16, 32][i & 3],
      })), 0x6b01);
      C3CK.keys(out, set, 'ih');
      out.obj = set.pngBlk;
      return out;
    },

    // ================================================== png_handle_IEND 0x10079c20 (12 vectors)
    //
    // Twelve end-of-stream chunks. Every one carries both PNG_HAVE_IHDR and PNG_HAVE_IDAT in the mode
    // (without them the body calls png_error 0x10079c5b, which longjmps), with different spare bits
    // above them so the `or al, 0x18` at 0x10079c71 is seen not to touch the upper three bytes. The
    // lengths 0 to 32 against zbuf sizes 4, 8 and 16 take both sides of png_crc_finish 0x10079510's
    // loop at 0x1007954b and of its remainder test at 0x10079573 - length 8 with zbuf 8 and length 16
    // with zbuf 16 are the multiples the remainder read takes alone - and every non-zero length also
    // takes the warning arm at 0x10079c7d.
    c3ck_iend() {
      const out = {};
      const MODE = [0x5, 0x7, 0x1d, 0xf5, 0x105, 0x5, 0xffffff05, 0x7, 0x5, 0x25, 0x5, 0x8000000d];
      const ZB = [8, 8, 8, 4, 8, 8, 16, 8, 8, 16, 8, 4];
      const set = C3CK.pngSet(C3CK_DATA.iend.map((src, i) => ({
        src: src, mode: MODE[i], flags: 0x400, name: IEND, zbufSize: ZB[i],
      })), 0x6b11);
      C3CK.keys(out, set, 'ie');
      out.obj = set.pngBlk;
      return out;
    },

    // ================================================== png_handle_pHYs 0x1007b380 (14 vectors)
    //
    // Vectors 0 to 5 and 11 to 12 carry the nine-byte payload; 6 to 10 and 13 carry a payload length the
    // reader refuses (0, 1, 8, 10, 16 and 4), which takes the warning arm at 0x1007b42a and hands that
    // length to png_crc_finish. Vector 11 has PNG_HAVE_IDAT set (the out-of-place arm 0x1007b3c3),
    // vector 12 a png_info whose valid word already carries the pHYs bit 0x80 (the duplicate arm
    // 0x1007b3fe), and vectors 2 and 13 a NULL info_ptr, which skips the duplicate test at 0x1007b3ef
    // and makes png_set_pHYs 0x1007dfc0 a no-op. Seven of the fourteen carry a CRC that disagrees, which
    // is the only way to take the `jne` side of 0x1007b471 and drop a well-formed chunk.
    c3ck_phys() {
      const out = {};
      const MODE = [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 5, 1, 1];
      const VALID = [0, 0x40, 0, 0x200, 0, 0, 0, 0, 0, 0, 0, 0, 0x80, 0];
      const set = C3CK.pngSet(C3CK_DATA.phys.map((src, i) => ({
        src: src, mode: MODE[i], flags: 0, name: PHYS, zbufSize: [4, 8, 16, 8][i & 3],
        valid: VALID[i],
      })), 0x6b21);
      C3CK.keys(out, set, 'ph');
      out.phI2 = ptr(0);
      out.phI13 = ptr(0);
      out.obj = set.pngBlk;
      return out;
    },

    // ================================================== png_handle_oFFs 0x1007b4d0 (14 vectors)
    //
    // The same fourteen shapes against the oFFs valid bit 0x100 (0x1007b547) and png_set_oFFs
    // 0x1007ddb0: a nine-byte payload, the five refused lengths, the out-of-place arm, the duplicate arm
    // and two NULL info_ptrs.
    c3ck_offs() {
      const out = {};
      const MODE = [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 5, 1, 1];
      const VALID = [0, 0x80, 0, 0x200, 0, 0, 0, 0, 0, 0, 0, 0, 0x100, 0];
      const set = C3CK.pngSet(C3CK_DATA.phys.map((src, i) => ({
        src: src, mode: MODE[i], flags: 0, name: OFFS, zbufSize: [8, 4, 8, 16][i & 3],
        valid: VALID[i],
      })), 0x6b31);
      C3CK.keys(out, set, 'of');
      out.ofI2 = ptr(0);
      out.ofI13 = ptr(0);
      out.obj = set.pngBlk;
      return out;
    },

    // ================================================== png_handle_tIME 0x1007b9b0 (14 vectors)
    //
    // Vectors 0 to 5, 11 and 12 carry the seven-byte payload; 6 to 10 and 13 carry 0, 1, 6, 8, 14 and 3
    // bytes, which the length test at 0x1007ba40 refuses. Vector 11's png_info already carries the tIME
    // valid bit 0x200, which is the duplicate arm at 0x1007b9f4, and vector 12 has PNG_HAVE_IDAT, which
    // is the only way to take the `jne` side of 0x1007ba2a and set bit 3 of the mode. Vectors 2 and 13
    // pass a NULL info_ptr. Seven CRCs disagree, which takes the other side of 0x1007ba8a.
    c3ck_time() {
      const out = {};
      const MODE = [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 5, 1];
      const VALID = [0, 0x100, 0, 0x80, 0, 0, 0, 0, 0, 0, 0, 0x200, 0, 0];
      const set = C3CK.pngSet(C3CK_DATA.time.map((src, i) => ({
        src: src, mode: MODE[i], flags: 0, name: TIME, zbufSize: [8, 8, 4, 16][i & 3],
        valid: VALID[i],
      })), 0x6b41);
      C3CK.keys(out, set, 'tm');
      out.tmI2 = ptr(0);
      out.tmI13 = ptr(0);
      out.obj = set.pngBlk;
      return out;
    },

    // ================================================== png_handle_sBIT 0x10079ec0 (14 vectors)
    //
    // The expected payload length is three for a palette image and the channel count otherwise
    // (0x10079fa8), so the png_structs carry five different colour-type / channel pairs: 0/1, 4/2, 2/3,
    // 6/4 and 3/1. Vectors 0 to 7 and 13 give exactly that length, which reaches the two writing arms -
    // greyscale (colour type bit 1 clear: the gray byte plus red, green and blue all from the first
    // payload byte, 0x1007a061) and colour (0x1007a02f) - and vectors 8, 10 and 11 give a length the
    // test at 0x10079fc7 refuses. Vector 9's png_info already carries the sBIT valid bit 2 (the
    // duplicate arm 0x10079f70), vector 12 has PNG_HAVE_IDAT (the out-of-place arm 0x10079f1c) and
    // vector 13 has PNG_HAVE_PLTE, which only WARNS and carries on (0x10079f4f). Vector 2 passes a NULL
    // info_ptr, so png_set_sBIT 0x1007e070 does nothing while the png_struct's own sig_bit field is
    // still written. Six CRCs disagree.
    c3ck_sbit() {
      const out = {};
      //        0     1     2     3     4      5     6     7     8     9    10    11    12    13
      const CT = [0, 0, 4, 4, 3, 2, 6, 6, 3, 0, 2, 6, 2, 0];
      const CH = [1, 1, 2, 2, 1, 3, 4, 4, 1, 1, 3, 4, 3, 1];
      const MODE = [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 5, 3];
      const VALID = [0, 0x80, 0, 0x100, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0];
      const set = C3CK.pngSet(C3CK_DATA.sbit.map((src, i) => ({
        src: src, mode: MODE[i], flags: 0, name: SBIT, zbufSize: [8, 4, 8, 16][i & 3],
        colorType: CT[i], channels: CH[i], valid: VALID[i],
      })), 0x6b51);
      // the five bytes of the png_color_8 at png+0x17c, pre-filled so that the byte each arm LEAVES
      // alone (gray on the colour path) is visible in the compared state
      for (let i = 0; i < set.n; i++) {
        for (let k = 0; k < 5; k++) set.png[i].add(0x17c + k).writeU8((0xb0 + i * 5 + k) & 0xff);
      }
      C3CK.keys(out, set, 'sb');
      out.sbI2 = ptr(0);
      out.obj = set.pngBlk;
      return out;
    },

    // =============================================== png_handle_unknown 0x1007c150 (12 vectors)
    //
    // Twelve ancillary chunks, which is what the test at 0x1007c18d demands: a critical chunk reaches
    // png_chunk_error 0x100790a0, which longjmps, and so does any name byte outside the two letter
    // ranges png_check_chunk_name 0x1007c1f0 accepts. Four names are used in rotation. Half the vectors
    // carry PNG_HAVE_IDAT, which is the only way to set bit 3 of the mode at 0x1007c1b3; the lengths 0
    // to 32 against zbuf sizes 4, 8 and 16 drive png_crc_finish's loop; and vector 5 carries flags 0x300
    // complete, which is how png_crc_error 0x10079620 is made to skip the comparison altogether, so its
    // chunk is accepted although its CRC bytes disagree.
    c3ck_unknown() {
      const out = {};
      const MODE = [0, 4, 1, 5, 0, 4, 1, 5, 0, 4, 1, 5];
      const FLAGS = [0, 0, 0, 0, 0, 0x300, 0, 0x100, 0, 0, 0x100, 0];
      const ZB = [8, 8, 4, 8, 16, 8, 8, 4, 16, 8, 8, 8];
      const set = C3CK.pngSet(C3CK_DATA.unknown.map((src, i) => ({
        src: src, mode: MODE[i], flags: FLAGS[i], name: ANC[i & 3], zbufSize: ZB[i],
      })), 0x6b61);
      C3CK.keys(out, set, 'uk');
      out.obj = set.pngBlk;
      return out;
    },

    // ============================== Palette::setDefault / setRGB / getRGBRange (sixteen palettes)
    //
    // Sixteen palettes of ours plus four RGB source buffers and sixteen destination buffers. Nothing
    // here is the game's: the running game has its own Palette and enters setRGB and getRGBRange some
    // four hundred times a second (log/c3/alt/c3ck_probe.txt) over it, never over these.
    c3ck_palettes() {
      const out = {};
      const pal = palettes(0x6b71);
      out.palBlk = pal.blk;
      for (let i = 0; i < pal.p.length; i++) out['p' + i] = pal.p[i];
      // four source buffers of 0x400 bytes, enough for 256 triples plus the overrun vector
      const rgbBlk = Memory.alloc(4 * 0x400);
      for (let i = 0; i < 4 * 0x400; i++) rgbBlk.add(i).writeU8((i * (7 + i % 3) + 0x1d) & 0xff);
      out.rgbBlk = rgbBlk;
      for (let i = 0; i < 4; i++) out['rgb' + i] = rgbBlk.add(i * 0x400);
      // sixteen destination buffers of 0x400 bytes for getRGBRange
      const dstBlk = Memory.alloc(16 * 0x400);
      C3CK.fill(dstBlk, 16 * 0x400, 0x6b72);
      out.dstBlk = dstBlk;
      for (let i = 0; i < 16; i++) out['dst' + i] = dstBlk.add(i * 0x400);
      out.obj = pal.blk;
      return out;
    },

    // ================================================= Palette::getColor 0x1006add0 (14 vectors)
    //
    // The same sixteen palettes, plus the three module bytes at VA 0x10128734 as the compared state -
    // the only module global this batch compares. The running game writes them (30 entries into
    // getColor in 20 s at the menu, all on the process's first thread), so this key suspends that
    // thread for its vectors; without the suspend a write between the two arms would be a false RED.
    c3ck_getcolor() {
      const out = {};
      const pal = palettes(0x6b81);
      out.palBlk = pal.blk;
      for (let i = 0; i < pal.p.length; i++) out['g' + i] = pal.p[i];
      const scratch = C3CK.va(0x10128734);
      out.scratch = scratch;
      out.obj = scratch;
      const saved = [scratch.readU8(), scratch.add(1).readU8(), scratch.add(2).readU8()];
      freezeFirstThread(function () {
        scratch.writeU8(saved[0]);
        scratch.add(1).writeU8(saved[1]);
        scratch.add(2).writeU8(saved[2]);
      });
      return out;
    },

    // ==================================================== windowProc 0x100692b0 (45 vectors)
    //
    // A recording event sink of ours in VA 0x1012872c and a recording command handler in VA 0x10128728
    // for the length of the key, with the process's first thread suspended and both globals written back
    // when the key ends. The sink's vtable carries a stub per slot with that slot's own tag and stack
    // argument count, so WHICH slot each message reaches, with which of windowProc's arguments and in
    // which order, is in the compared state; the stub returns its first stack argument, which is what
    // makes WM_NCHITTEST's pass-through at 0x100694ec visible in the return value. The window handle is
    // the invalid 0x1 and the display global 0x10128420 is deliberately left alone (see the header).
    c3ck_wndproc() {
      const out = {};
      const sink = Memory.alloc(0x200);
      C3CK.fill(sink, 0x200, 0x6b91);
      const vt = Memory.alloc(0x80);
      // [slot offset, tag, stack argument count]; the dispatch site of each is in shim/src/re/c3ck.cpp
      const spec = [[0x00, 0xB0000000, 2],    // onWmSize         0x10069de0
                    [0x04, 0xB1000000, 1],    // onPaint          0x10069b2c
                    [0x08, 0xB2000000, 2],    // onWmNcHitTest    0x10069e40
                    [0x0c, 0xB3000000, 2],    // onKey            0x10069ca0
                    [0x10, 0xB4000000, 1],    // onChar           0x10069d7c
                    [0x14, 0xB5000000, 2],    // onWmMouseMove    0x10069f10
                    [0x18, 0xB6000000, 3],    // onWmMouseWheel   0x10069ea3
                    [0x1c, 0xB7000000, 3],    // onWmLButtonDown  0x10069f73
                    [0x20, 0xB8000000, 2],    // onWmLButtonUp    0x10069fe0
                    [0x24, 0xB9000000, 3],    // onWmRButtonDown  0x1006a043
                    [0x28, 0xBA000000, 2],    // onWmRButtonUp    0x1006a0b0
                    [0x2c, 0xBB000000, 2],    // onActivate       0x10069bef
                    [0x30, 0xBC000000, 1]];   // windowProc's own 0x100693d0 for message 0x401
      const nullStub = C3CK.code(sinkStub(0, 0));
      for (let i = 0; i < 0x20; i++) vt.add(i * 4).writePointer(nullStub);
      for (const [slot, tag, n] of spec) vt.add(slot).writePointer(C3CK.code(sinkStub(tag, n)));
      sink.writePointer(vt);                 // +0x00 vtable
      sink.add(4).writeU32(0x40);            // +0x04 record cursor (a byte offset from the object)
      out.sink = sink;

      // the __cdecl command handler onCommand 0x1006a0e0 calls: it records its two arguments and
      // returns the first, and does NOT pop them (plain `ret`), which is what onCommand's `add esp,8`
      // at 0x1006a111 requires.
      const rec = Memory.alloc(0x40);
      C3CK.fill(rec, 0x40, 0x6b92);
      const handler = C3CK.code([0xB8, 0, 0, 0, 0,
                                 0x8B, 0x54, 0x24, 0x04, 0x89, 0x10,
                                 0x8B, 0x54, 0x24, 0x08, 0x89, 0x50, 0x04,
                                 0x8B, 0x44, 0x24, 0x04, 0xC3]);
      handler.add(1).writePointer(rec);
      out.rec = rec;
      out.hwnd = ptr(1);
      out.obj = sink;

      const gSink = C3CK.va(0x1012872c);
      const gCmd = C3CK.va(0x10128728);
      const savedSink = gSink.readPointer();
      const savedCmd = gCmd.readPointer();
      freezeFirstThread(function () {
        gSink.writePointer(savedSink);
        gCmd.writePointer(savedCmd);
      });
      gSink.writePointer(sink);
      gCmd.writePointer(handler);
      return out;
    },
  });
})();
