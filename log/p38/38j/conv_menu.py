"""Phase 3.8 stream 38j: mainLoop's TITLE MENU and SESSION DISPATCH for the tick module, converted the way phase 3.7a converted
the frame (log/p3/p37a_notes.txt section 2): log/p3/p37a_conv.py over the C3 transcription shim/src/re/c3fy.cpp, then the
asserted hand edits below -> port/src/sim/tick_menu.inc (included inside struct Frame of tick.cpp, after tick_frame.inc).

Parts converted (c3fy.cpp, the line ranges are asserted by their first and last lines):
  - Setup's frame-slot stores (the title-button geometry and the label pointers, c3fy.cpp Setup ML+0xbd..ML+0x1f6) as
    MenuSeedSlots: Setup is boot code and runs once; it is the only writer of these slots (log/p38/38j/slotscan.py lists
    every write of F+0xf4..0x10b, F+0x150..0x16b, F+0x1f4..0x207 in mainLoop), so writing Setup's constants on every entry to
    the menu leaves them as the original has them at any time after boot. The port's frame block starts zeroed.
  - part MV, the title menu ML+0x11875..ML+0x12050 (Menu, MV_blitFull),
  - part SV's Session ML+0x43c..ML+0x664.
Every callee is called by its hooks.csv name (TkBase::call): a registered port function, a documented drop (tick.cpp kDrops),
a tick local (pumpInput483bd0, pumpMessages) or MissingResource{name}.

usage: ~/bin/py -3.12 log/p38/38j/conv_menu.py [--check]     (--check: compare with the file on disk, write nothing)
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
SRC = ROOT / "shim" / "src" / "re" / "c3fy.cpp"
OUT = ROOT / "port" / "src" / "sim" / "tick_menu.inc"
CONV = ROOT / "log" / "p3" / "p37a_conv.py"

# (first line, last line, the first line's text, the last line's text): the parts of c3fy.cpp
PARTS = {
    "seed": (203, 218, "    // title button geometry", '    FD(0x204) = 0x004C6C54;'),
    "menu": (279, 512, "namespace {", "}  // namespace"),
    "session": (524, 599, "namespace {", "}"),
}

# (old, new, count): exact replacements of the converted text, each asserted
EDITS = [
    # SV's clock helper is FramePreamble's (frame head); Session does not read the clock
    ("// timeGetTime through the IAT slot the original calls (never the import directly; the scenario harness redirects it)\n"
     "u32 SV_timeGetTime() { return reinterpret_cast<DWORD(WINAPI*)()>(GU32(CODE_4ba21c))(); }\n", "", 1),
    # the hotspot -> selection map, measured (log/p38/38j_notes.md section 3): the label pointers F+0x1f4 + 4 i (Setup) put
    # <Play a Championship> at hotspot 3 and <Sandbox Mode> at hotspot 4; the remap sends hotspot 3 to selection 2 and 4 to 3.
    # The c3fy comments called the selection-3 block the championship flags: it is the Sandbox Mode block (cash 100000,
    # flag 1 << 24, every building and amenity unlocked by Session), and flag 1 << 26 set for selection 2 is the
    # championship's (Session's startNewCourse: the course list and the pro, loadGameBrowserScreen / pickAProScreen)
    ("        // championship flags (ML+0x11e4c..ML+0x11e79)\n",
     "        // Sandbox Mode (hotspot 4, selection 3): cash, flag 1 << 24, unlocks (ML+0x11e4c..ML+0x11e79)\n", 1),
    ("    if (ebx == 2) {                  //                               ML+0x11f81 jne ML+0x11f9e\n",
     "    if (ebx == 2) {                  // Play a Championship (hotspot 3)  ML+0x11f81 jne ML+0x11f9e\n", 1),
    ("    if (flags & 0x4000000) {                        // ML+0x554 test eax,4000000; ML+0x563 je (NOT sandbox)\n",
     "    if (flags & 0x4000000) {                        // ML+0x554 test eax,4000000; ML+0x563 je (the championship flag)\n", 1),
    ("            G32(GS(g_flags)) &= 0xFBFFFFFF;          // ML+0x56e clear the sandbox bit\n",
     "            G32(GS(g_flags)) &= 0xFBFFFFFF;          // ML+0x56e clear the championship flag\n", 1),
    ("    // loadWorldAssets block ML+0x5a3 (reached by the sandbox-nonzero and mode==6 fall-throughs)\n",
     "    // loadWorldAssets block ML+0x5a3 (reached by the championship's startNewCourse != 0 and mode == 6 fall-throughs)\n", 1),
    # negative controls (the pseudo-function tick.menu, registered by module menu; modes listed in port/src/sim/menu.cpp)
    # mode 1 drops buildGolferBioText (dropping the menu's own initRecords is an equivalent mutant: buildGolferBioText ends
    # with initRecords, log/c3/c3fyk1_results.tsv mutant 154)
    ('    cc("buildGolferBioText");                    // buildGolferBioText()           ML+0x1189a (no pushed args)\n',
     '    if (menu_neg() != 1) cc("buildGolferBioText");   // buildGolferBioText()  ML+0x1189a (no pushed args)\n', 1),
    ("    GU32(GS(g_date)) = 0;              // g_date = 0                     ML+0x1188a\n",
     "    if (menu_neg() != 2) GU32(GS(g_date)) = 0;   // g_date = 0              ML+0x1188a\n", 1),
    ("        GU8(GS(g_courseType)) = GU8((GS(g_courseTable) + 32) + r * 46); // ML+0x5f5",
     "        GU8(GS(g_courseType)) = GU8((GS(g_courseTable) + 32) + (r + (menu_neg() == 3 ? 1 : 0)) * 46); // ML+0x5f5", 1),
    ("        G32(GS(g_buildingUnlockLevel)) = 6;                        // ML+0x627\n",
     "        G32(GS(g_buildingUnlockLevel)) = menu_neg() == 4 ? 7 : 6;  // ML+0x627\n", 1),
]

NEG = """
// The negative-control mode of the menu / session bodies: the pseudo-function tick.menu's switch (module menu registers it;
// modes in port/src/sim/menu.cpp). 0 without that module.
static int menu_neg() {
#if PORTSIM_NEGCTL
    static const int i = registry_find("tick.menu");
    const FuncDef *f = i >= 0 ? registry_at(i, nullptr) : nullptr;
    return f && f->negctl ? *f->negctl : 0;
#else
    return 0;
#endif
}
"""

HEADER = """\
// GENERATED by log/p38/38j/conv_menu.py from shim/src/re/c3fy.cpp (the C3 transcription of mainLoop: Setup's frame-slot
// stores, part MV the title menu, part SV's Session) and edited by the asserted edits of that script. Included inside struct
// Frame (tick.cpp) after tick_frame.inc. Citations: ML+0x<off> = mainLoop + offset, <object>+0x<off>, <function>.
// The menu's selections (hotspot -> ebx): 0 Continue Saved Game -> 1 (then 6), 1 Start New Game -> 0, 2 Select A Theme -> 4,
// 3 Play a Championship -> 2, 4 Sandbox Mode -> 3, 5 quit -> -2 (log/p38/38j_notes.md section 3).
"""

DRIVER = """
// MainLoopBody's loop (c3fy.cpp) from a frame's exit to the title menu (ML+0x11875) back to a frame head: Setup's slots, the
// menu (the selection in ebx), then Session. Session's sMenu goes back to the menu (ML+0x11875), sQuit is mainLoop's
// return (ML+0x544 ret), sFrame enters the frame at ML+0x664: rebuildTileCoverage, then the frame head ML+0x669, where the
// original's frame record is taken (so the rebuild is this step's and the next frame enters at the head).
FrameExit menu_session(u32 F) {
    for (;;) {
        MenuSeedSlots(F);
        const int sel = Menu(F);
        const int act = Session(F, sel);
        if (act == sQuit) return FrameExit::Quit;
        if (act == sFrame) {
            cc("rebuildTileCoverage");                 // ML+0x664
            return FrameExit::Frame;
        }
    }
}
"""


def conv(a, b):
    r = subprocess.run([sys.executable, str(CONV), str(SRC), str(a), str(b)], capture_output=True, text=True,
                       encoding="utf-8")
    if r.returncode:
        sys.exit(r.stderr)
    return r.stdout, r.stderr


def main():
    src = SRC.read_text(encoding="utf-8").splitlines()
    for name, (a, b, first, last) in PARTS.items():
        if not src[a - 1].startswith(first) or not src[b - 1].startswith(last):
            sys.exit(f"c3fy.cpp moved: part {name} lines {a}..{b} do not start {first!r} / end {last!r}")
    warn = []
    seed, w = conv(*PARTS["seed"][:2])
    warn += [x for x in w.splitlines() if x]
    menu, w = conv(*PARTS["menu"][:2])
    warn += [x for x in w.splitlines() if x]
    sess, w = conv(*PARTS["session"][:2])
    warn += [x for x in w.splitlines() if x]
    # the one literal the converter leaves is SV_timeGetTime's IAT slot, removed by the first edit
    if warn != ["529: literal 0x4ba21c (code:ba21c) left"]:
        sys.exit("converter warnings changed: %s" % warn)
    text = ("// Setup's stores of the title menu's frame slots (Setup ML+0xbd..ML+0x1f6), run on every entry to the menu: Setup is\n"
            "// their only writer (log/p38/38j/slotscan.py) and runs once at boot, before the first menu\n"
            "void MenuSeedSlots(u32 F) {\n" + seed + "}\n\n" + menu + "\n" + sess)
    for old, new, n in EDITS:
        got = text.count(old)
        if got != n:
            sys.exit("edit expected %d, found %d: %s" % (n, got, old[:90]))
        text = text.replace(old, new)
    for bad in ("CODE_", "WINAPI", "__asm", "reinterpret_cast", "DWORD"):
        if bad in text:
            sys.exit("left in the converted text: " + bad)
    out = HEADER + NEG + "\n" + text.rstrip("\n") + "\n" + DRIVER
    if "--check" in sys.argv:
        sys.exit(0 if OUT.exists() and OUT.read_text(encoding="utf-8") == out else "tick_menu.inc differs from the generator")
    OUT.write_text(out, encoding="utf-8", newline="\n")
    print("wrote", OUT.relative_to(ROOT), len(out.splitlines()), "lines")


if __name__ == "__main__":
    main()
