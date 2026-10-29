"""release_due.py - is a new LAN release of the port due?

The roadmap orchestrator runs this after every merge commit (re/ROADMAP_ORCHESTRATOR.md, "The loop" step 2, MERGED) and
publishes with publish.sh when it prints DUE. The build on the mini-server is the truth: its manifest's label names the
commit it was built from ("SimGolf port <sha> (<date>)"), so no local state is kept.

Rule (Mariano, 2026-10-29): a release after every ROUNDS merge rounds, a round being a commit since the published one that
changes what the build archives (port/ or re/model/include); a single round is enough once the published build is MAX_AGE
hours old.

  py -3.12 scripts/lan_release/release_due.py [--rounds 2] [--max-age-hours 24]

Prints one line, DUE / NOT_DUE / ERROR with the reason, then the counted commits. Exit 0 = due, 1 = not due, 2 = error.
"""
import argparse
import datetime
import json
import re
import subprocess
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SERVERS = ("http://mariano-server.local:8088/simgolf", "http://192.168.68.112:8088/simgolf")
BUILD_PATHS = ("port", "re/model/include")  # what publish.sh archives


def published_manifest():
    for base in SERVERS:
        try:
            with urllib.request.urlopen(base + "/manifest.json", timeout=15) as r:
                return json.load(r), base
        except Exception:
            continue
    return None, None


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True, check=True).stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rounds", type=int, default=2)
    ap.add_argument("--max-age-hours", type=float, default=24.0)
    a = ap.parse_args()

    man, base = published_manifest()
    if man is None:
        print("ERROR no server answered with manifest.json (%s)" % ", ".join(SERVERS))
        return 2
    m = re.search(r"SimGolf port ([0-9a-f]{7,40})", man.get("label", ""))
    if not m:
        print("ERROR the published label names no commit: %r" % man.get("label"))
        return 2
    sha = m.group(1)
    try:
        commits = git("log", "--format=%h %s", "%s..HEAD" % sha, "--", *BUILD_PATHS).splitlines()
    except subprocess.CalledProcessError as e:
        print("ERROR git log %s..HEAD failed: %s" % (sha, e.stderr.strip()))
        return 2
    age_h = None
    try:
        gen = datetime.datetime.strptime(man.get("generated", ""), "%Y-%m-%d %H:%M")
        age_h = (datetime.datetime.now() - gen).total_seconds() / 3600
    except ValueError:
        pass

    n = len(commits)
    head = git("rev-parse", "--short=8", "HEAD").strip()
    where = "published %s (%s, %s h old), HEAD %s, %d round(s) since" % (
        sha, man.get("generated", "?"), "?" if age_h is None else "%.1f" % age_h, head, n)
    if n >= a.rounds:
        verdict, rc = "DUE (%d >= %d rounds)" % (n, a.rounds), 0
    elif n >= 1 and age_h is not None and age_h >= a.max_age_hours:
        verdict, rc = "DUE (%d round(s), build older than %g h)" % (n, a.max_age_hours), 0
    else:
        verdict, rc = "NOT_DUE (need %d rounds, or 1 after %g h)" % (a.rounds, a.max_age_hours), 1
    print("%s: %s" % (verdict, where))
    for c in commits:
        print("  " + c[:110])
    return rc


if __name__ == "__main__":
    sys.exit(main())
