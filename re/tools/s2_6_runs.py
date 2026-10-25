r"""Roadmap S2.6 driver (log/roadmap/S2/6_notes.md): re/tools/scenario_refs.py and re/tools/segment_corpus.py run unchanged,
with re/tools/scenario_s2_6.py imported first (its scenario cov_r_s2_screens registers into scenario.SCENARIOS, which the
end-of-file hook of scenario.py does not do for scenario_s2_*.py files until the glob is widened: log/roadmap/S2/6_merge.md).

Both tools re-launch their own file for every boot (scenario_refs' Pool: `<py> scenario_refs.py run ...`; segment_corpus' _sub:
`<py> segment_corpus.py ...`). The driver points each module's __file__ at itself and passes the tool name to the children in
S2_6_TOOL, so every child imports scenario_s2_6 too.

    ~/bin/py -3.12 re/tools/s2_6_runs.py refs regen cov_r_s2_screens --rerecord --jobs 1 --instances s2_6
    ~/bin/py -3.12 re/tools/s2_6_runs.py seg --instance s2_6 capture --scenario cov_r_s2_screens --label c_a
    ~/bin/py -3.12 re/tools/s2_6_runs.py seg --instance s2_6 trace --scenario cov_r_s2_screens
    ~/bin/py -3.12 re/tools/s2_6_runs.py trace replay --scenario cov_r_s2_screens --trace port/corpus/traces/cov_r_s2_screens.trace --instance s2_6

Every boot is on instances\<name> (never original\): scenario_refs.setup points scenario_ab at the instance given by --instance.
"""
from __future__ import annotations

import os
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve()
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE.parents[1] / "frida"))

import scenario  # noqa: E402,F401
import scenario_s2_6  # noqa: E402,F401  (registers cov_r_s2_screens into scenario.SCENARIOS)

TOOLS = {"refs": "scenario_refs", "seg": "segment_corpus", "trace": "trace_tool"}


def main() -> int:
    tool = None
    if len(sys.argv) > 1 and sys.argv[1] in TOOLS:
        tool = sys.argv.pop(1)
        os.environ["S2_6_TOOL"] = tool
    else:
        tool = os.environ.get("S2_6_TOOL")          # a child re-launched by the tool through __file__
    if tool not in TOOLS:
        raise SystemExit("usage: s2_6_runs.py refs|seg|trace <the tool's own arguments>")
    mod = __import__(TOOLS[tool])
    mod.__file__ = str(HERE)                          # the tool's children re-launch this driver
    return mod.main() or 0


if __name__ == "__main__":
    sys.exit(main())
