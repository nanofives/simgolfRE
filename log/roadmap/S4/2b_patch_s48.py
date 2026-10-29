"""S4.2b: migrate S4.8's modules onto the core (one-time patch; the results are the deliverables).

  * audio_dev.cpp: the shared state (DevCtx, SndLocals, the frame block, the pending entry esp, the control word) and the
    configuration entries audio_dev.fpcw / audio_dev.esp removed: the stack is audio.h's SndFrame, the word is the x87 unit's;
  * audio_ds.cpp: SndLocals -> SndFrame; DsBuffer::setVolume on the x87 unit (its fild / fmul / __ftol / fldln2 / fxch /
    fyl2x in the original's order, __ftol through the dispatcher), the module's own soft 80-bit model removed;
  * audio_dev.h: only snd_ds_error is left.

    ~/bin/py -3.12 log/roadmap/S4/2b_patch_s48.py [--check]
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
SIM = ROOT / "port" / "src" / "sim"
HDR = ROOT / "port" / "include" / "portsim" / "sim" / "audio_dev.h"


class Src:
    def __init__(self, p):
        self.p = p
        self.t = p.read_text(encoding="utf-8").replace("\r\n", "\n")

    def rep(self, old, new, n=1):
        assert self.t.count(old) == n, (self.p.name, old[:80], self.t.count(old))
        self.t = self.t.replace(old, new)

    def cut(self, start, end, repl=""):
        i = self.t.index(start)
        j = self.t.index(end, i)
        self.t = self.t[:i] + repl + self.t[j:]


dev = Src(SIM / "audio_dev.cpp")
dev.rep("""// timer clients, the sound thread and window, and the GetProcAddress exports; plus the frame block and control-word state the
// two S4.8 modules share (portsim/sim/audio_dev.h).""", """// timer clients, the sound thread and window, and the GetProcAddress exports. Locals passed by address live on the audio core's
// stack (SndFrame, portsim/sim/audio.h; S4.8's SndLocals until S4.2b).""")
dev.cut("// ================================================================================================ shared state (audio_dev.h)\n",
        "namespace {\nusing u32 = uint32_t;\n")
dev.cut('// configuration entries: "audio_dev.fpcw <word>", "audio_dev.esp <esp>" (the replay harness, 8_replay.py)\n',
        "#define G(v) &g_negctl_##v\n")
dev.rep('    {"audio_dev.fpcw", "default", "uint32", 1, 0, a_cfg_fpcw, nullptr},\n'
        '    {"audio_dev.esp", "default", "uint32", 1, 0, a_cfg_esp, nullptr},\n', "")
dev.rep("  SndLocals l(c, 0x1c);", "  SndFrame l(c, 0x1c);", 2)
for inc in ("#include <map>\n", "#include <memory>\n", "#include <mutex>\n", "#include <vector>\n"):
    dev.rep(inc, "")

ds = Src(SIM / "audio_ds.cpp")
ds.rep("""// with no host the AudioSink receives them as commands (portsim/sim/audio_sink.h). Locals the original passes by address live
// at the original's stack offsets (SndLocals, portsim/sim/audio_dev.h).""", """// with no host the AudioSink receives them as commands (portsim/sim/audio_sink.h). Locals the original passes by address live
// at the original's stack offsets (SndFrame, portsim/sim/audio.h; S4.8's SndLocals until S4.2b).""")
ds.cut("// ================================================================================================ the x87 model (setVolume)\n",
       "// ================================================================================================ DsBuffer\n")
for k in ("8", "0x10", "0xc", "4", "0x28"):
    n = ds.t.count(f"  SndLocals l(c, {k});")
    assert n >= 1, k
    ds.t = ds.t.replace(f"  SndLocals l(c, {k});", f"  SndFrame l(c, {k});")
assert "SndLocals" not in ds.t.replace("S4.8's SndLocals", "")
ds.rep("""// DsBuffer::setVolume (rva 0xf1c0, c3fh DsSetVolume_re): v = min(volume, 0x7f) (unsigned, 0xf1ca); m = the master's +0x188
// (master at +0x5c) or 0x7f; e = __ftol(m * k * (v * k) * 127.0) with k the double at rva 0x5b488 (1/127) and 127.0 at
// 0x5b478; e != 0: e = __ftol(ln(e * k) * 10.0 * 100.0) (0x5b468, 0x5b458; fldln2 / fyl2x 0xf23b-0xf23f), else e = -10000
// (0xf254). +0xc4 = e; with +0x60 non-NULL SetVolume (slot +0x3c) with e and its result is returned, else e (eax).
// neg 1: the silent volume is -9999 instead of -10000 (0xf254). neg 2: every step at 0x027f whatever the caller's word.
// neg 3: the first product alone at 0x027f (blind on the 30 vectors: see 8_notes.md).
void a_DsBuffer_setVolume(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  u32 vol = a[1];
  if (vol > 0x7f) vol = 0x7f;
  const u32 master = ld<u32>(c, self + 0x5c);
  const u32 m = master ? ld<u32>(c, master + 0x188) : 0x7f;
  const uint32_t cw = NEG(DsBuffer_setVolume) == 2 ? 0x027fu : snd_fpcw(c);
  const uint32_t cw1 = NEG(DsBuffer_setVolume) == 3 ? 0x027fu : cw;
  const X80 k = x_from_double(ld<double>(c, snd_va(c, 0x5b488)));
  X80 x = x_mul(x_from_u64(m), k, cw1);
  const X80 y = x_mul(x_from_u64(vol), k, cw);
  x = x_mul(x, y, cw);
  x = x_mul(x, x_from_double(ld<double>(c, snd_va(c, 0x5b478))), cw);
  uint32_t e = x_ftol(x);
  if (e) {
    X80 z = x_mul(x_from_u64(e), k, cw);
    z = x_ln_fyl2x(z, cw);
    z = x_mul(z, x_from_double(ld<double>(c, snd_va(c, 0x5b468))), cw);
    z = x_mul(z, x_from_double(ld<double>(c, snd_va(c, 0x5b458))), cw);
    e = x_ftol(z);
  } else {
    e = NEG(DsBuffer_setVolume) == 1 ? 0xffffd8f1u : 0xffffd8f0u;
  }""", """// DsBuffer::setVolume (rva 0xf1c0, c3fh DsSetVolume_re): v = min(volume, 0x7f) (unsigned, 0xf1ca); m = the master's +0x188
// (master at +0x5c) or 0x7f; e = __ftol(m * k * (v * k) * 127.0) with k the double at rva 0x5b488 (1/127) and 127.0 at
// 0x5b478; e != 0: e = __ftol(ln(e * k) * 10.0 * 100.0) (0x5b468, 0x5b458; fldln2 / fyl2x 0xf23b-0xf23f), else e = -10000
// (0xf254). +0xc4 = e; with +0x60 non-NULL SetVolume (slot +0x3c) with e and its result is returned, else e (eax).
// The x87 instructions on the core's unit under the caller's word (S4.2b): fild qword m (0xf1f4, the high dword 0), fmul
// qword k (0xf204), fild qword v (0xf20a), fmul k (0xf20e), fmulp st(1) (0xf214), fmul qword 127.0 (0xf216), __ftol (0xf21c,
// through the dispatcher: it pops); fild qword e (0xf231), fmul k (0xf235), fldln2 (0xf23b), fxch st(1) (0xf23d), fyl2x
// (0xf23f: the reference emulator's, portsim/sim/audio.h), fmul 10.0 (0xf241), fmul 100.0 (0xf247), __ftol (0xf24d).
// neg 1: the silent volume is -9999 instead of -10000 (0xf254). neg 2: every step at 0x027f whatever the caller's word.
// neg 3: the first product alone at 0x027f (blind on the 30 vectors: see 8_notes.md).
void a_DsBuffer_setVolume(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  u32 vol = a[1];
  if (vol > 0x7f) vol = 0x7f;
  const u32 master = ld<u32>(c, self + 0x5c);
  const u32 m = master ? ld<u32>(c, master + 0x188) : 0x7f;
  x87::Unit &u = x87::unit(c);
  const int neg = NEG(DsBuffer_setVolume);
  const uint16_t cw = neg == 2 ? 0x027fu : u.cw;
  const uint16_t cw1 = neg == 3 ? 0x027fu : cw;
  auto rd = [&](u32 rva) { return x87::from_f64(ld<uint64_t>(c, snd_va(c, rva))); };
  uint32_t e = 0;
  SG_X87_GUARD(c, {
    u.push(x87::fild(static_cast<int64_t>(m)));
    u.st(0) = x87::mul(u.st(0), rd(0x5b488), cw1);
    u.push(x87::fild(static_cast<int64_t>(vol)));
    u.st(0) = x87::mul(u.st(0), rd(0x5b488), cw);
    u.st(1) = x87::mul(u.st(1), u.st(0), cw);
    u.pop();
    u.st(0) = x87::mul(u.st(0), rd(0x5b478), cw);
  })
  e = snd_ftol(c).eax;
  if (e) {
    SG_X87_GUARD(c, {
      u.push(x87::fild(static_cast<int64_t>(e)));
      u.st(0) = x87::mul(u.st(0), rd(0x5b488), cw);
      u.push(x87::ln2());
      u.fxch(1);
      u.st(1) = x87::fyl2x(u.st(0), u.st(1), cw);
      u.pop();
      u.st(0) = x87::mul(u.st(0), rd(0x5b468), cw);
      u.st(0) = x87::mul(u.st(0), rd(0x5b458), cw);
    })
    e = snd_ftol(c).eax;
  } else {
    e = neg == 1 ? 0xffffd8f1u : 0xffffd8f0u;
  }""")
ds.rep("#include <cmath>\n", "")

hdr = '''// Step S4.8 (log/roadmap/S4/8_notes.md): sound.dll's DirectSound wrappers (module audio_ds) and its devices, Ctrl objects,
// command queues, sound timer clients, sound thread and window and the GetProcAddress exports (module audio_dev).
//
// Both modules are written against portsim/sim/audio.h. Since S4.2b the locals the original passes by address (S4.8's
// SndLocals) are audio.h's SndFrame on the core's stack, and the caller's x87 control word (S4.8's snd_fpcw) is the core's
// x87 unit's word (x87::unit(ctx).cw). What is left here:
#ifndef PORTSIM_SIM_AUDIO_DEV_H
#define PORTSIM_SIM_AUDIO_DEV_H

#include <cstdint>

namespace portsim {

// The DirectSound / COM result -> sound.dll error code map that mapDsError (rva 0xf480) and the inline copies in
// DsBuffer::setPrimaryFormat and DsBuffer::create compute (see audio_ds.cpp, mapDsError).
uint32_t snd_ds_error(uint32_t hr);

}  // namespace portsim

#endif  // PORTSIM_SIM_AUDIO_DEV_H
'''
if "--check" in sys.argv:
    print("ok (not written)")
else:
    dev.p.write_text(dev.t, encoding="utf-8")
    ds.p.write_text(ds.t, encoding="utf-8")
    HDR.write_text(hdr, encoding="utf-8")
    print("wrote audio_dev.cpp, audio_ds.cpp, audio_dev.h")
