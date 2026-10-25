"""S2.13: build the integration mirror port/build/S2_13_head.

  HEAD export (git archive) + junctions (log/roadmap/S0/export.md)
  + layer W13: waves 1-3 = the files S2_4_head changed against c6d7d78b (3-way merged onto HEAD with git merge-file,
               base = c6d7d78b), plus wave 1's README section (MERGE.md 1.2)
  + layer W4:  wave 4's new files from the working tree (files_wave4.txt) and scenario_refs.json's key cov_r_s2_screens
  + layers S2.7 / S2.8 / S2.9: the files each step mirror changed against S2_4_head, 3-way merged (base = S2_4_head)
Writes the pre-step tree's copies of every file layers S2.7-S2.9 touch to port/build/S2_13_pre (the patch base) and prints
the merge report (clean / conflict per file).

usage: ~/bin/py -3.12 log/roadmap/S2/13_tools/make_mirror.py [--commit <id>] [--keep]
"""
import argparse
import json
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
B = ROOT / "port" / "build"
MIRROR = B / "S2_13_head"
PRE = B / "S2_13_pre"
C6 = B / "S2_13_c6"           # git archive c6d7d78b of port re/tools re/roadmap tests scripts
S24 = B / "S2_4_head"
STEPS = {"7": B / "S2_7_head", "8": B / "S2_8_head", "9": B / "S2_9_head"}
JUNCTIONS = ["log", "original", "port/corpus", "re/replay", "re/versions", "shim/build", "tests/fixtures", "tests/golden"]
SUBS = ["port", "re/tools", "re/roadmap", "tests", "scripts"]
# not merged: S2.8's boot.py KNOWN entry (orchestrator); S2.7's copy of test_s2_6_scenario.py / s2_6 tools (wave 4 takes the
# working tree's lead rewrite); re/roadmap step tables (the lead's)
EXCLUDE = {"port/test/boot.py", "port/test/test_s2_6_scenario.py", "re/tools/s2_6_runs.py", "re/tools/scenario_s2_6.py"}
WAVE4 = ["port/test/test_s2_5_traces.py", "re/tools/scenario_s2_6.py", "re/tools/s2_6_runs.py", "port/test/test_s2_6_scenario.py"]

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import scan  # noqa: E402


def changed(tree_b, tree_a):
    out = []
    for sub in SUBS:
        if not (tree_b / sub).exists():
            continue
        b = scan.walk(tree_b / sub, sub + "/")
        a = scan.walk(tree_a / sub, sub + "/") if (tree_a / sub).exists() else {}
        for rel, p in b.items():
            r = f"{sub}/{rel}"
            if r.startswith("port/build/") or r.startswith("port/corpus/") or "__pycache__" in r or r.endswith(".pyc"):
                continue
            if r in EXCLUDE or r.startswith("re/roadmap/"):
                continue
            if rel not in a:
                out.append(("new", r))
            elif p.read_bytes().replace(b"\r\n", b"\n") != a[rel].read_bytes().replace(b"\r\n", b"\n"):
                out.append(("mod", r))
    return out


def lf(p):
    return p.read_bytes().replace(b"\r\n", b"\n")


def merge3(ours: pathlib.Path, base: pathlib.Path, theirs: pathlib.Path, label):
    """merge theirs' change (base -> theirs) into ours in place; returns 'clean', 'same' or 'conflict:<n>'"""
    o, b, t = lf(ours), lf(base), lf(theirs)
    if t == b or t == o:
        ours.write_bytes(o)
        return "same"
    if o == b:
        ours.write_bytes(t)
        return "clean(fast)"
    tmp = MIRROR.parent / "S2_13_tmp"
    tmp.mkdir(exist_ok=True)
    (tmp / "o").write_bytes(o)
    (tmp / "b").write_bytes(b)
    (tmp / "t").write_bytes(t)
    r = subprocess.run(["git", "merge-file", "-L", "ours", "-L", "base", "-L", label, str(tmp / "o"), str(tmp / "b"),
                        str(tmp / "t")], capture_output=True, text=True)
    ours.write_bytes((tmp / "o").read_bytes())
    return "clean" if r.returncode == 0 else f"conflict:{r.returncode}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--commit", default="HEAD")
    a = ap.parse_args()
    commit = subprocess.run(["git", "-C", str(ROOT), "rev-parse", a.commit], capture_output=True, text=True,
                            check=True).stdout.strip()
    for d in (MIRROR, PRE):
        if d.exists():
            for j in JUNCTIONS:   # never follow a junction while deleting
                p = d / j
                if os.path.isjunction(p):
                    os.rmdir(p)
            shutil.rmtree(d)
    MIRROR.mkdir(parents=True)
    ar = subprocess.Popen(["git", "-C", str(ROOT), "archive", commit], stdout=subprocess.PIPE)
    subprocess.run(["tar", "-x", "-C", str(MIRROR)], stdin=ar.stdout, check=True)
    ar.wait()
    (MIRROR / "HEAD_COMMIT").write_text(commit + "\n")
    for j in JUNCTIONS:
        p = MIRROR / j
        if p.exists():
            shutil.rmtree(p)
        p.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["cmd", "/c", "mklink", "/J", str(p), str(ROOT / j)], check=True, capture_output=True)
    report = []
    # layer W13
    for kind, r in changed(S24, C6):
        dst = MIRROR / r
        if kind == "new" or not dst.exists():
            dst.parent.mkdir(parents=True, exist_ok=True)
            dst.write_bytes(lf(S24 / r))
            report.append(("W13", r, "copied"))
        else:
            report.append(("W13", r, merge3(dst, C6 / r, S24 / r, "S2_4")))
    # wave 1 item 1.2: README section (the working tree has it when the integrator applied it; else from MERGE.md)
    readme = MIRROR / "port/test/README.md"
    if "## Body families (graphics backend, roadmap S2.1)" not in readme.read_text(encoding="utf-8"):
        md = (ROOT / "log/roadmap/S2/MERGE.md").read_text(encoding="utf-8")
        sec = md.split("### 1.2 port/test/README.md: append at the end of the file", 1)[1].split("```markdown\n", 1)[1]
        sec = sec.split("\n```\n", 1)[0]
        txt = readme.read_text(encoding="utf-8").rstrip("\n") + "\n\n" + sec + "\n"
        readme.write_bytes(txt.encode("utf-8"))
        report.append(("W1", "port/test/README.md", "appended"))
    # layer W4
    for r in WAVE4:
        dst = MIRROR / r
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes(lf(ROOT / r))
        report.append(("W4", r, "copied (working tree)"))
    refs_p = MIRROR / "re/analysis/data/scenario_refs.json"
    refs = json.loads(refs_p.read_text(encoding="utf-8"))
    wt = json.loads((ROOT / "re/analysis/data/scenario_refs.json").read_text(encoding="utf-8"))
    key = "cov_r_s2_screens"
    container = refs.get("scenarios", refs) if isinstance(refs, dict) else refs
    wt_container = wt.get("scenarios", wt)
    if key in wt_container and key not in container:
        container[key] = wt_container[key]
        refs_p.write_text(json.dumps(refs, indent=2) + "\n", encoding="utf-8")
        report.append(("W4", "re/analysis/data/scenario_refs.json", "key cov_r_s2_screens added"))
    # the patch base: copies of everything the steps touch, before they are applied
    step_changes = {k: changed(p, S24) for k, p in STEPS.items()}
    touched = sorted({r for ch in step_changes.values() for _, r in ch})
    for r in touched:
        if (MIRROR / r).exists():
            (PRE / r).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(MIRROR / r, PRE / r)
    (PRE / "TOUCHED.txt").parent.mkdir(parents=True, exist_ok=True)
    (PRE / "TOUCHED.txt").write_text("\n".join(touched) + "\n")
    # layers S2.7, S2.8, S2.9
    for k, src in STEPS.items():
        for kind, r in step_changes[k]:
            dst = MIRROR / r
            if kind == "new":
                if dst.exists() and lf(dst) != lf(src / r):
                    report.append((f"S2.{k}", r, "CONFLICT new file exists"))
                    continue
                dst.parent.mkdir(parents=True, exist_ok=True)
                dst.write_bytes(lf(src / r))
                report.append((f"S2.{k}", r, "copied"))
            else:
                report.append((f"S2.{k}", r, merge3(dst, S24 / r, src / r, f"S2_{k}")))
    for row in report:
        print("\t".join(row))


if __name__ == "__main__":
    main()
