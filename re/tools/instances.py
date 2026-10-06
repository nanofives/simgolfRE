"""Extra copies of the portable install for parallel A/B runs (CLAUDE.md, parallel C3).

    py -3.12 re/tools/instances.py create c3a        # copy original\ -> instances\c3a (no logs), ~266 MB
    py -3.12 re/tools/instances.py list
    py -3.12 re/tools/instances.py env c3a           # print the env settings for that copy (bash syntax)

The game has no single-instance guard; a copy only needs the DWM8And16BitMitigation compatibility layer, which
re/tools/game.py passes as __COMPAT_LAYER (the registry flag is keyed to original\\golf_clean.exe's path). Keep copies
under instances\ (short path, gitignored): a copy under the long Temp scratchpad path failed to start."""
import argparse, pathlib, shutil, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = ROOT / "original"
DST = ROOT / "instances"
SKIP = {"simgolf_shim.log"}


def create(name):
    dst = DST / name
    if dst.exists():
        print(f"{dst} exists")
        return 0
    shutil.copytree(SRC, dst, ignore=lambda d, names: [n for n in names if n in SKIP])
    print(f"created {dst}")
    return 0


def env(name):
    d = (DST / name).as_posix()
    print(f"export SIMGOLF_ALLOW_MULTI=1 SIMGOLF_GAME_DIR='{d}'")
    print(f"# build:  SIMGOLF_BUILD_DIR=build_{name} SIMGOLF_DEPLOY_DIR='..\\instances\\{name}' "
          f"SIMGOLF_RE_BATCHES=<file listing your batch> shim\\build.bat")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["create", "list", "env"])
    ap.add_argument("name", nargs="?")
    a = ap.parse_args()
    if a.cmd == "list":
        for d in sorted(DST.glob("*")) if DST.exists() else []:
            print(d.name)
        return 0
    if not a.name:
        raise SystemExit("name required")
    return create(a.name) if a.cmd == "create" else env(a.name)


if __name__ == "__main__":
    sys.exit(main())
