"""Coverage census: which functions run in each phase of a named canonical scenario, per module.

    py -3.12 re/tools/coverage_census.py [--scenario sandbox_basic|course_season] [--runs 3]

Uses the shim's INT3 coverage (shim/src/coverage.cpp) over every Ghidra function entry of the modules in
MODULE_TSVS. Phase 0 is always boot (spawn -> main menu); the scenario's phases follow (re/tools/scenario.py).
Writes re/coverage/<scenario>.tsv: module, addr, phases (comma list), runs_seen (of --runs).
"""
import argparse
import collections
import pathlib
import sys
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
from game import Game  # noqa: E402
import imgcmp  # noqa: E402
import scenario  # noqa: E402

TAB = "\t"
MODULE_TSVS = {"golf_clean.exe": "functions_ghidra.tsv", "Terrain.dll": "functions_ghidra_Terrain.dll.tsv",
               "jgld.dll": "functions_ghidra_jgld.dll.tsv", "sound.dll": "functions_ghidra_sound.dll.tsv"}


def function_lists() -> dict[str, list[str]]:
    out = {}
    for mod, name in MODULE_TSVS.items():
        f = ROOT / "re" / name
        if f.exists():
            out[mod] = [r.split(TAB)[0] for r in f.read_text().splitlines()[1:]]
    return out


def entries_file(skip: frozenset = frozenset()) -> pathlib.Path:
    """Every function entry to arm; `skip` = golf_clean.exe entries a scenario hooks with Frida
    (an INT3 and a Frida inline hook must not share a first byte)."""
    out = pathlib.Path(tempfile.gettempdir()) / "simgolf_cov_entries.txt"
    out.write_text("".join(f"{mod}{TAB}{a}\n" for mod, addrs in function_lists().items() for a in addrs
                           if not (mod == "golf_clean.exe" and a in skip)))
    return out


def one_run(name: str) -> tuple[dict[tuple[str, str], int], list[str]]:
    spec = scenario.SCENARIOS[name]
    phases = ["boot"] + [p for p, _ in spec["phases"]]
    dump = pathlib.Path(tempfile.gettempdir()) / f"simgolf_cov_{time.time_ns()}.tsv"
    watched = {f"{a:08x}": ph for a, ph in spec.get("watched", {}).items()}
    with Game(env={"SIMGOLF_SKIP_INTRO": "1", **spec["env"]}, coverage=entries_file(set(watched)),
              extra_js=spec.get("js", [])) as g:
        g.wait_window()
        imgcmp.wait_for(g, imgcmp.load(scenario.GOLDEN / "main_menu.png"), timeout=60)
        for i, (_, step) in enumerate(spec["phases"], start=1):
            g.cov_phase(i)
            step(g)
        assert g.alive, g.detach_reason
        g.cov_dump(dump)
        seen_watched = {a for a in watched if scenario.events_script(g).calls(int(a, 16))} if watched else set()
    out = {}
    for line in dump.read_text().splitlines()[1:]:
        mod, addr, mask = line.split(TAB)
        out[(mod, addr)] = int(mask, 16)
    for a in seen_watched:
        out[("golf_clean.exe", a)] = 1 << phases.index(watched[a])
    return out, phases


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--scenario", default="sandbox_basic", choices=sorted(scenario.SCENARIOS))
    ap.add_argument("--runs", type=int, default=1)
    a = ap.parse_args()
    out_path = ROOT / "re" / "coverage" / f"{a.scenario}.tsv"
    seen = collections.defaultdict(lambda: [0, 0])
    phases = []
    for i in range(a.runs):
        r, phases = one_run(a.scenario)
        for key, mask in r.items():
            seen[key][0] |= mask
            seen[key][1] += 1
        print(f"run {i + 1}: {len(r)} functions", flush=True)
    lists = function_lists()
    with open(out_path, "w") as f:
        f.write("module\taddr\tphases\truns_seen\n")
        for (mod, addr) in sorted(seen):
            mask, runs = seen[(mod, addr)]
            ph = ",".join(p for i, p in enumerate(phases) if mask >> i & 1)
            f.write(f"{mod}\t{addr}\t{ph}\t{runs}/{a.runs}\n")
    for mod, addrs in lists.items():
        n = sum(1 for (m, _) in seen if m == mod)
        per = collections.Counter(p for (m, _), (mask, _) in seen.items() if m == mod
                                  for i, p in enumerate(phases) if mask >> i & 1)
        print(f"  {mod}: {n}/{len(addrs)} functions ran ({100 * n / len(addrs):.1f}%)  per phase {dict(per)}")
    print(f"-> {out_path}")


if __name__ == "__main__":
    main()
