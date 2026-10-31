"""I20 mirror: port/build/I20 = `git archive f2dff003` + I19's junctions + S7 wave 4's files (MERGE_w4.md), then the shared-file
edits of MERGE_w4.md applied to the mirror's own copies. log/ is a junction to the shared tree's log/, so the patched
log/roadmap/S6/merge_decisions.py is written to log/roadmap/merges/I20/shared/ instead (the orchestrator commits that blob).

    ~/bin/py -3.12 log/roadmap/merges/I20/make_mirror.py            # create + copy + edit (refuses an existing mirror)
"""
from __future__ import annotations

import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
BASE = "f2dff003"
M = ROOT / "port" / "build" / "I20"
S7 = ROOT / "log" / "roadmap" / "S7"
SRC = ROOT / "port" / "build" / "S7_w4_root"
OUT = ROOT / "log" / "roadmap" / "merges" / "I20"
JUNCTIONS = ["extracted", "log", "original", "port/corpus", "re/replay", "re/versions", "shim/build", "tests/fixtures",
             "tests/golden", "tools/dxsdk8", "tools/vc6", "tools/vc6sp2", "tools/vc6sp5"]
# port files of files_w4.txt: from S7_w4_root (the tree S7's gate ran on), main.cpp from the rebase result
PORT_FILES = ["port/app/CMakeLists.txt", "port/app/audio_out.cpp", "port/app/audio_out.h", "port/app/ds_sink.cpp",
              "port/app/ds_sink.h", "port/app/tools/audio_check.cpp", "port/include/portsim/sim/audio_exe.h",
              "port/src/sim/audio_exe.cpp", "port/test/app_replay.py", "port/test/test_app_audio.py",
              "port/test/test_defects_golfsfx.py"]
MAIN = S7 / "w4_rebase" / "main_merged.cpp"
# non-port files of files_w4.txt outside log/ (S7_w4_root's re/port is a junction to the shared tree)
SHARED_FILES = ["re/port/defect_decisions.tsv", "re/port/defect_profiles.tsv", "re/roadmap/S7_steps.tsv"]


def junction(dst: pathlib.Path, src: pathlib.Path) -> None:
    if dst.exists():
        shutil.rmtree(dst)
    dst.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["cmd", "/c", "mklink", "/J", str(dst), str(src)], check=True, capture_output=True)


def replace_once(path: pathlib.Path, old: str, new: str) -> None:
    t = path.read_bytes().decode("utf-8")
    nl = "\r\n" if "\r\n" in t else "\n"
    old, new = old.replace("\n", nl), new.replace("\n", nl)
    assert t.count(old) == 1, (path, t.count(old), old[:80])
    path.write_bytes(t.replace(old, new).encode("utf-8"))
    print("edited", path.relative_to(ROOT))


def tsv_rows(p: pathlib.Path) -> list[str]:
    lines = p.read_text(encoding="utf-8").splitlines()
    return [l for l in lines[1:] if l.strip()]


def main() -> None:
    assert not M.exists(), M
    M.mkdir(parents=True)
    arc = subprocess.run(["git", "-C", str(ROOT), "archive", BASE], check=True, capture_output=True).stdout
    subprocess.run(["tar", "-x", "-C", str(M)], input=arc, check=True)
    (M / "HEAD_COMMIT").write_text(BASE + " + S7 wave 4 (I20)\n")
    for j in JUNCTIONS:
        junction(M / j, ROOT / j)
    for rel in PORT_FILES:
        shutil.copy(SRC / rel, M / rel)
    shutil.copy(MAIN, M / "port/app/main.cpp")
    for rel in SHARED_FILES:
        (M / rel).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(ROOT / rel, M / rel)

    # 2. integrator.py ledger entry (12_merge.md patch 1)
    replace_once(M / "port/test/integrator.py",
                 '"test_tropical_world_loads_equal_the_alias_recording_off (state)"}\n',
                 '"test_tropical_world_loads_equal_the_alias_recording_off (state)",\n'
                 '                 # roadmap S7.12 (D54, G15): the device sink opens the 18 course effects through the alias at 0, line for line\n'
                 '                 # an install with the exe\'s directory name; the switch-1 run is golden $device\n'
                 '                 "sound.golfSfxAlias": "port/test/test_defects_golfsfx.py::test_device_run_switch_zero_opens_the_course_effects, "\n'
                 '                                       "test_device_run_switch_zero_equals_a_renamed_install (device sink)"}\n')

    # 3. merge_decisions.py (12_merge.md patch 2), written beside the merge report (log/ is the shared tree's)
    md = OUT / "shared" / "log" / "roadmap" / "S6" / "merge_decisions.py"
    md.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["git", "-C", str(ROOT), "show", f"{BASE}:log/roadmap/S6/merge_decisions.py"], check=True,
                   stdout=md.open("wb"))
    replace_once(md, '    d5_s3 = {r["id"] for r in d}\n',
                 '    d5_s3 = {r["id"] for r in d}\n'
                 '    # roadmap S7.12 (G15, Mariano 2026-10-30): the course-sfx directory alias\n'
                 '    h7, g = read(ROOT / "log" / "roadmap" / "S7" / "12_decisions.tsv")\n'
                 '    assert h7 == h6, h7\n'
                 '    for r in g:\n'
                 '        assert r["id"] not in out, r["id"]\n'
                 '        out[r["id"]] = dict(r, verified="device-sink run (S7.12)")\n'
                 '    g15 = {r["id"] for r in g}\n')
    replace_once(md, '        elif r["recommendation"] == "fix":\n',
                 '        elif r["id"] in g15:\n'
                 '            r["decision"] = "G15 fix (S7.12)"\n'
                 '        elif r["recommendation"] == "fix":\n')

    # 4. UNCERTAINTIES.md: rows of 10_ / 11_ / 12_uncertain.tsv, the U-1499 sentence
    u = M / "UNCERTAINTIES.md"
    t = u.read_bytes().decode("utf-8")
    nl = "\r\n" if "\r\n" in t else "\n"
    lines = t.split(nl)
    i1499 = [i for i, l in enumerate(lines) if l.startswith("| U-1499 ")]
    assert len(i1499) == 1, i1499
    cells = lines[i1499[0]].split("|")
    assert len(cells) == 8, len(cells)
    # the sentence goes at the end of the row's `path to resolution` cell (the last cell is the status)
    cells[5] = cells[5].rstrip() + " S7.12: switch sound.golfSfxAlias (profile fixed, the app's default) opens them from Golf_Sfx (D54). "
    lines[i1499[0]] = "|".join(cells)
    new = []
    for f in ("10_uncertain.tsv", "11_uncertain.tsv", "12_uncertain.tsv"):
        for r in tsv_rows(S7 / f):
            c6 = r.split("\t")
            assert len(c6) == 6 and "|" not in r, r[:60]
            rid = c6[0]
            if c6[1] == "-":
                c6[1] = ""  # addr cells are one hex number or empty (re_classify parses every row)
            assert not any(x.startswith("| " + rid + " ") for x in lines), rid
            new.append("| " + " | ".join(c6) + " |")
    while lines and lines[-1] == "":
        lines.pop()
    lines += new + [""]
    u.write_bytes(nl.join(lines).encode("utf-8"))
    print("edited UNCERTAINTIES.md: +", len(new), "rows")

    # 5. CLAUDE.md: three paragraphs after "Roadmap S7.9", S7.13 sentence appended to S7.12's
    mw = (S7 / "MERGE_w4.md").read_text(encoding="utf-8")
    sec = mw.split("## CLAUDE.md paragraphs (after \"Roadmap S7.9\", in this order)", 1)[1].split("\n## ", 1)[0]
    paras = [p.strip() for p in sec.strip().split("\n\n")]
    assert len(paras) == 3 and paras[0].startswith("Roadmap S7.10,") and paras[2].startswith("Roadmap S7.12,"), paras
    s713 = mw.split("- CLAUDE.md: append to the S7.12 paragraph: `", 1)[1].split("`\n", 1)[0]
    s713 = " ".join(x.strip() for x in s713.split("\n"))
    assert s713.startswith("S7.13 (G16)") and s713.endswith("x64 = Win32."), s713
    paras[2] = paras[2] + " " + s713
    c = M / "CLAUDE.md"
    t = c.read_bytes().decode("utf-8")
    nl = "\r\n" if "\r\n" in t else "\n"
    lines = t.split(nl)
    st = [i for i, x in enumerate(lines) if x.startswith("Roadmap S7.9,")]
    assert len(st) == 1, st
    # the S7.9 paragraph runs to the line before "Function bodies are not always contiguous" (no blank lines between paragraphs)
    j = st[0] + 1
    while not lines[j].startswith(("Roadmap ", "Section ", "Phase ", "Function bodies are not always contiguous", "### ", "## ")):
        j += 1
    assert lines[j].startswith("Function bodies are not always contiguous"), lines[j][:80]
    ins = []
    for p in paras:
        ins += p.split("\n")
    lines[j:j] = ins
    c.write_bytes(nl.join(lines).encode("utf-8"))
    print("edited CLAUDE.md: +", len(ins), "lines after line", j)


if __name__ == "__main__":
    sys.exit(main())
