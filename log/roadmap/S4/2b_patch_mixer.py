"""S4.2b: migrate port/src/sim/audio_mixer.cpp onto the core (one-time patch; the result is the deliverable).

  * part 1 (the x87 unit, its backends, the side table, frame_push / frame_pop) moved to audio_core.cpp part 0: removed here;
  * SG_X87_GUARD is audio.h's;
  * the three by-address locals at the original's stack offsets (audio.h SndFrame) instead of S4.4's frame block;
  * __ftol and the harness entries (x87load / x87save / x80soft / x80batch / report) moved to audio_core: removed here.

    ~/bin/py -3.12 log/roadmap/S4/2b_patch_mixer.py [--check]
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
P = ROOT / "port" / "src" / "sim" / "audio_mixer.cpp"
t = P.read_text(encoding="utf-8").replace("\r\n", "\n")


def cut(start: str, end: str, keep_end=True, repl=""):
    global t
    i = t.index(start)
    j = t.index(end, i)
    t = t[:i] + repl + (t[j:] if keep_end else t[j + len(end):])


def rep(old: str, new: str, n: int = 1):
    global t
    assert t.count(old) == n, (old[:80], t.count(old))
    t = t.replace(old, new)


# header comment
rep("""// sound.dll's mixer and channel renderers (step S4.4, log/roadmap/S4/4_notes.md; API portsim/sim/audio_mixer.h) and the
// virtual x87 unit they run on.
//
// Part 1: the unit (per-context side table, image I/O), the operation backends (Win32: the x87 instruction under the
// unit's control word; x64 and audio_mixer.x80soft: audio_x80.inc), sound.dll's __ftol, the harness entries.
// Part 2: the functions: the integer mixers, the x87 resamplers, the MMX renderers, the mixer
// objects, the channel renderers.
""", """// sound.dll's mixer and channel renderers (step S4.4, log/roadmap/S4/4_notes.md; header portsim/sim/audio_mixer.h): the
// integer mixers, the x87 resamplers, the MMX renderers, the mixer objects, the channel renderers. They compute on the audio
// core's virtual x87 unit (portsim/sim/audio.h "the x87 unit"; S4.4 built it here, S4.2b moved it to audio_core.cpp with
// sound.dll's __ftol and the harness entries) and keep their by-address locals on the core's stack (SndFrame).
""")
# part 1 out
cut("namespace portsim {\nnamespace x87 {\n\n#include \"audio_x80.inc\"",
    "// ================================================================================================ part 2: the functions\n")
rep("// ================================================================================================ part 2: the functions\n",
    "// ================================================================================================ the functions\n")
cut("// A body's x87 stop (X87Unmodelled) is kept as the context's detail (audio_mixer.report) and the call stops.\n#define SG_X87_GUARD",
    "// <<< S4.4 bodies")
# the Frame class
cut("class Frame {\n", "// renderChannel 0x35990")
rep("""// The channel renderers. Every x87 instruction is one operation on the unit (its register stack included: a callee's
// leftovers stay below what these push). Frame locals the original passes to a callee by address live in the frame block
// (x87::frame_push), at the same addresses in the oracle's emulator and in the port.""",
    """// The channel renderers. Every x87 instruction is one operation on the unit (its register stack included: a callee's
// leftovers stay below what these push). Locals the original passes to a callee by address live on the core's stack at the
// original's offsets (SndFrame, portsim/sim/audio.h).""")
# renderChannelSingle: push ecx + four pushes (0x36280..0x36284) = 0x14; the cursor is the caller's second argument slot
# (entry esp + 8: `mov [esp + 0x1c], ecx` at 0x36318 with esp = entry - 0x14; `lea edx, [esp + 0x24]` at 0x3632f after two
# pushes), written and passed to the mixers there
rep("""u32 renderChannelSingle(Ctx &c, u32 s, u32 a0, u32 a1, u32 a2, u32 a3) {
  if (a1 > 0x7d0) return 0x22;""", """u32 renderChannelSingle(Ctx &c, u32 s, u32 a0, u32 a1, u32 a2, u32 a3) {
  SndFrame fr(c, 0x14);           // push ecx; push ebx; push ebp; push esi; push edi (0x36280..0x36284)
  const u32 cursor = fr.arg(1);   // the caller's count slot, reused for the sample cursor (0x36318, `lea` at 0x3632f)
  if (a1 > 0x7d0) return 0x22;""")
rep("""  Frame fr(c, 4);
  st<u32>(c, fr.va, ld<u32>(c, ld<u32>(c, s + 8) + 0x1c));
  u32 rva;""", """  st<u32>(c, cursor, ld<u32>(c, ld<u32>(c, s + 8) + 0x1c));
  u32 rva;""")
rep("  st<u32>(c, a3, snd_call(c, rva, SndAbi::Cdecl, {a0, fr.va, a1, s, 1}).eax);",
    "  st<u32>(c, a3, snd_call(c, rva, SndAbi::Cdecl, {a0, cursor, a1, s, 1}).eax);")
rep("  st<u32>(c, ld<u32>(c, s + 8) + 0x1c, ld<u32>(c, fr.va));", "  st<u32>(c, ld<u32>(c, s + 8) + 0x1c, ld<u32>(c, cursor));")
# renderChannelStreamed: sub esp, 0x2c + four pushes (0x36660..0x3666a) = 0x3c; zeroed at 0x36675..0x36689; getPosition's
# (want, off) at entry - 0x24 / - 8 (0x366b6..0x366c0), restore's (p1, n1, p2, n2) at entry - 0xc / - 0x10 / - 0x14 / - 0x18
# (0x36704..0x3671d)
rep("""  Frame fr(c, 0x18);
  const u32 pWant = fr.va, pOff = fr.va + 4, pP1 = fr.va + 8, pN1 = fr.va + 12, pP2 = fr.va + 16, pN2 = fr.va + 20;
  for (u32 k = 0; k < 0x18; k += 4) st<u32>(c, fr.va + k, 0);""",
    """  SndFrame fr(c, 0x3c);           // sub esp, 0x2c; push ebx; push ebp; push esi; push edi (0x36660..0x3666a)
  const u32 pWant = fr.at(0x24), pOff = fr.at(8);                                  // 0x366b6..0x366c0
  const u32 pP1 = fr.at(0xc), pN1 = fr.at(0x10), pP2 = fr.at(0x14), pN2 = fr.at(0x18);  // 0x36704..0x3671d
  for (u32 p : {pWant, pOff, pN2, pP2, pN1, pP1}) st<u32>(c, p, 0);                // 0x36675..0x36689""")
# renderChannelVoice: push ebp; mov ebp, esp; and esp, -8; sub esp, 0x1c; three pushes (0x371d0..0x371e0): esp =
# ((entry - 4) & ~7) - 0x28; the cursor [esp + 0x10] (0x371ea), passed as `lea ecx, [esp + 0x18]` after two pushes (0x37282)
rep("""  Frame fr(c, 4);
  st<u32>(c, fr.va, ld<u32>(c, v + 0x1c));
  u32 rendered = 0;""", """  SndFrame fr(c, 0);
  const u32 aligned = (fr.entry() - 4u) & ~7u;    // ebp = entry - 4; and esp, -8 (0x371d3)
  fr.set_esp(aligned - 0x28u);                    // sub esp, 0x1c; push ebx; push esi; push edi (0x371d6..0x371e0)
  const u32 cursor = aligned - 0x18u;             // [esp + 0x10] (0x371ea)
  st<u32>(c, cursor, ld<u32>(c, v + 0x1c));
  u32 rendered = 0;""")
rep("      st<u32>(c, out, snd_call(c, rva, SndAbi::Cdecl, {buf, fr.va, count, ch, 1}).eax);",
    "      st<u32>(c, out, snd_call(c, rva, SndAbi::Cdecl, {buf, cursor, count, ch, 1}).eax);")
rep("      rendered = snd_call(c, big ? 0x1f64 : 0x2617, SndAbi::Cdecl, {buf, fr.va, count, v, 0}).eax;",
    "      rendered = snd_call(c, big ? 0x1f64 : 0x2617, SndAbi::Cdecl, {buf, cursor, count, v, 0}).eax;")
rep("    st<u32>(c, v + 0x1c, ld<u32>(c, fr.va));", "    st<u32>(c, v + 0x1c, ld<u32>(c, cursor));")
# __ftol and the harness entries
cut("// ================================================================================================ sound.dll's __ftol\n",
    "int g_negctl_none_mix = 0;\n")
cut("int g_negctl_none_mix = 0;\nconst FuncDef kFuncs[] = {\n", "#define F(name, abi, ret, n)", repl="const FuncDef kFuncs[] = {\n")
cut("    // static CRT functions without a dispatchable hooks.csv row (S4.4 merge request: audio_crt_name)\n", "};\n\n}  // namespace\n")
if "--check" in sys.argv:
    print("ok (not written)")
else:
    P.write_text(t, encoding="utf-8")
    print(f"wrote {P}")
