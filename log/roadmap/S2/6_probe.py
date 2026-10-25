"""S2.6 probe runner: imports re/tools/scenario_s2_6.py (scenario.py's hook globs only scenario_s1_*.py until the
orchestrator widens it), then runs scenario_refs.py's command line. Used only for exploratory record runs with a scratch
schedule (--schedule outside log/c3/alt/refs) to measure click positions; never for the pinned references."""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "re" / "frida"))
sys.path.insert(0, str(ROOT / "re" / "tools"))
import scenario  # noqa: E402,F401
import scenario_s2_6  # noqa: E402,F401
import scenario_refs  # noqa: E402

sys.exit(scenario_refs.main())
