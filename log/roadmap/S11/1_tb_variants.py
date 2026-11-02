"""S11.1: hand variants of SMSG's RenderBuildingToolbar (Toolbar.cpp) under the default toolchain (cl 8168), looking for
a source that reads 100% at drawAmenitiesPanel 0x00434cf0 (the unmodified source reads 99.9%: one reload pair swapped after
the second tooltip call 0x00432620, log/roadmap/S10/12_toolbar_434cf0.show.log).

Each variant is a list of (old, new) replacements applied inside the function only; the copy is written to
log/roadmap/S11/1_tb/Toolbar.cpp and scored with match.py --no-evidence --only 0x00434cf0 (no CSV).
Run: ~/bin/py -3.12 log/roadmap/S11/1_tb_variants.py [name ...]
"""
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
SRC = ROOT / "third_party/smsg/SMSG-decomp/golf/golf.exe/src/Toolbar.cpp"
WORK = ROOT / "log/roadmap/S11/1_tb"
WRAP = WORK / "wrap.cpp"
PY = sys.executable

TT2 = "                RenderToolbarTooltip(x, y - 2);\n"
VARIANTS = {
    "base": [],
    "tt2_plus": [("RenderToolbarTooltip(x, y - 2);", "RenderToolbarTooltip(x, y + -2);")],
    "case_m2_swap": [("            tx = x;\n            ty = y;\n", "            ty = y;\n            tx = x;\n")],
    "tt1_tt2_vars": [("RenderToolbarTooltip(x, y - 0xc);", "RenderToolbarTooltip(x, -0xc + y);"),
                     ("RenderToolbarTooltip(x, y - 2);", "RenderToolbarTooltip(x, -2 + y);")],
    "decl_order": [("int sel, lit, id, tx, ty, price, n, cx;", "int sel, lit, id, cx, n, price, ty, tx;")],
    "decl_split": [("int sel, lit, id, tx, ty, price, n, cx;", "int sel;\n    int lit;\n    int id;\n    int tx, ty, price, n, cx;")],
    "id_local_landmark": [("            id = RenderToolbarRow(0, 0x168", "            int id = RenderToolbarRow(0, 0x168")],
    "switch_default": [("                case 3: strcpy(g_printBuffer, \"Happy Endings\"); break;\n                }",
                        "                default: strcpy(g_printBuffer, \"Happy Endings\"); break;\n                }")],
    "freelm_ne0": [("if (g_freeLandmarks & (1 << id))", "if ((g_freeLandmarks & (1 << id)) != 0)")],
    "hover_y_first": [("g_buildingToolbarPos[lit][0], g_buildingToolbarPos[lit][1],\n            0);\n    }\n    if (g_selectedBuildingKind == SCENERY_SCENIC_BRIDGE)",
                       "g_buildingToolbarPos[lit][0], g_buildingToolbarPos[lit][1],\n            0);\n    }\n    if (SCENERY_SCENIC_BRIDGE == g_selectedBuildingKind)")],
    "y_copy": [("    int sel, lit, id, tx, ty, price, n, cx;\n", "    int sel, lit, id, tx, ty, price, n, cx;\n    int yy = y;\n"),
               ("RenderToolbarTooltip(x, y - 2);", "RenderToolbarTooltip(x, yy - 2);")],
}


def header() -> str:
    w = (ROOT / "re/match/wip/smsg_golf_Toolbar.cpp").read_text().splitlines()
    keep = [l for l in w if l.startswith("// FLAGS") or l.startswith("// INCLUDE")]
    return "\n".join(keep + [f"// COMPILE {(WORK / 'Toolbar.cpp').relative_to(ROOT).as_posix()}", "",
                             "// MATCH: golf_clean.exe 0x00434cf0 ?RenderBuildingToolbar@@YAXHH@Z   // drawAmenitiesPanel"]) + "\n"


def apply(text: str, reps) -> str:
    a = text.index("void RenderBuildingToolbar(int x, int y)")
    b = text.index("\nint HitTestGolferPairingGrid", a)
    body = text[a:b]
    for old, new in reps:
        if body.count(old) != 1:
            raise SystemExit(f"anchor not unique ({body.count(old)}): {old[:60]!r}")
        body = body.replace(old, new)
    return text[:a] + body + text[b:]


def main(names) -> int:
    WORK.mkdir(parents=True, exist_ok=True)
    WRAP.write_text(header())
    base = SRC.read_text(encoding="latin-1")
    for name in names or list(VARIANTS):
        (WORK / "Toolbar.cpp").write_text(apply(base, VARIANTS[name]), encoding="latin-1")
        r = subprocess.run([PY, str(ROOT / "re/tools/match.py"), str(WRAP), "--no-evidence", "--only", "0x00434cf0"],
                           cwd=ROOT, capture_output=True, text=True)
        out = (r.stdout + r.stderr).strip().splitlines()
        score = next((l for l in out if "0x00434cf0" in l and "%" in l), out[-1] if out else "no output")
        m = re.search(r"([\d.]+)% \((\d+)/(\d+)", score)
        print(f"[{name}] {m.group(0) if m else score}", flush=True)
        for l in out:
            if l.startswith("   !"):
                print("    " + l.strip(), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
