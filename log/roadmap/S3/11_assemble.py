"""S3.11: assemble ONE resubmission tree = a commit (default HEAD) + S3 wave 2 (S3.4 / S3.4b, with S3.4c's test files) + wave 4
(S3.6 / S3.6b) + wave 5 (S3.5 as rebased by S3.5b) + S3.7 + S3.8, every shared-file edit taken from the steps' own edit lists and
merge requests, anchored (each old text exactly once, refusing an edit already applied), in the parent's order.

    ~/bin/py -3.12 log/roadmap/S3/11_assemble.py [--dst S3_11_src] [--commit <rev>] [--recopy] [--only-edits] [--dry]

Tree: port/build/<dst> = `git archive <commit>` + the junctions of I6's mirror (original, log, port/corpus, tests/golden,
tests/fixtures, re/versions, tools/vc6, shim/build, re/replay, extracted). Own files come from the shared tree (each equals its
step's final mirror, checked in 11_notes.md section 1) EXCEPT the files S3.10 is editing in place (gl_raster.*, test_gl_raster.py,
glraster.py, gfx_terrain.cpp), which stay at the commit's version. Builds are separate (11_build.py).
Every applied edit is printed with its step and number; the log is 11_assemble.log."""
import argparse
import importlib.util
import os
import pathlib
import re
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
RM = ROOT / "log" / "roadmap"
S3 = RM / "S3"
JUNCTIONS = ["original", "log", "port/corpus", "tests/golden", "tests/fixtures", "re/versions", "tools/vc6", "shim/build",
             "re/replay", "extracted"]
EXTRA_UNTRACKED = ["re/tools/glexact/glreplay.exe", "re/tools/glexact/glexact.exe"]
S310_FILES = {"port/src/sim/gl_raster.cpp", "port/include/portsim/sim/gl_raster.h", "port/test/test_gl_raster.py",
              "port/test/glraster.py", "port/src/sim/gfx_terrain.cpp"}

OWN = {
    "w2": ["port/src/sim/flic.cpp", "port/include/portsim/sim/flic.h", "port/test/test_flic.py", "port/test/test_defects_flic.py",
           "re/tools/flic_cells_rec.py", "re/frida/js/flic_cells.js",
           # S3.4c (test files only)
           "port/test/test_menu.py", "port/test/golden_menu.json", "port/test/test_terrain_live.py", "port/test/terrainlive.py"],
    "w4": ["port/src/sim/gfx_hud.cpp", "port/include/portsim/sim/gfx_hud.h", "port/test/test_gfx_hud.py",
           "port/src/sim/gfx_pixel.cpp", "port/include/portsim/sim/gfx_pixel.h", "port/test/test_gfx_pixel.py"],
    "s35": ["port/src/sim/gfx_scene.cpp", "port/include/portsim/sim/gfx_scene.h", "port/test/test_gfx_scene.py"],
    "s37": ["port/src/sim/gfx_text.cpp", "port/include/portsim/sim/gfx_text.h", "port/test/test_gfx_text.py"],
    "s38": ["port/src/sim/terrain_overlay.cpp", "port/include/portsim/sim/terrain_overlay.h", "port/test/test_terrain_overlay.py"],
    "s311": [],   # S3.11 edits only (11_edits.py)
}


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


I6 = load("i6_apply", RM / "merges" / "I6" / "apply_shared.py")
Root, fenced_after, table_rows, addr_cell = I6.Root, I6.fenced_after, I6.table_rows, I6.addr_cell
rows_from_md, rows_from_tsv, resolve, claude_before, section, fenced_all, esc = (
    I6.rows_from_md, I6.rows_from_tsv, I6.resolve, I6.claude_before, I6.section, I6.fenced_all, I6.esc)


def env_of(path):
    env = {}
    exec(path.read_text(encoding="utf-8"), env)
    return env


def portrel(rel):
    return rel if rel.startswith(("port/", "re/", "log/", "tests/")) else "port/" + rel


def edits(R, lst, step):
    for k, (rel, old, new) in enumerate(lst, 1):
        R.replace(portrel(rel), old, new, f"{step} edit {k}")


def ws_sub(text, old, new, what):
    """replace a sentence that may be wrapped differently (whitespace-tolerant), exactly once"""
    pat = r"\s+".join(re.escape(w) for w in old.split())
    out, n = re.subn(pat, lambda _m: new, text)
    assert n == 1, (what, n)
    return out


def replace_unc_row(R, uid, row, what):
    rel = "UNCERTAINTIES.md"
    t = R.get(rel)
    nl = R.nl(rel)
    lines = t.split(nl)
    k = [i for i, l in enumerate(lines) if l.startswith(f"| {uid} |")]
    assert len(k) == 1, (uid, k)
    assert lines[k[0]] != row, what
    lines[k[0]] = row
    R.put(rel, nl.join(lines))
    print("row replace", uid, what)


def tsv_rows_by_id(p):
    return {r.split(" | ")[0][2:].strip(): addr_cell(r) for r in rows_from_tsv(p)}


# ---------------------------------------------------------------------------------------------------------------- wave 2
def wave2(R, root):
    """S3 wave 2 (MERGE_w2.md) exactly as I6's apply_shared.s3w2 applied it in its mirror port/build/I6, plus the PORT_PLAN row 7
    form I6 prepared for it (75 rows, 13 fix, 9 switches), plus S3.4c's CLAUDE.md sentence (4c_merge.md section 2)."""
    m = (S3 / "MERGE_w2.md").read_text(encoding="utf-8")
    R.append("port/src/sim/modules.inc", "SG_MODULE(flic)", "w2 1 modules.inc", sep_blank=False)
    b = fenced_all(m, "### 2. port/src/sim/menu.cpp", "### 3.")
    assert len(b) == 5, len(b)
    R.replace("port/src/sim/menu.cpp", '#include "portsim/sim/gfx.h"\n', '#include "portsim/sim/gfx.h"\n' + b[0] + "\n", "w2 2a include")
    R.replace("port/src/sim/menu.cpp", b[1] + "\n", b[2] + "\n", "w2 2b column read")
    R.replace("port/src/sim/menu.cpp", b[3] + "\n", b[4] + "\n", "w2 2c sheet_to_gfx")
    prof = fenced_after(m, "### 3. re/port/defect_profiles.tsv")
    assert prof.count("\t") == 3, prof
    R.append("re/port/defect_profiles.tsv", prof, "w2 3 profile line", sep_blank=False)
    R.flush()
    # 4: defect_decisions.tsv through S6's generator with I6's prepared D53 input (log/roadmap/merges/I6/prepared_s3w2/);
    # the generator text is run in memory with its input path pointed at the prepared file (no shared log file written)
    prep = RM / "merges" / "I6" / "prepared_s3w2"
    src = (prep / "merge_decisions.py.with_D53").read_text(encoding="utf-8")
    old_in = 'read(S6 / "s3_4b_decisions.tsv")'
    assert src.count(old_in) == 1
    src = src.replace(old_in, "read(pathlib.Path(%r))" % str(prep / "s3_4b_decisions.tsv"))
    g = {"__name__": "merge_decisions_d53", "__file__": str(RM / "S6" / "merge_decisions.py")}
    argv = sys.argv
    try:
        sys.argv = ["merge_decisions.py", "--write", "--root", str(root)]
        if not R.dry:
            exec(compile(src, "merge_decisions_d53", "exec"), g)
            g["main"]()
    finally:
        sys.argv = argv
    print("w2 4 defect_decisions.tsv regenerated with D53")
    R.cache.pop("re/port/defect_decisions.tsv", None)
    R.replace("re/PORT_PLAN.md",
              "| 7. Defect decisions | decisions made (re/port/defect_decisions.tsv, 74 rows: 12 fix, 62 keep; G3 answered the 26 asks); "
              "profile fixed = 8 switches over the hang and fault defects (re/port/defect_profiles.tsv); profile original = every "
              "switch 1 |",
              "| 7. Defect decisions | decisions made (re/port/defect_decisions.tsv, 75 rows: 13 fix, 62 keep; G3 answered the 26 asks, "
              "D53 is the tropical-tree fault of roadmap S3.4b); profile fixed = 9 switches over the hang and fault defects "
              "(re/port/defect_profiles.tsv); profile original = every switch 1 |",
              "w2 PORT_PLAN row 7 (I6's prepared form)")
    rows = table_rows(m, "### 5. UNCERTAINTIES.md", "| U-133")
    R.unc_rows([addr_cell(x) for x in rows_from_md(rows)], "w2 U-1330..U-1339")
    R.unc_edit_row("U-1701", resolve("settled by roadmap S3.4 as U-1330: the six sites are loadWorldAssets', not loadInterfaceArt's; "
                                     "the original dies at the first of them on the shipped install."), "w2 U-1701 resolved")
    para = (fenced_after(m, "### 6. CLAUDE.md")
            + "\nRe-merged with S3 waves 4 / 5 (roadmap S3.11): the defect row is D53 of re/port/defect_decisions.tsv (75 rows, 13 fix / 62"
            + "\nkeep, its input log/roadmap/S6/s3_4b_decisions.tsv) and profile fixed has 9 switches. "
            + fenced_after((S3 / "4c_merge.md").read_text(encoding="utf-8"), "## 2. CLAUDE.md paragraph"))
    claude_before(R, "Roadmap S3 wave 3, the terrain in the frame", para, "w2 CLAUDE.md")
    (root / "port" / "src" / "registry.cpp").touch()


# ---------------------------------------------------------------------------------------------------------------- wave 4
FAM_LAST = "SG_GFX_FAMILY(dialogs)\n"


def wave4(R):
    """S3.6 + S3.6b (MERGE_w4.md: 6_merge.md sections 1-5 as amended by 6b_merge.md sections 2 and 4)."""
    R.replace("port/src/sim/gfx_families.inc", FAM_LAST, FAM_LAST + "SG_GFX_FAMILY(hud)\nSG_GFX_FAMILY(pixel)\n", "w4 families hud, pixel")
    edits(R, env_of(S3 / "6_edits.py")["EDITS"], "w4 6_edits")
    assert env_of(S3 / "6b_edits.py")["EDITS"] == []
    # 6_merge.md section 2 (pixel family's Widget_fillRect line) is done in S3.6b's own gfx_pixel.cpp (6b_merge.md section 2)
    by6 = tsv_rows_by_id(S3 / "6_uncertain.tsv")
    R.unc_rows(list(by6.values()), "w4 U-1350..U-1359")
    R.unc_edit_row("U-1359", resolve("drawn by S3.6b (gfx_hud.cpp b_view_panel / b_detail_panel); evidence: the original's drawing calls "
                                     "under the emulator oracle equal the port's on 240 + 239 varied images, and ui_tour people's bottom "
                                     "panel equals the reference (U-1380, U-1381)."), "w4 U-1359 resolved (6b)")
    R.unc_rows(list(tsv_rows_by_id(S3 / "6b_uncertain.tsv").values()), "w4 U-1380..U-1384")
    m6 = (S3 / "6_merge.md").read_text(encoding="utf-8")
    para = section(m6, "## 5. CLAUDE.md paragraph (after the S3 wave paragraphs)")
    m6b = (S3 / "6b_merge.md").read_text(encoding="utf-8")
    s4 = section(m6b, "## 4. CLAUDE.md sentence (replaces the one named in section 2)")
    para = ws_sub(para, "The golfer list and detail panels (drawEmployeeViewPanel / drawEmployeeDetailPanel) are handed over and "
                        "reported, not drawn.", s4, "6b sentence")
    para = ws_sub(para, "GATE MEASURE: the port draws no terrain or scene yet, so",
                  "GATE MEASURE: framegate --no-live (no terrain; S3.2's terrain is judged under G2) and no scene sprites, so", "6b gate")
    claude_before(R, "Phase 7 defect decisions, section S6 of the roadmap orchestrator", para, "w4 CLAUDE.md (S3.6 / S3.6b)")


# ---------------------------------------------------------------------------------------------------------------- wave 5
def wave5(R):
    """S3.5 (5_merge.md) as rebased by S3.5b (5b_merge.md): 5_merge_edits.py EDITS, EDITS_AFTER_S2W5, EDITS_HUD, then 5b_edits.py
    EDITS and the S2 test expectations of 5b_edits_tests.py EDITS_TESTS."""
    me = env_of(S3 / "5_merge_edits.py")
    edits(R, me["EDITS"], "s35 EDITS")
    edits(R, me["EDITS_AFTER_S2W5"], "s35 EDITS_AFTER_S2W5")
    edits(R, me["EDITS_HUD"], "s35 EDITS_HUD")
    edits(R, env_of(S3 / "5b_edits.py")["EDITS"], "s35b EDITS")
    edits(R, env_of(S3 / "5b_edits_tests.py")["EDITS_TESTS"], "s35b EDITS_TESTS")
    by5 = tsv_rows_by_id(S3 / "5_uncertain.tsv")
    replace_unc_row(R, "U-1332", by5.pop("U-1332"), "s35 U-1332 resolved (5_uncertain.tsv's row)")
    R.unc_rows(list(by5.values()), "s35 U-1340..U-1349")
    R.unc_rows(list(tsv_rows_by_id(S3 / "5b_uncertain.tsv").values()), "s35b U-1385..U-1387")
    R.unc_edit_row("U-1349", resolve("(b) resolved by S3.5b: the figure is placeAnimal's queue entry (an animal record's standing figure), "
                                     "queued from golfers_world.inc; (a) the green ring / grey lines stay S3.8's", "partial"),
                   "s35b U-1349 partial")
    n5 = (S3 / "5_notes.md").read_text(encoding="utf-8")
    p8 = section(n5, "## 8. CLAUDE.md paragraph (ready to paste)")
    main, r2 = p8.split("\n\nRound 2 (append to the paragraph): ")
    para5 = main + "\nRound 2: " + r2
    claude_before(R, "Phase 7 defect decisions, section S6 of the roadmap orchestrator", para5, "s35 CLAUDE.md")
    para5b = section((S3 / "5b_merge.md").read_text(encoding="utf-8"), "## 4. CLAUDE.md paragraph (after the S3.6 / S3.6b paragraph)")
    claude_before(R, "Phase 7 defect decisions, section S6 of the roadmap orchestrator", para5b, "s35b CLAUDE.md")


# ---------------------------------------------------------------------------------------------------------------- S3.7
def s37(R):
    R.append("port/src/sim/gfx_families.inc", "SG_GFX_FAMILY(text)", "s37 family text", sep_blank=False)
    edits(R, env_of(S3 / "7_edits.py")["EDITS"], "s37 7_edits")
    R.unc_rows(list(tsv_rows_by_id(S3 / "7_uncertain.tsv").values()), "s37 U-1360..U-1368")
    m7 = (S3 / "7_merge.md").read_text(encoding="utf-8")
    for uid, status in (("U-1069", "partial"), ("U-1072", "resolved"), ("U-1353", "resolved")):
        mm = re.search(r"- " + uid + r": status `" + status + r"`, append: \"(.*?)\"\n(?=- |\n|$)", m7, re.S)
        assert mm, uid
        R.unc_edit_row(uid, resolve(" ".join(mm.group(1).split()), status), f"s37 {uid} {status}")
    para = section((S3 / "7_notes.md").read_text(encoding="utf-8"), "## 9. CLAUDE.md paragraph")
    claude_before(R, "Phase 7 defect decisions, section S6 of the roadmap orchestrator", para, "s37 CLAUDE.md")


# ---------------------------------------------------------------------------------------------------------------- S3.8
def s38(R):
    edits(R, env_of(S3 / "8_edits.py")["EDITS"], "s38 8_edits")
    R.unc_rows(list(tsv_rows_by_id(S3 / "8_uncertain.tsv").values()), "s38 U-1370..")
    n8 = (S3 / "8_notes.md").read_text(encoding="utf-8")
    i = n8.index("## 6. CLAUDE.md paragraph") + len("## 6. CLAUDE.md paragraph")
    para = n8[i:n8.index("\n## ", i)].strip("\n")
    claude_before(R, "Phase 7 defect decisions, section S6 of the roadmap orchestrator", para, "s38 CLAUDE.md")


# ---------------------------------------------------------------------------------------------------------------- S3.11
def s311(R):
    """S3.11's own integration fixes (11_edits.py, named in 11_notes.md section 2)."""
    edits(R, env_of(S3 / "11_edits.py")["EDITS"], "s311 11_edits")
    para = fenced_after((S3 / "MERGE_w5.md").read_text(encoding="utf-8"), "## 6. CLAUDE.md paragraph")
    claude_before(R, "Phase 7 defect decisions, section S6 of the roadmap orchestrator", para, "s311 CLAUDE.md")


STEPS = [("w2", wave2), ("w4", wave4), ("s35", wave5), ("s37", s37), ("s38", s38), ("s311", s311)]


def make_tree(dst, commit, recopy):
    if (dst / "port" / "CMakeLists.txt").exists() and not recopy:
        sys.exit(f"{dst} exists (use --recopy to rebuild it from the archive)")
    if dst.exists():
        # unlink the junctions first (never followed), keep port/build (the builds), remove the rest
        for j in JUNCTIONS:
            if os.path.isjunction(dst / j):
                os.rmdir(dst / j)
        for e in dst.iterdir():
            assert not os.path.isjunction(e), e
            if e.name == "port":
                for f in e.iterdir():
                    assert not os.path.isjunction(f), f
                    if f.name == "build":
                        continue
                    shutil.rmtree(f) if f.is_dir() else f.unlink()
            elif e.is_dir():
                shutil.rmtree(e)
            else:
                e.unlink()
    dst.mkdir(parents=True, exist_ok=True)
    head = subprocess.run(["git", "rev-parse", commit], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
    tar = subprocess.run(["git", "archive", head], cwd=ROOT, capture_output=True, check=True).stdout
    subprocess.run(["tar", "-xf", "-"], cwd=dst, input=tar, check=True)
    (dst / "BASE_COMMIT").write_text(head + "\n")
    for j in JUNCTIONS:
        link = dst / j
        if link.exists() and not os.path.isjunction(link):
            assert dst in link.resolve().parents, link
            shutil.rmtree(link)
        if not link.exists() and (ROOT / j).exists():
            link.parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(ROOT / j)], check=True, capture_output=True)
            print("junction", j)
    for rel in EXTRA_UNTRACKED:
        (dst / rel).parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / rel, dst / rel)
    print("tree", dst, "base", head)
    return head


def copy_own(dst, steps):
    if dst.resolve() == ROOT.resolve():
        print("own files: already in the shared tree, not copied")
        return
    for s in steps:
        for rel in OWN[s]:
            assert rel not in S310_FILES, rel
            src = ROOT / rel
            assert src.is_file(), rel
            (dst / rel).parent.mkdir(parents=True, exist_ok=True)
            (dst / rel).write_bytes(src.read_bytes())
            print("copied", s, rel)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dst", default="S3_11_src")
    ap.add_argument("--commit", default="HEAD")
    ap.add_argument("--recopy", action="store_true")
    ap.add_argument("--only-edits", action="store_true", help="apply the edits to an existing fresh tree")
    ap.add_argument("--dry", action="store_true")
    ap.add_argument("--stop-after", choices=[s for s, _ in STEPS])
    ap.add_argument("--root", help="apply the own files and edits to this root instead (e.g. `.` = the integrator's tree after "
                                   "its own merges; no archive is made, implies --only-edits)")
    a = ap.parse_args()
    dst = ROOT / "port" / "build" / a.dst
    if a.root:
        dst, a.only_edits = pathlib.Path(a.root).resolve(), True
    if not a.only_edits:
        make_tree(dst, a.commit, a.recopy)
    steps = [s for s, _ in STEPS]
    if a.stop_after:
        steps = steps[:steps.index(a.stop_after) + 1]
    copy_own(dst, steps)
    R = Root(dst, a.dry)
    for name, fn in STEPS:
        if name not in steps:
            continue
        print("==", name)
        if name == "w2":
            fn(R, dst)
        else:
            fn(R)
        R.flush()
    (dst / "port" / "src" / "registry.cpp").touch()
    (dst / "port" / "src" / "sim" / "gfx.cpp").touch()
    print("done", dst, "steps", steps)


if __name__ == "__main__":
    main()
