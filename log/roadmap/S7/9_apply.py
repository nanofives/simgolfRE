"""S7.9: every shared-file edit of S7 waves 1-3 (S7.1-S7.8) plus S7.9, as ONE merge on HEAD 78ffb06d, applied to <root> (the
mirror's pristine copy, a scratch copy or the shared tree). Built on I7's apply_shared.py (its Root helper from I5, its
UNCERTAINTIES / CLAUDE.md helpers and its s7w1 / s7w2 texts), refusing to apply twice.

  ~/bin/py -3.12 log/roadmap/S7/9_apply.py <root> [--files]

--files also copies S7's step-owned files from the mirror port/build/S7_9_head into <root> (live.h / live.cpp / live.py /
test_live.py, port/app/**, the app tests, app_replay.py; list: FILES below).

Order: 9_tick.patch, 9_capi.patch, 9_cmake.patch, 9_gfx.patch (+ registry.cpp touched), UNCERTAINTIES rows (1, 2, 3, 4, 6, 7, 5, 8, 9 _uncertain.tsv)
and status edits (U-1600, U-1601, U-1604, U-1611..U-1614 as I7 had them; U-1642, U-1643 resolved by S7.8; U-1670 resolved by S7.9), mainLoop_structure.md
row P6 (8_merge.md section 4), CLAUDE.md paragraphs before `Function bodies are not always contiguous` in this order: S7 wave 1
(MERGE.md section 2), S7.4 (4_merge.md section 4), S7.5 (5_merge.md section 4), S7.8 (8_notes.md section 6), S7.9 (9_notes.md
section 7)."""
import importlib.util
import pathlib
import shutil
import sys

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parents[2]
RM = REPO / "log" / "roadmap"
MIRROR = REPO / "port" / "build" / "S7_9_head"


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


I7 = load("i7_apply", RM / "merges" / "I7" / "apply_shared.py")
Root, rows_from_tsv, cells_edit, claude_before, section_text, fenced_after = (
    I7.Root, I7.rows_from_tsv, I7.cells_edit, I7.claude_before, I7.section_text, I7.fenced_after)
END = I7.END_ANCHOR

FILES = (["port/include/portsim/sim/live.h", "port/src/sim/live.cpp", "port/test/live.py", "port/test/test_live.py",
          "port/test/app_replay.py", "port/test/test_app_input.py", "port/test/test_app_shell.py", "port/test/test_app_live.py",
          "port/test/test_app_replay.py", "port/test/test_app_ingame.py", "port/test/test_app_present.py"]
         + sorted(p.relative_to(MIRROR).as_posix() for p in (MIRROR / "port" / "app").rglob("*")
                  if p.is_file() and "__pycache__" not in p.parts))

U1642 = ("Resolved by roadmap S7.8 (2026-10-25, log/roadmap/S7/8_notes.md section 1): the original's back buffer reaches the window only "
         "through jgld Display::present (Display vtable slot +0xa4), called once in the exe, at the end of drawTooltip 0x0047cdb0; "
         "Window::refreshFull reaches it through refreshRect. tick.cpp calls LiveIo::present_point after the Window::refreshFull drop of a "
         "live run and the app presents there: 0 frame-wait presents over four traces and the in-game session.")
U1643 = ("Resolved by roadmap S7.8 (2026-10-25, log/roadmap/S7/8_notes.md section 2): the terrain log is small after the world load "
         "(25,404 of 25,671 calls over course_season_6y); the growth was the live core's in-memory records. LiveOptions::trace_file spills "
         "the recording, on_record hands each frame record to the app: private bytes flat after frame 2,000 over 51,092 records.")
U1670 = ("Resolved by roadmap S7.9 (2026-10-26, log/roadmap/S7/9_notes.md section 3): the gfx backend calls a per-context refresh "
         "hook at the end of its Window::refreshFull body (gfx.h gfx_set_refresh_hook), which a live run's core sets, so every refresh "
         "presents, whichever path issued it (the frame code's drop, a module's gx calls, a family calling the body); tick.cpp's "
         "TkBase-only call (8_tick.patch) is gone. Presents at one recording position: U-1685.")
P6_OLD = "the display present slot [[0x0083ad50]+0x9c] (0x00418cd1)"
P6_NEW = ("the display scale slot [[0x0083ad50]+0x9c] = Display::setScale(1, 1, 1) (0x00418cd1; the back buffer reaches the window in "
          "Window::refreshFull -> refreshRect -> drawTooltip -> Display::present +0xa4 at 0x0047cfba, H1 0x0041e29b in an ordinary frame)")


def touch(root, rel):
    (root / rel).touch()
    print("touch", rel)


def main():
    root = pathlib.Path(sys.argv[1]).resolve()
    if "--files" in sys.argv[2:]:
        for rel in FILES:
            d = root / rel
            d.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(MIRROR / rel, d)
            print("copied", rel)
    S7 = RM / "S7"
    R = Root(root, False)
    for p in ("9_tick.patch", "9_capi.patch", "9_cmake.patch", "9_gfx.patch"):
        R.patch(S7 / p, f"S7.9 ({p})")
    touch(root, "port/src/registry.cpp")
    rows = []
    for k in ("1", "2", "3", "4", "6", "7", "5", "8", "9"):
        rows += rows_from_tsv(S7 / f"{k}_uncertain.tsv")
    R.unc_rows(rows, "S7 waves 1-3 + S7.9 uncertain rows")
    R.unc_edit_row("U-1600", cells_edit(desc_add="Resolved by roadmap S7.6: the trace host's stuck deliveries are recorded inflated and "
                                                  "replay (log/roadmap/S7/6_notes.md section 3); judged since S7.9 on ui_tour / ui_tour_v "
                                                  "(9_notes.md section 1).", status="resolved"), "S7 U-1600")
    R.unc_edit_row("U-1601", cells_edit(desc_add="Resolved by roadmap S7.6: idle passes are recorded, the residue is U-1652 / U-1656.",
                                        status="resolved"), "S7 U-1601")
    R.unc_edit_row("U-1604", cells_edit(desc_add=I7.U1604, status="resolved"), "S7 U-1604")
    R.unc_edit_row("U-1611", cells_edit(path_new=I7.U1611, status="resolved"), "S7.4 U-1611")
    R.unc_edit_row("U-1612", cells_edit(path_new=I7.U1612, status="resolved (left button; right button U-1631)"), "S7.4 U-1612")
    R.unc_edit_row("U-1613", cells_edit(path_add=I7.U1613), "S7.4 U-1613")
    R.unc_edit_row("U-1614", cells_edit(path_add=I7.U1614), "S7.4 U-1614")
    R.unc_edit_row("U-1642", cells_edit(desc_add=U1642, status="resolved"), "S7.8 U-1642")
    R.unc_edit_row("U-1643", cells_edit(desc_add=U1643, status="resolved"), "S7.8 U-1643")
    R.unc_edit_row("U-1670", cells_edit(desc_add=U1670, status="resolved"), "S7.9 U-1670")
    R.replace("re/analysis/sim/mainLoop_structure.md", P6_OLD, P6_NEW, "S7.8 mainLoop_structure.md row P6")
    m = (S7 / "MERGE.md").read_text(encoding="utf-8")
    claude_before(R, END, section_text(m, "## 2. CLAUDE.md paragraph (after the S8 paragraph)", "## 3."), "S7 wave 1")
    claude_before(R, END, section_text((S7 / "4_merge.md").read_text(encoding="utf-8"),
                                       "## 4. CLAUDE.md (port section, after S7.7's paragraph)", None), "S7.4")
    claude_before(R, END, fenced_after((S7 / "5_merge.md").read_text(encoding="utf-8"), "## 4. CLAUDE.md paragraph"), "S7.5")
    claude_before(R, END, section_text((S7 / "8_notes.md").read_text(encoding="utf-8"), "## 6. Paragraph for CLAUDE.md",
                                       "## 7."), "S7.8")
    claude_before(R, END, section_text((S7 / "9_notes.md").read_text(encoding="utf-8"), "## 7. Paragraph for CLAUDE.md",
                                       "## 8."), "S7.9")
    R.flush()
    print("done", root)


if __name__ == "__main__":
    main()
