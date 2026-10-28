// ---------------------------------------------------------------- FLIC SHEET-CELL RECORDER (roadmap S3.4, 2026-10-24)
// Appended after scenario_det.js by re/tools/flic_cells_rec.py only, as `const FLIC_CFG = {...};` + this file. No pinned
// run loads it, and it changes no shared agent file.
//
// What it records, on the main thread only, for every call of loadFlic (golf_clean.exe 0x0043d740, cdecl
// (id, path, frames, size, flag), plain `ret` at 0x0043dbdd):
//  - at entry: the five arguments, the path text, the caller (return address), g_sheetBase 0x00820b6c, the cell cursor
//    0x00820ec0 and g_flicLoadCount 0x00820ecc;
//  - every flicOpenStream (0x00404120, cdecl (path, slot, outCount)) the load makes: path text, slot, its result (the
//    frame Surface pointer, 0 on a failed open) and the count it stored (loadFlic reads it from 0x0053f3e8 + 4 * id);
//  - every Surface_blit (0x00473bf0, thiscall (src, x, y, w, h, kind, trim), `ret 0x1c`) whose return address lies in
//    loadFlic (0x0043db31 / 0x0043db80 call it): the cell (ecx), the seven arguments, its result (eax) and, at its exit,
//    the cell's 0x2c-byte SpriteRef record plus the jgld Sprite its +4 handle points at: the object's words +0x00..+0x38
//    (vtable, m_10 palette, m_14 pixel buffer, m_18 flags, m_1c row spans, m_20 bpp, m_24 buffer bytes, m_28 key,
//    m_2c pitch, m_30 width, m_34 height; re/match/jgld_sprite.cpp class Sprite), m_24 bytes of the buffer and, when
//    m_18 bit 0 is set (Sprite::create8's row-span form), m_34 * 4 bytes of spans;
//  - at exit: g_sheetFirstCell 0x005a9370[id], g_sheetRows 0x005a5a2c[id], the count word 0x0053f3e8[id], the cursor, and
//    the working Image 0x005aa778's handle words (width slot +0xd8, height slot +0xdc, pitch slot +0xe0, palette slot +0xe8
//    of its jgld Surface, the slots cropSprite reads), and the 256 three-byte palette entries of the load's slot and of
//    the shadow slot 590 (0x24e) after the load (appended after the cells' bytes, in `pals` order).
// One 'flic' message per load (JSON meta + the cells' bytes concatenated); a failed open sends 'flic_open_failed' at once
// (the load does not return then on this install's Tree Cerc / Tree Drac names, U-1701). JavaScript runs on the game thread only inside
// these listeners and calls no clock import, so the virtual clock is unchanged (flic_cells_rec.py compares the run's
// checkpoints with the pinned reference).
const FCC = (typeof FLIC_CFG !== 'undefined') ? FLIC_CFG : {};
const FC_LOADFLIC = ptr(0x0043d740), FC_LOADFLIC_END = 0x0043dbe0;
const FC_OPENSTREAM = ptr(0x00404120);
const FC_BLIT = ptr(0x00473bf0);
const FC_SHEETBASE = ptr(0x00820b6c), FC_CURSOR = ptr(0x00820ec0), FC_LOADCOUNT = ptr(0x00820ecc);
const FC_FIRSTCELL = 0x005a9370, FC_ROWS = 0x005a5a2c, FC_COUNTS = 0x0053f3e8;
const FC_WORKIMG = ptr(0x005aa778);
const FC_PALREC = 0x004e9aa0;
let fcCur = null;          // the load in progress
let fcSeq = 0;
let fcOther = 0;           // calls on other threads (counted only)

function fcStr(p) {
  try { return p.isNull() ? null : p.readCString(); } catch (e) { return '<unreadable>'; }
}
function fcSlot(obj, off) {   // thiscall slot `off` of obj's vtable, no argument, its eax
  const vt = obj.readPointer();
  return new NativeFunction(vt.add(off).readPointer(), 'uint32', ['pointer'], 'thiscall')(obj) >>> 0;
}
function fcWork() {
  const h = FC_WORKIMG.add(4).readPointer();
  if (h.isNull()) return { handle: 0 };
  return { handle: h.toUInt32(), w: fcSlot(h, 0xd8), h: fcSlot(h, 0xdc), pitch: fcSlot(h, 0xe0), pal: fcSlot(h, 0xe8) };
}

Interceptor.attach(FC_LOADFLIC, {
  onEnter(args) {
    if (Process.getCurrentThreadId() !== MAIN) { fcOther++; return; }
    const sp = this.context.esp;
    const a = [];
    for (let i = 0; i < 5; i++) a.push(sp.add(4 + 4 * i).readU32());
    fcCur = { seq: fcSeq++, args: a, path: fcStr(ptr(a[1])), caller: sp.readU32(), base: FC_SHEETBASE.readS32(),
              cursor0: FC_CURSOR.readS32(), loads0: FC_LOADCOUNT.readS32(), opens: [], cells: [], chunks: [], err: null };
    try { fcCur.work0 = fcWork(); } catch (e) { fcCur.err = 'work0 ' + e; }
  },
  onLeave() {
    if (Process.getCurrentThreadId() !== MAIN || fcCur === null) return;
    const c = fcCur;
    fcCur = null;
    try {
      const id = (c.args[0] + c.base) | 0;
      c.id = id;
      c.firstCell = ptr(FC_FIRSTCELL + 4 * id).readS32();
      c.rows = ptr(FC_ROWS + 4 * id).readS32();
      c.count = ptr(FC_COUNTS + 4 * id).readS32();
      c.cursor1 = FC_CURSOR.readS32();
      c.work1 = fcWork();
      // the slot's palette after the load: Palette::decodeChunk writes the flic's colour chunks to the object at +4 of
      // the frame-table record 0x004e9aa0 + 0x58 * slot (the Flic's +0x74, flicOpenStream's source argument); its slot
      // +0x10 (buf, 0, 0x100) copies the 256 three-byte entries out, as decodeChunk itself does first
      c.pals = [];
      for (const slot of [id, 590]) {
        const p4 = ptr(FC_PALREC + 0x58 * slot).add(4).readPointer();
        if (p4.isNull()) { c.pals.push({ slot: slot, obj: 0, len: 0 }); continue; }
        const buf = Memory.alloc(0x300);
        new NativeFunction(p4.readPointer().add(0x10).readPointer(), 'uint32', ['pointer', 'pointer', 'int', 'int'],
                          'thiscall')(p4, buf, 0, 0x100);
        c.chunks.push(buf.readByteArray(0x300));
        c.pals.push({ slot: slot, obj: p4.toUInt32(), len: 0x300 });
      }
    } catch (e) { c.err = (c.err || '') + ' leave ' + e; }
    let total = 0;
    for (const b of c.chunks) total += b.byteLength;
    const buf = new Uint8Array(total);
    let o = 0;
    for (const b of c.chunks) { buf.set(new Uint8Array(b), o); o += b.byteLength; }
    delete c.chunks;
    send({ type: 'flic', meta: c }, buf.buffer);
  }
});

Interceptor.attach(FC_OPENSTREAM, {
  onEnter(args) {
    if (Process.getCurrentThreadId() !== MAIN || fcCur === null) { this.fc = null; return; }
    const sp = this.context.esp;
    this.fc = { path: fcStr(sp.add(4).readPointer()), slot: sp.add(8).readU32(), out: sp.add(12).readU32() };
  },
  onLeave(ret) {
    if (!this.fc || fcCur === null) return;
    this.fc.ret = ret.toUInt32();
    this.fc.count = this.fc.out ? ptr(this.fc.out).readS32() : null;
    fcCur.opens.push(this.fc);
    // a failed open is reported at once: loadFlic then calls Image_draw on the NULL frame Surface (U-1701)
    if (this.fc.ret === 0) send({ type: 'flic_open_failed', seq: fcCur.seq, args: fcCur.args, path: fcCur.path, open: this.fc });
  }
});

Interceptor.attach(FC_BLIT, {
  onEnter(args) {
    this.fc = null;
    if (Process.getCurrentThreadId() !== MAIN || fcCur === null) return;
    const sp = this.context.esp;
    const ra = sp.readU32();
    if (ra < 0x0043d740 || ra >= FC_LOADFLIC_END) return;
    const a = [];
    for (let i = 0; i < 7; i++) a.push(sp.add(4 + 4 * i).readU32());
    this.fc = { cell: this.context.ecx.toUInt32(), args: a, ra: ra };
  },
  onLeave(ret) {
    if (!this.fc || fcCur === null) return;
    const r = this.fc;
    r.ret = ret.toInt32();
    try {
      const cell = ptr(r.cell);
      const rec = cell.readByteArray(0x2c);
      const parts = [rec];
      r.recLen = 0x2c;
      const h = cell.add(4).readPointer();
      r.handle = h.toUInt32();
      if (!h.isNull()) {
        const obj = h.readByteArray(0x38);
        parts.push(obj);
        r.objLen = 0x38;
        const w = new Uint32Array(obj);
        const px = ptr(w[0x14 / 4]), n = w[0x24 / 4], flags = w[0x18 / 4], spans = ptr(w[0x1c / 4]), hh = w[0x34 / 4];
        if (!px.isNull() && n > 0 && n < (1 << 24)) { parts.push(px.readByteArray(n)); r.pxLen = n; } else r.pxLen = 0;
        if ((flags & 1) && !spans.isNull() && hh > 0 && hh < 4096) { parts.push(spans.readByteArray(hh * 4)); r.spanLen = hh * 4; }
        else r.spanLen = 0;
      }
      for (const p of parts) fcCur.chunks.push(p);
    } catch (e) { r.err = String(e); }
    fcCur.cells.push(r);
  }
});

send({ type: 'flic_ready' });
