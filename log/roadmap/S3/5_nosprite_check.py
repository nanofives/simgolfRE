"""S3.5: every sheet cell the port draws with no backend sprite, judged as port/test/test_gfx_scene.py does
(judge_no_sprite: the sheet that held the cell at the draw cuts nothing at that index, and the original's g_cellPool record is
empty in every segment start image with the same sheet layout). Run from port/build/S3_5_root with SG_PORTSIM_DLL set.
Writes log/roadmap/S3/5_nosprite_check.tsv."""
from __future__ import annotations

import pathlib
import sys

ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT))

from port.test import lib as L  # noqa: E402
from port.test import test_gfx_scene as T  # noqa: E402

SCENARIOS = ["cov_k1_paths", "cov_k1_label", "cov_k1_routing", "cov_b_decor", "cov_a_build3", "cov_b_build",
             "cov_b_career_types"]
OUT = pathlib.Path(__file__).resolve().parent / "5_nosprite_check.tsv"


def main() -> int:
    lib = L.load()
    calls = T.flic_calls(lib)
    rows = ["scenario\tkeys\tdraws\tcut_ok\tno_sheet\tdirect_records\tbad\tcells"]
    bad_total = 0
    for i, s in enumerate(SCENARIOS, 1):
        keys = T.no_sprite_keys(lib, s)
        r = T.judge_no_sprite(lib, s, keys, calls)
        cells = ",".join(f"{(k - T.CELL_POOL_VA) // T.CELL_SIZE}@{T.NO_SPRITE_WHERE.get(k, '?')}" for k in sorted(keys))
        rows.append(f"{s}\t{len(keys)}\t{sum(keys.values())}\t{r['cut_ok']}\t{r['no_sheet']}\t{r['direct']}\t"
                    f"{len(r['bad'])}\t{cells}")
        bad_total += len(r["bad"])
        verdict = "FAIL" if r["bad"] else "PASS" if keys else "EMPTY"
        print(f"[{i}/{len(SCENARIOS)}] {s} keys={len(keys)} cut_ok={r['cut_ok']} direct={r['direct']} bad={len(r['bad'])} "
              f"{verdict} {r['bad'][:3]}", flush=True)
    OUT.write_text("\n".join(rows) + "\n")
    return 1 if bad_total else 0


if __name__ == "__main__":
    sys.exit(main())
