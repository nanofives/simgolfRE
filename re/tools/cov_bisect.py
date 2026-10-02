"""Delta debugging for coverage breakpoints: find which function entries make the game hang when
an INT3 is planted on them. Runs the game WITHOUT Frida (plain process + shim coverage).

    py -3.12 re/tools/cov_bisect.py [--timeout 25]

A trial passes when the main menu appears. Prints the minimal culprit entries.
"""
import argparse, os, pathlib, subprocess, sys, tempfile, time
ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import game, imgcmp  # noqa: E402

REF = imgcmp.load(ROOT / "tests" / "golden" / "main_menu.png")
TIMEOUT = 25
trials = 0


def trial(entries, label):
    global trials
    trials += 1
    f = pathlib.Path(tempfile.gettempdir()) / "simgolf_cov_bisect.txt"
    f.write_text("".join(f"golf_clean.exe\t{a}\n" for a in entries))
    env = dict(os.environ, SIMGOLF_COVERAGE=str(f), SIMGOLF_SKIP_INTRO="1", SIMGOLF_WINDOWED="1")
    if game.Game.running_instances():
        raise SystemExit("SimGolf already running")
    p = subprocess.Popen([str(game.GAME_DIR / "golf_clean.exe")], cwd=game.GAME_DIR, env=env)
    ok, t0 = False, time.time()
    try:
        while time.time() - t0 < TIMEOUT and p.poll() is None:
            if game._enum_windows(p.pid, "JackalClass"):
                g = game.Game(); g.pid = p.pid
                try:
                    if imgcmp.matches(g.shot(), REF):
                        ok = True
                        break
                except Exception:
                    pass
            time.sleep(0.5)
    finally:
        p.kill(); p.wait(10)
        while game.Game.running_instances():
            time.sleep(0.2)
    print(f"[{trials:3}] {label}: {len(entries)} entries -> {'menu' if ok else 'HANG'}", flush=True)
    return ok


def bisect(entries):
    if len(entries) == 1:
        return list(entries)
    h = len(entries) // 2
    a, b = entries[:h], entries[h:]
    fa = not trial(a, f"{a[0]}..{a[-1]}")
    fb = not trial(b, f"{b[0]}..{b[-1]}")
    out = []
    if fa:
        out += bisect(a)
    if fb:
        out += bisect(b)
    if not fa and not fb:
        out.append(f"INTERACTION({a[0]}..{b[-1]})")
    return out


def ddmin(entries):
    """Zeller's ddmin: a 1-minimal subset of `entries` that still hangs (handles interactions)."""
    n = 2
    while len(entries) >= 2:
        chunk = len(entries) // n
        subsets = [entries[i:i + chunk] for i in range(0, len(entries), chunk)]
        reduced = False
        for sub in subsets:
            if not trial(sub, f"subset {sub[0]}..{sub[-1]}"):
                entries, n, reduced = sub, 2, True
                break
        if not reduced:
            for sub in subsets:
                comp = [e for e in entries if e not in set(sub)]
                if not trial(comp, f"complement -{sub[0]}..{sub[-1]}"):
                    entries, n, reduced = comp, max(n - 1, 2), True
                    break
        if not reduced:
            if n >= len(entries):
                break
            n = min(len(entries), n * 2)
    return entries


def main():
    global TIMEOUT
    ap = argparse.ArgumentParser(); ap.add_argument("--timeout", type=float, default=25)
    TIMEOUT = ap.parse_args().timeout
    allf = [l.split("\t")[0] for l in (ROOT / "re" / "functions_ghidra.tsv").read_text().splitlines()[1:]]
    if trial(allf, "all"):
        print("no hang with all entries"); return
    start = allf[:len(allf) // 2]  # first half hangs on its own (trial 2 of the bisect run)
    culprits = ddmin(start if not trial(start, "first half") else allf)
    print("CULPRITS", culprits, f"({trials} trials)")


if __name__ == "__main__":
    main()
