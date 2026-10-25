"""S1.6 combined shared-file edits: S1.5's tick.cpp replacements (log/roadmap/S1/5/patches.py) first, then S1.4's
log/roadmap/S1/4_load_pump.diff as replacements, then the comment-only edits of 5_merge.md sections 2 and 3.
Each `old` must occur exactly once (in the text left by the edits before it). Apply order is the list order."""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "5"))
import patches as p5  # noqa: E402

S14_TBATCH_OLD = "  bool body = false;\n  std::vector<Command> cmds;\n};\n"
S14_TBATCH_NEW = """  bool body = false;
  // U-1051: the next clock read after the dispatch's handlers is jgld's Palette::update QPC although the pump before it was
  // not one of the frame body's (the last clock site was a wait loop's): a pumpInput483bd0 of a world load
  // (loadFlic +0x54: the 15th of the load in cov_r_flic_input) took it, a message posted before the load started; the harness's
  // polls ride on timeGetTime reads and the load makes none
  bool load = false;
  std::vector<Command> cmds;
};
"""
# S1.5 put focus_resolve / focus_flush before `ctx_ra = ra`; S1.4's line goes after them (it only touches `cur->load`)
S14_CLOCK_OLD = "        focus_flush();\n        ctx_ra = ra;\n        open_batch = false;\n"
S14_CLOCK_NEW = ("        focus_flush();\n"
                 "        if (open_batch && cur && !cur->body && sub == 2 && ra == kPaletteQpcRa) cur->load = true;   // U-1051\n"
                 "        ctx_ra = ra;\n        open_batch = false;\n")
S14_BATCH_OLD = ("    if (io.pump_site == kPumpSiteModal ? (kind != PumpKind::Blocking || b.body) : !b.body) return false;\n")
S14_BATCH_NEW = """    if (kind == PumpKind::Load) {  // loadFlic's pump (U-1051): the dispatch the trace puts before the load's first palette QPC
      if (b.body || !b.load || SG_NEGCTL(g_negctl_frame) == 8) return false;
    } else if (io.pump_site == kPumpSiteModal ? (kind != PumpKind::Blocking || b.body) : !b.body) {
      return false;
    }
"""

PATCHES = {
    "src/sim/tick.cpp": list(p5.PATCHES["src/sim/tick.cpp"]) + [
        (S14_TBATCH_OLD, S14_TBATCH_NEW), (S14_CLOCK_OLD, S14_CLOCK_NEW), (S14_BATCH_OLD, S14_BATCH_NEW)],
    "include/portsim/sim/commands.h": [
        ("// What kind of pump asks (see MODAL BLOCKS).\nenum class PumpKind : uint32_t { Blocking = 1, Drain = 2 };\n",
         "// What kind of pump asks (see MODAL BLOCKS).\n"
         "//   PumpKind::Load      loadFlic's pumpInput483bd0 (U-1051): a Drain that also takes the one dispatch a trace puts before\n"
         "//                       a world load's first palette QPC read (a message posted before the load began)\n"
         "enum class PumpKind : uint32_t { Blocking = 1, Drain = 2, Load = 3 };\n"),
        ("  // 8 onChar, 9 onChar0, 10 onPress1b, 11 onPress2b; trace_schema.md HANDLER).\n",
         "  // 8 onChar, 9 onChar0, 10 onPress1b, 11 onPress2b; trace_schema.md HANDLER; 12 a WM_KEYDOWN and 13 a WM_CHAR that a\n"
         "  // focused child window took before any root handler (tick.cpp parse_trace; a0 = wParam; apply_command does nothing\n"
         "  // with them, the bio entry in chareditor.cpp gives them to its EditBox)).\n"),
    ],
    "src/sim/commands_internal.h": [
        ("void pump_messages(Ctx &ctx, PumpKind kind, const char *where);\n",
         "void pump_messages(Ctx &ctx, PumpKind kind, const char *where);\n"
         "void pump_load(Ctx &ctx);   // loadFlic +0x54: pumpInput483bd0, whose dispatch is a Load pump (U-1051)\n"),
    ],
    "src/sim/commands.cpp": [
        ("void pump_light(Ctx &ctx, const char *where) {\n",
         """// pumpInput483bd0 in loadFlic (+0x54): the display's showScreen loop dispatches what has arrived; no click / button words
// are touched (pumpMessages' are not this pump's)
void pump_load(Ctx &ctx) {
  InputSource *src = input_source(ctx);
  std::vector<Command> batch;
  if (src && src->batch(PumpKind::Load, batch))
    for (const Command &c : batch) apply_command(ctx, c);
}

void pump_light(Ctx &ctx, const char *where) {
"""),
    ],
    "src/sim/menu.cpp": [
        ("// pumpInput483bd0 (+0x54: a pump of a modal site, the port's takes nothing, U-1052). A delay of -1 loads one frame\n",
         "// pumpInput483bd0 (+0x54: a Load pump, U-1051: it takes the one dispatch a trace puts before the load's first palette QPC). A delay of -1 loads one frame\n"),
        ("  if (c % 100 == 0 && gs.g_difficulty == -1) cm::call_peer(ctx, \"showLoadingScreen\", {static_cast<uint32_t>(c / 4)});\n",
         "  if (c % 100 == 0 && gs.g_difficulty == -1) cm::call_peer(ctx, \"showLoadingScreen\", {static_cast<uint32_t>(c / 4)});\n"
         "  cm::pump_load(ctx);   // +0x54 pumpInput483bd0\n"),
    ],
    "include/portsim/sim/chareditor.h": [
        ("// Drawing, sound and the screen Image are dropped. The bio entry (an EditBox in the window tree) is not ported: a press on\n"
         "// it throws MissingResource before any of its writes.\n",
         "// Drawing, sound and the screen Image are dropped. The bio entry (control 20, non-members) edits the record's bio through\n"
         "// the EditBox family registered by this module (EditBox::create / setText / resizeBuffer / insertChar / onKey /\n"
         "// resetFields / releaseState; roadmap S1.5); keys the window tree gives the EditBox arrive as RootHandler commands 12 / 13\n"
         "// (commands.h). Up / Down need the font's line wrap and throw MissingResource (U-1190).\n"),
    ],
}


def names():
    return sorted(PATCHES)


def apply(rel: str, data: bytes) -> bytes:
    if rel not in PATCHES:
        return data
    s = data.decode("utf-8")
    nl = "\r\n" if "\r\n" in s else "\n"
    s = s.replace("\r\n", "\n")
    for old, new in PATCHES[rel]:
        if s.count(old) != 1:
            raise SystemExit(f"patch for {rel}: expected one occurrence of {old[:70]!r}, found {s.count(old)}")
        s = s.replace(old, new)
    return s.replace("\n", nl).encode("utf-8")
