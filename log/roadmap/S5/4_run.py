"""S5.4: run port.test.boot / integrator / gfx-style tools with the patched selection copies (4_patch.py write first).

    ~/bin/py -3.12 log/roadmap/S5/4_run.py boot|integrator <args...>      (SG_TEXT_METRICS selects the provider)

Sets S5_4_PATCH=1 and PYTHONPATH=log/roadmap/S5/4_site (sitecustomize loads the copies in this process and in every child).
"""
import os
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def main():
    which, args = sys.argv[1], sys.argv[2:]
    if os.environ.get("S5_4_PATCH") != "1":
        env = dict(os.environ, S5_4_PATCH="1", PYTHONPATH=str(HERE / "4_site") + os.pathsep + os.environ.get("PYTHONPATH", ""))
        return subprocess.call([sys.executable, str(pathlib.Path(__file__).resolve()), which] + args, env=env, cwd=str(ROOT))
    import importlib
    mod = importlib.import_module(f"port.test.{which}")
    return mod.main(args)


if __name__ == "__main__":
    sys.exit(main())
