// ---------------------------------------------------------------- TEXT WIDTH RECORDER (phase 3.8 stream 38a, 2026-10-20)
// Appended after trace_rec.js by re/tools/widths_rec.py only, as `const WIDTHS_CFG = {...};` + this file. A plain
// re/tools/trace_tool.py record never loads it, and it changes neither scenario_det.js nor trace_rec.js.
//
// What it logs, on the main thread only (other threads are counted, not logged):
//  - every call of jgld.dll Font::textWidth (RVA 0x68e60, thiscall (s, n), `ret 8` at 0x10068f00): the Font object (ecx),
//    its owner word [Font+4] (= the exe FontRef + 4: Factory::createFont 0x10067850 passes the address of the FontRef's
//    handle field, FontRef::createFromId 0x00483821 / createSized 0x00483873), Font+0x14 (flags) and Font+0x18 (the
//    HFONT GetTextExtentPoint32A measures with, 0x10068e8c), s, n, the return address, the result (eax), the owner's
//    metrics words [owner+8] / [owner+0xc] (written by Font::create 0x100689e7 / 0x100689f6) and min(n, 4096) bytes of s;
//  - every call of golf_clean.exe Text_put (0x00477250, thiscall (text), `ret 4`) and of drawMarkupRun (0x00477280,
//    thiscall (text, len), the run Text_put measures and the one the aligned-draw wrappers measure): the draw context (ecx), the
//    context's markup words +0x38 +0x3c +0x40 +0x4c +0x54 and font words +0x5c +0x60 +0x64 +0x68 before the call (the
//    fields drawMarkupRun 0x00477280 reads), the default-font word g_primarySurface 0x0083ad44 (drawMarkupRun's fill of a
//    zero +0x5c, 0x0047728c), the handle word of the +0x5c FontRef, the text, the result, +0x38 +0x3c +0x4c +0x5c after,
//    and the Font::textWidth sequence numbers at entry and exit (the widths measured inside this call); for
//    drawMarkupRun also its `len` argument (the text is copied up to its NUL, which the scanners and strchr may pass).
// Both are native Interceptor listeners in a CModule: no JavaScript runs per call on the game thread, and no clock
// import is called, so the virtual clock and the trace are unchanged (widths_rec.py compares the trace written by the
// same run with the corpus trace byte for byte). Records go into two rings in JS-allocated memory (a CModule's own data
// is read-only in this process); a record is published (the ring's write word advanced) only at the function's exit,
// when its result is known. The JS thread drains the rings every WR_MS ms and sends the raw bytes as 'wrec' messages;
// widths_rec.py parses them. At the trace's END (trace_rec.js trEnd, wrapped below) recording stops, the rings are
// drained, and 'wrec_end' carries the counters and the LOGFONTA of every HFONT seen (GetObjectA, gdi32).
const WRC = (typeof WIDTHS_CFG !== 'undefined') ? WIDTHS_CFG : {};
const WR_CAP = 1 << 23;                     // bytes per ring (power of two; a record header keeps its length in 24 bits)
const WR_MS = WRC.drainMs || 20;
const WR_MAXCOPY = 4096;
const WR_DATA_LO = 0x004c1000, WR_DATA_HI = 0x00842174;   // golf_clean.exe .data (re/model: GameState 0x381174 bytes)
const WR_PRIMARY = 0x0083ad44;              // g_primarySurface
const wrCtl = K(Memory.alloc(256));
const wrTw = K(Memory.alloc(WR_CAP));
const wrTp = K(Memory.alloc(WR_CAP));
const wrDm = K(Memory.alloc(WR_CAP));
const wrFonts = K(Memory.alloc(4 * 512));   // [0] count, then up to 511 HFONT values in order of first use
// control words (indices; the C enum below must match)
const WI = { MAIN: 0, ON: 1, TW_CAP: 2, TW_W: 3, TW_R: 4, TW_SEQ: 5, TW_DROP: 6, TW_POFF: 7, TW_PLEN: 8, TW_OTHER: 9,
             TP_CAP: 10, TP_W: 11, TP_R: 12, TP_SEQ: 13, TP_DROP: 14, TP_POFF: 15, TP_PLEN: 16, TP_OTHER: 17,
             D_LO: 18, D_HI: 19, PRIMARY: 20, MAXCOPY: 21,
             DM_CAP: 22, DM_W: 23, DM_R: 24, DM_SEQ: 25, DM_DROP: 26, DM_POFF: 27, DM_PLEN: 28, DM_OTHER: 29 };
const wrSet = (i, v) => wrCtl.add(4 * i).writeU32(v >>> 0);
const wrGet = (i) => wrCtl.add(4 * i).readU32();
wrSet(WI.MAIN, MAIN);
wrSet(WI.ON, 1);
wrSet(WI.TW_CAP, WR_CAP);
wrSet(WI.TP_CAP, WR_CAP);
wrSet(WI.DM_CAP, WR_CAP);
wrSet(WI.D_LO, WR_DATA_LO);
wrSet(WI.D_HI, WR_DATA_HI);
wrSet(WI.PRIMARY, WR_PRIMARY);
wrSet(WI.MAXCOPY, WR_MAXCOPY);
wrFonts.writeU32(0);

const WR_SRC = `
#include <gum/guminterceptor.h>
typedef unsigned int u32;
extern volatile u32 wr_ctl[];
extern unsigned char wr_tw[];
extern unsigned char wr_tp[];
extern unsigned char wr_dm[];
extern volatile u32 wr_fonts[];
enum { C_MAIN = 0, C_ON = 1, TW_CAP = 2, TW_SEQ = 5, TW_OTHER = 9, TP_CAP = 10, TP_SEQ = 13, TP_OTHER = 17,
       D_LO = 18, D_HI = 19, PRIMARY = 20, MAXCOPY = 21, DM_CAP = 22, DM_SEQ = 25, DM_OTHER = 29 };
/* ring b: wr_ctl[b] cap, [b+1] write (published), [b+2] read (JS), [b+3] seq, [b+4] drops, [b+5] pending offset,
   [b+6] pending length (0 = none) */
static unsigned char *reserve(unsigned char *ring, int b, u32 len)
{
  u32 cap = wr_ctl[b], w = wr_ctl[b + 1], r = wr_ctl[b + 2];
  u32 off = w & (cap - 1);
  u32 pad = (off + len > cap) ? cap - off : 0;
  if ((w - r) + pad + len > cap) {
    wr_ctl[b + 4]++;
    wr_ctl[b + 6] = 0;
    return 0;
  }
  if (pad) {
    *(u32 *) (ring + off) = pad;    /* type 0: padding to the ring's end */
    wr_ctl[b + 1] = w + pad;
    off = 0;
  }
  wr_ctl[b + 5] = off;
  wr_ctl[b + 6] = len;
  return ring + off;
}
static void publish(int b)
{
  u32 len = wr_ctl[b + 6];
  wr_ctl[b + 6] = 0;
  wr_ctl[b + 1] = wr_ctl[b + 1] + len;
}
static int in_data(u32 p) { return p >= wr_ctl[D_LO] && p + 0x20 <= wr_ctl[D_HI]; }
static void copy_bytes(u32 *dst, const unsigned char *src, u32 k)
{
  unsigned char *d = (unsigned char *) dst;
  u32 i;
  for (i = 0; i < k; i++) d[i] = src[i];
  for (; i & 3; i++) d[i] = 0;
}
static void note_font(u32 h)
{
  u32 n = wr_fonts[0], i;
  for (i = 0; i < n; i++)
    if (wr_fonts[1 + i] == h) return;
  if (n < 511) {
    wr_fonts[1 + n] = h;
    wr_fonts[0] = n + 1;
  }
}
/* Font::textWidth record (type 1), u32 words: 0 header, 1 seq, 2 font, 3 owner [font+4], 4 [font+0x14], 5 hfont
   [font+0x18], 6 s, 7 n, 8 return address, 9 result, 10 Text_put seq at entry, 11 bytes copied, 12 [owner+8],
   13 [owner+0xc]; then the bytes, zero-padded to 4 */
void tw_enter(GumInvocationContext *ic)
{
  u32 *f, *r, s, k, len;
  int n;
  if (!wr_ctl[C_ON]) return;
  if (gum_invocation_context_get_thread_id(ic) != wr_ctl[C_MAIN]) {
    wr_ctl[TW_OTHER]++;
    return;
  }
  f = (u32 *) ic->cpu_context->ecx;
  s = (u32) gum_invocation_context_get_nth_argument(ic, 0);
  n = (int) (u32) gum_invocation_context_get_nth_argument(ic, 1);
  k = (s != 0 && n > 0) ? (u32) n : 0;
  if (k > wr_ctl[MAXCOPY]) k = wr_ctl[MAXCOPY];
  len = 14 * 4 + ((k + 3) & ~3u);
  r = (u32 *) reserve(wr_tw, TW_CAP, len);
  if (!r) return;
  r[0] = (1u << 24) | len;
  r[1] = wr_ctl[TW_SEQ]++;
  r[2] = (u32) f;
  r[3] = f[1];
  r[4] = f[5];
  r[5] = f[6];
  r[6] = s;
  r[7] = (u32) n;
  r[8] = (u32) gum_invocation_context_get_return_address(ic);
  r[9] = 0xcccccccc;
  r[10] = wr_ctl[TP_SEQ];
  r[11] = k;
  r[12] = 0xffffffff;
  r[13] = 0xffffffff;
  if (f[1] >= wr_ctl[D_LO] && f[1] + 0x1c <= wr_ctl[D_HI]) {
    r[12] = ((u32 *) f[1])[2];
    r[13] = ((u32 *) f[1])[3];
  }
  copy_bytes(r + 14, (const unsigned char *) s, k);
  note_font(f[6]);
}
void tw_leave(GumInvocationContext *ic)
{
  u32 *r;
  if (wr_ctl[TW_CAP + 6] == 0) return;
  if (gum_invocation_context_get_thread_id(ic) != wr_ctl[C_MAIN]) return;
  r = (u32 *) (wr_tw + wr_ctl[TW_CAP + 5]);
  r[9] = (u32) gum_invocation_context_get_return_value(ic);
  publish(TW_CAP);
}
/* Text_put record (type 2) and drawMarkupRun record (type 3, own ring), u32 words: 0 header, 1 seq, 2 draw context, 3 text, 4 return address, 5 textWidth seq at
   entry, 6 textWidth seq at exit, 7 result, 8..16 context +0x38 +0x3c +0x40 +0x4c +0x54 +0x5c +0x60 +0x64 +0x68 before,
   17 [g_primarySurface], 18 handle word [[+0x5c]+4] (0xfffffffe when +0x5c is not a .data FontRef), 19..22 +0x38 +0x3c
   +0x4c +0x5c after, 23 strlen(text), 24 bytes copied, 25 the run length (drawMarkupRun's len; strlen for Text_put);
   then the bytes, zero-padded to 4 */
static void run_enter(GumInvocationContext *ic, unsigned char *ring, int b, u32 type)
{
  u32 *c, *r, text, sl = 0, k, len;
  if (!wr_ctl[C_ON]) return;
  if (gum_invocation_context_get_thread_id(ic) != wr_ctl[C_MAIN]) {
    wr_ctl[b + 7]++;
    return;
  }
  c = (u32 *) ic->cpu_context->ecx;
  text = (u32) gum_invocation_context_get_nth_argument(ic, 0);
  if (text) while (((const unsigned char *) text)[sl]) sl++;
  k = sl > wr_ctl[MAXCOPY] ? wr_ctl[MAXCOPY] : sl;
  len = 26 * 4 + ((k + 3) & ~3u);
  r = (u32 *) reserve(ring, b, len);
  if (!r) return;
  r[0] = (type << 24) | len;
  r[1] = wr_ctl[b + 3]++;
  r[2] = (u32) c;
  r[3] = text;
  r[4] = (u32) gum_invocation_context_get_return_address(ic);
  r[5] = wr_ctl[TW_SEQ];
  r[6] = 0xffffffff;
  r[7] = 0xcccccccc;
  r[8] = c[0x38 / 4];
  r[9] = c[0x3c / 4];
  r[10] = c[0x40 / 4];
  r[11] = c[0x4c / 4];
  r[12] = c[0x54 / 4];
  r[13] = c[0x5c / 4];
  r[14] = c[0x60 / 4];
  r[15] = c[0x64 / 4];
  r[16] = c[0x68 / 4];
  r[17] = *(volatile u32 *) wr_ctl[PRIMARY];
  r[18] = in_data(c[0x5c / 4]) ? ((u32 *) c[0x5c / 4])[1] : 0xfffffffe;
  r[19] = r[20] = r[21] = r[22] = 0xffffffff;
  r[23] = sl;
  r[24] = k;
  r[25] = type == 3 ? (u32) gum_invocation_context_get_nth_argument(ic, 1) : sl;
  copy_bytes(r + 26, (const unsigned char *) text, k);
}
static void run_leave(GumInvocationContext *ic, unsigned char *ring, int b)
{
  u32 *r, *c;
  if (wr_ctl[b + 6] == 0) return;
  if (gum_invocation_context_get_thread_id(ic) != wr_ctl[C_MAIN]) return;
  r = (u32 *) (ring + wr_ctl[b + 5]);
  c = (u32 *) r[2];
  r[6] = wr_ctl[TW_SEQ];
  r[7] = (u32) gum_invocation_context_get_return_value(ic);
  r[19] = c[0x38 / 4];
  r[20] = c[0x3c / 4];
  r[21] = c[0x4c / 4];
  r[22] = c[0x5c / 4];
  publish(b);
}
void tp_enter(GumInvocationContext *ic) { run_enter(ic, wr_tp, TP_CAP, 2); }
void tp_leave(GumInvocationContext *ic) { run_leave(ic, wr_tp, TP_CAP); }
void dm_enter(GumInvocationContext *ic) { run_enter(ic, wr_dm, DM_CAP, 3); }
void dm_leave(GumInvocationContext *ic) { run_leave(ic, wr_dm, DM_CAP); }
`;
const wrCm = K(new CModule(WR_SRC, { wr_ctl: wrCtl, wr_tw: wrTw, wr_tp: wrTp, wr_dm: wrDm, wr_fonts: wrFonts }));
globalThis.__widthsRecCm = wrCm;          // keep the module referenced: Frida frees an unreferenced CModule's code

const wrSeq = { tw: 0, tp: 0, dm: 0 };
const wrStats = { chunks: 0, bytes: 0, maxBacklog: 0 };
function wrDrainRing(name, wIdx, rIdx, ring) {
  const w = wrGet(wIdx), r = wrGet(rIdx);
  const n = (w - r) >>> 0;
  if (!n) return;
  if (n > wrStats.maxBacklog) wrStats.maxBacklog = n;
  const off = r & (WR_CAP - 1);
  const a = Math.min(n, WR_CAP - off);
  let buf;
  if (a === n) buf = ring.add(off).readByteArray(n);
  else {
    const u = new Uint8Array(n);
    u.set(new Uint8Array(ring.add(off).readByteArray(a)), 0);
    u.set(new Uint8Array(ring.readByteArray(n - a)), a);
    buf = u.buffer;
  }
  wrSet(rIdx, w);
  wrStats.chunks++; wrStats.bytes += n;
  send({ type: 'wrec', ring: name, seq: wrSeq[name]++ }, buf);
}
// LOGFONTA of each HFONT, queried on the JS thread soon after the font's first use (the game keeps its fonts for the
// whole run; Font::create 0x100688ec deletes an HFONT only when the same Font is created again)
let wrGetObjectA = null;
const wrLogfonts = [];
function wrQueryFonts() {
  if (!wrGetObjectA) wrGetObjectA = new NativeFunction(Process.getModuleByName('gdi32.dll').getExportByName('GetObjectA'), 'int', ['pointer', 'int', 'pointer'], 'stdcall');
  const n = wrFonts.readU32();
  const buf = Memory.alloc(64);
  for (let i = wrLogfonts.length; i < n; i++) {
    const h = wrFonts.add(4 + 4 * i).readU32();
    for (let k = 0; k < 64; k += 4) buf.add(k).writeU32(0);
    const got = wrGetObjectA(ptr(h), 60, buf);
    wrLogfonts.push({ hfont: h, got, logfont: Array.from(new Uint8Array(buf.readByteArray(60))) });
  }
}
function wrDrain() {
  try {
    wrDrainRing('tw', WI.TW_W, WI.TW_R, wrTw);
    wrDrainRing('tp', WI.TP_W, WI.TP_R, wrTp);
    wrDrainRing('dm', WI.DM_W, WI.DM_R, wrDm);
    wrQueryFonts();
  } catch (e) { send({ type: 'error', where: 'widths drain', msg: String(e && e.stack || e) }); }
}
const wrTimer = setInterval(wrDrain, WR_MS);

function wrHookJgld(m) {
  Interceptor.attach(m.base.add(0x68e60), { onEnter: wrCm.tw_enter, onLeave: wrCm.tw_leave });
}
Interceptor.attach(ptr(0x00477250), { onEnter: wrCm.tp_enter, onLeave: wrCm.tp_leave });
Interceptor.attach(ptr(0x00477280), { onEnter: wrCm.dm_enter, onLeave: wrCm.dm_leave });
{
  const j = Process.findModuleByName('jgld.dll');
  if (j) wrHookJgld(j);
}
Process.attachModuleObserver({ onAdded(m) {
  try {
    if (m.name.toLowerCase() === 'jgld.dll') wrHookJgld(m);
  } catch (e) { send({ type: 'error', where: 'widths patch ' + m.name, msg: String(e && e.stack || e) }); }
} });

let wrEnded = false;
const _wrTrEnd = trEnd;
trEnd = function () {
  if (!wrEnded) {
    wrEnded = true;
    wrSet(WI.ON, 0);
    clearInterval(wrTimer);
    wrDrain();
    send({ type: 'wrec_end', tw: { seq: wrGet(WI.TW_SEQ), drops: wrGet(WI.TW_DROP), other: wrGet(WI.TW_OTHER), pending: wrGet(WI.TW_PLEN) },
           tp: { seq: wrGet(WI.TP_SEQ), drops: wrGet(WI.TP_DROP), other: wrGet(WI.TP_OTHER), pending: wrGet(WI.TP_PLEN) },
           dm: { seq: wrGet(WI.DM_SEQ), drops: wrGet(WI.DM_DROP), other: wrGet(WI.DM_OTHER), pending: wrGet(WI.DM_PLEN) },
           chunks: wrSeq, stats: wrStats, fonts: wrLogfonts });
  }
  return _wrTrEnd();
};
send({ type: 'wrec_ready' });
