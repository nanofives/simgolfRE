"""I7: apply every shared-file edit of S2 wave 6 (S2.10), S2 wave 7 (S2.11), S5 wave 1 and S7 waves 1 / 2 to <root> (the mirror
or the shared tree). Texts come from the merge requests themselves (patches, fenced blocks, the uncertain TSVs, S5's 4_patch.py
EDITS), anchored, refusing to apply twice. The Root helper class is I5's (log/roadmap/merges/S2S3-5/apply_shared.py).

  ~/bin/py -3.12 log/roadmap/merges/I7/apply_shared.py <root> [--drop <wave>...]     waves: s2w6 s2w7 s5 s7w1 s7w2

Order (the parent's): S2 wave 6, S2 wave 7, S5, S7 wave 1, S7 wave 2.
- S2 wave 6 (MERGE_wave6.md -> 10_merge.md): git apply 10_shared.patch (8 files), registry.cpp and gfx.cpp touched, U-1290..U-1297
  (10_uncertain.tsv; 10_merge.md asks for no UNCERTAINTIES / CLAUDE.md edit, the rows are appended so the ids the notes cite exist).
- S2 wave 7 (MERGE_wave7.md -> 11_merge.md): git apply 11_shared.patch (12 files), U-1215..U-1219, CLAUDE.md paragraph after S2.13's.
- S5 (S5/MERGE.md): CMake block appended, modules.inc textlab / fttext / fttext_raster (+ registry.cpp touched), the harness edits of
  4_merge.md items 1-3 (4_patch.py EDITS plus the two comment updates), U-1500..U-1532, CLAUDE.md paragraph.
- S7 wave 1 (S7/MERGE.md sections 1-3 + 7_merge.md + 4_merge.md): git apply 7_tick.patch and 7_capi.patch, modules.inc live (+ registry
  touched), CMake add_subdirectory(app), U-1600..U-1664 (1, 2, 3, 4, 6, 7_uncertain.tsv), U-1600 / U-1601 / U-1604 resolved, U-1611 ..
  U-1614 changed as 4_merge.md section 3, CLAUDE.md paragraphs (MERGE.md section 2, 4_merge.md section 4).
- S7 wave 2 (5_merge.md): git apply 5_test_app_live.patch, U-1640..U-1643, CLAUDE.md paragraph (5_merge.md section 4).
CLAUDE.md: S2.11 goes right after the S2.13 paragraph; S5 and the S7 paragraphs go after the S8 paragraph (the end of the
narrative, before `Function bodies are not always contiguous`), in merge order: S5, S7 wave 1, S7.4, S7.5. UNCERTAINTIES rows go
after the last table row; address cells that are not hex addresses are left empty; a `|` inside a cell is written `\\|`."""
import importlib.util
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parents[3]
RM = REPO / "log" / "roadmap"


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


I5 = load("i5_apply", RM / "merges" / "S2S3-5" / "apply_shared.py")
Root, fenced_after = I5.Root, I5.fenced_after
END_ANCHOR = "Function bodies are not always contiguous"


def esc(cell: str) -> str:
    return re.sub(r"(?<!\\)\|", r"\\|", cell)


def rows_from_tsv(p: pathlib.Path) -> list:
    out = []
    for l in p.read_text(encoding="utf-8").splitlines():
        if not l.strip():
            continue
        c = l.split("\t")
        if c[0].strip().lower() == "id":
            continue
        assert len(c) == 6 and c[0].startswith("U-"), (p, l[:60])
        a = c[1].strip()
        if a and not re.fullmatch(r"0x[0-9a-fA-F]+", a):
            c[1] = ""
        out.append("| " + " | ".join(esc(x.strip()) for x in c) + " |")
    return out


def cells_edit(desc_add=None, path_new=None, path_add=None, status=None):
    def f(row: str) -> str:
        cells = row.split(" | ")
        assert len(cells) == 6, len(cells)
        if desc_add:
            cells[3] = cells[3].rstrip() + " " + esc(desc_add)
        if path_new is not None:
            cells[4] = esc(path_new)
        if path_add:
            cells[4] = cells[4].rstrip() + " " + esc(path_add)
        if status:
            cells[5] = status + " |"
        return " | ".join(cells)
    return f


def claude_before(R, anchor: str, block: str, what: str):
    rel = "CLAUDE.md"
    t = R.get(rel)
    b = R.conv(rel, block.strip("\n"))
    if b.split(R.nl(rel))[0] in t:
        raise SystemExit(f"{what}: already in CLAUDE.md")
    a = R.conv(rel, anchor)
    assert t.count(a) == 1, (what, t.count(a))
    i = t.index(a)
    R.put(rel, t[:i] + b + R.nl(rel) + t[i:])
    print("claude", what)


def section_text(md: str, heading: str, stop: str | None) -> str:
    i = md.index(heading) + len(heading)
    j = md.index(stop, i) if stop else len(md)
    return md[i:j].strip("\n")


def touch(root, rel):
    (root / rel).touch()
    print("touch", rel)


def s2w6(R, root):
    S2 = RM / "S2"
    R.patch(S2 / "10_shared.patch", "S2 wave 6 (10_shared.patch)")
    touch(root, "port/src/registry.cpp")
    touch(root, "port/src/sim/gfx.cpp")
    R.unc_rows(rows_from_tsv(S2 / "10_uncertain.tsv"), "S2 wave 6 10_uncertain")


def s2w7(R):
    S2 = RM / "S2"
    R.patch(S2 / "11_shared.patch", "S2 wave 7 (11_shared.patch)")
    R.unc_rows(rows_from_tsv(S2 / "11_uncertain.tsv"), "S2 wave 7 11_uncertain")
    para = fenced_after((S2 / "11_notes.md").read_text(encoding="utf-8"), "## CLAUDE.md paragraph")
    claude_before(R, "Roadmap S3 wave 1, the terrain draw-list passes", para, "S2.11")


def s5(R, root):
    S5 = RM / "S5"
    m = (S5 / "MERGE.md").read_text(encoding="utf-8")
    R.append("port/CMakeLists.txt", (S5 / "1_cmake_block.txt").read_text(encoding="utf-8").strip("\n"), "S5 1 CMake block")
    R.append("port/src/sim/modules.inc", "SG_MODULE(textlab)\nSG_MODULE(fttext)\nSG_MODULE(fttext_raster)", "S5 2 modules.inc",
             sep_blank=False)
    touch(root, "port/src/registry.cpp")
    p = load("s5_4_patch", S5 / "4_patch.py")
    for name, edits in p.EDITS.items():
        for k, (old, new) in enumerate(edits, 1):
            R.replace(f"port/test/{name}.py", old, new, f"S5 3 {name} edit {k}")
    R.replace("port/test/segments.py",
              '        # phase 3.8 (38a): "gdi" attaches the host GDI TextMetrics provider (module gditext) to every run when the\n'
              '        # library has it; "none" (or SG_TEXT_METRICS=none) runs without one (U-0944, U-0947)\n',
              '        # "gdi" | "freetype" | "none". Phase 3.8 (38a): "gdi" attaches the host GDI TextMetrics provider (module gditext) to\n'
              '        # every run when the library has it, "freetype" the FreeType one (module fttext, S5.4); "none" (or\n'
              '        # SG_TEXT_METRICS=none) runs without one (U-0944, U-0947)\n',
              "S5 3 segments comment")
    R.replace("port/test/integrator.py",
              "        # install held beyond original/, and the host GDI TextMetrics provider unless SG_TEXT_METRICS=none\n",
              "        # install held beyond original/, and the TextMetrics provider SG_TEXT_METRICS=none|gdi|freetype selects (default gdi)\n",
              "S5 3 integrator comment")
    rows = []
    for k in ("1", "2", "3", "4"):
        rows += rows_from_tsv(S5 / f"{k}_uncertain.tsv")
    R.unc_rows(rows, "S5 1..4_uncertain")
    claude_before(R, END_ANCHOR, section_text(m, "## CLAUDE.md paragraph", None), "S5")


U1604 = ("Resolved by roadmap S7.7 (2026-10-24, log/roadmap/S7/7_notes.md section 1): the live core runs over S1's two input paths. "
         "A dispatch given at loadFlic's pump (PumpKind::Load) is recorded with jgld's Palette::update QPC record right after its HANDLER "
         "records; that record is parse_trace's `load` marker, so TraceInput hands the batch to the same Load call in the replay. A key "
         "the window tree's focus child took (RootHandler 12 / 13) is recorded as its MSG record with no HANDLER after it; a following root "
         "handler first gets its own MSG record, so parse_trace resolves the pending key as the focus child's.")
U1611 = ("Measured by roadmap S7.4's cross-check, recording `hole` replayed into the original (log/roadmap/S7/4_notes.md section 4): the "
         "original passes rootWin_onKey(0, 0x10010) for Shift's key-down, (0, 0x20011) for Ctrl's, (0, 0x40012) for Alt's; it passes "
         "rootWin_onChar(0x40058) for Alt+X; it passes rootWin_onKey(0x61, 0x10041) for the WM_CHAR of 'a' typed with Shift held. The bits "
         "are uiKeyEvent's (0x00495918..0x00495943). 4_input_map.patch adds them to the translator.")
U1612 = ("S7.4 measured WM_LBUTTONDBLCLK -> rootWin_onPress1b(x, y) then onPoint(x, y) (onPress1b 0x004657e0: click point, button word "
         "0x00822d68 = 1, flags 0x00822d78 / 0x00822d74 = 1). The right button is translated by analogy: U-1631.")
U1613 = ("S7.4: the wheel (+-120, in-game) made no root-handler call in the original and changed no sim object; other screens are "
         "U-1632.")
U1614 = ("S7.4: WM_SYSKEYDOWN / WM_SYSKEYUP reach the root handlers as key-down / key-up with the Alt bit (onKey(0, 0x40012), "
         "onChar(0x40058), onChar0); the WM_SYSCHAR TranslateMessage makes reaches no root handler. WM_DEADCHAR / WM_SYSDEADCHAR are "
         "still unmeasured.")


FT_LOOP_END = ('''                                     "$<TARGET_FILE_DIR:${_sg_ft_tgt}>")
        endif()
      endforeach()
''')
FT_APP_COPY = ('''      if(TARGET simgolf_port)   # roadmap S7's app links portsim too; add_custom_command(TARGET) works only in the target's directory
        add_custom_target(simgolf_port_freetype ALL
                          COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:freetype>" "$<TARGET_FILE_DIR:simgolf_port>")
        add_dependencies(simgolf_port_freetype simgolf_port)
      endif()
''')


def s7w1(R, root):
    S7 = RM / "S7"
    m = (S7 / "MERGE.md").read_text(encoding="utf-8")
    R.patch(S7 / "7_tick.patch", "S7 wave 1 (7_tick.patch)")
    R.patch(S7 / "7_capi.patch", "S7 wave 1 (7_capi.patch)")
    R.append("port/src/sim/modules.inc", "SG_MODULE(live)", "S7 3 modules.inc", sep_blank=False)
    touch(root, "port/src/registry.cpp")
    s7_cmake = ("# Roadmap S7: the Windows host (input translation library and its trace checker; S7.3 adds the app executable).\n"
                "add_subdirectory(app)")
    ft_head = "# Roadmap S5.1 (decision D4): FreeType, the system library"
    if ft_head in R.get("port/CMakeLists.txt"):
        # I7: simgolf_port links portsim, which links a shared FreeType PRIVATE, so the exe imports freetype.dll too; S5's block copies
        # the DLL only next to targets of this directory that exist when it runs. The app goes before that block, which gains a copy
        # target for it (without it the x64 app loaded whatever freetype.dll PATH held: a JDK's on LIVING-PC).
        R.replace("port/CMakeLists.txt", ft_head, s7_cmake + "\n\n" + ft_head, "S7 4 CMake add_subdirectory(app) before S5's block")
        R.replace("port/CMakeLists.txt", FT_LOOP_END, FT_LOOP_END + FT_APP_COPY, "S7 4 freetype.dll beside simgolf_port")
    else:
        R.append("port/CMakeLists.txt", s7_cmake, "S7 4 CMake add_subdirectory(app)")
    rows = []
    for k in ("1", "2", "3", "4", "6", "7"):
        rows += rows_from_tsv(S7 / f"{k}_uncertain.tsv")
    R.unc_rows(rows, "S7 wave 1 uncertain")
    R.unc_edit_row("U-1600", cells_edit(desc_add="Resolved by roadmap S7.6: the trace host's stuck deliveries are recorded inflated and "
                                                  "replay (log/roadmap/S7/6_notes.md section 3).", status="resolved"), "S7 U-1600")
    R.unc_edit_row("U-1601", cells_edit(desc_add="Resolved by roadmap S7.6: idle passes are recorded, the residue is U-1652 / U-1656.",
                                        status="resolved"), "S7 U-1601")
    R.unc_edit_row("U-1604", cells_edit(desc_add=U1604, status="resolved"), "S7 U-1604")
    R.unc_edit_row("U-1611", cells_edit(path_new=U1611, status="resolved"), "S7.4 U-1611")
    R.unc_edit_row("U-1612", cells_edit(path_new=U1612, status="resolved (left button; right button U-1631)"), "S7.4 U-1612")
    R.unc_edit_row("U-1613", cells_edit(path_add=U1613), "S7.4 U-1613")
    R.unc_edit_row("U-1614", cells_edit(path_add=U1614), "S7.4 U-1614")
    claude_before(R, END_ANCHOR, section_text(m, "## 2. CLAUDE.md paragraph (after the S8 paragraph)", "## 3."), "S7 wave 1")
    claude_before(R, END_ANCHOR, section_text((S7 / "4_merge.md").read_text(encoding="utf-8"),
                                              "## 4. CLAUDE.md (port section, after S7.7's paragraph)", None), "S7.4")


def s7w2(R):
    S7 = RM / "S7"
    R.patch(S7 / "5_test_app_live.patch", "S7 wave 2 (5_test_app_live.patch)")
    R.unc_rows(rows_from_tsv(S7 / "5_uncertain.tsv"), "S7 wave 2 5_uncertain")
    para = fenced_after((S7 / "5_merge.md").read_text(encoding="utf-8"), "## 4. CLAUDE.md paragraph")
    claude_before(R, END_ANCHOR, para, "S7.5")


def main():
    root = pathlib.Path(sys.argv[1]).resolve()
    a = sys.argv[2:]
    drop = set(a[a.index("--drop") + 1:]) if "--drop" in a else set()
    R = Root(root, False)
    if "s2w6" not in drop:
        s2w6(R, root)
    if "s2w7" not in drop:
        s2w7(R)
    if "s5" not in drop:
        s5(R, root)
    if "s7w1" not in drop:
        s7w1(R, root)
        if "s7w2" not in drop:
            s7w2(R)
    R.flush()
    print("done", root, "dropped:", sorted(drop) or "none")


if __name__ == "__main__":
    main()
