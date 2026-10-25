"""I6: S3 wave 2's defect row (MERGE_w2.md section 4) as a generator input of log/roadmap/S6/merge_decisions.py:
writes log/roadmap/S6/s3_4b_decisions.tsv (header of 6_decisions.tsv, one row D53)."""
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[4]
S6 = ROOT / "log" / "roadmap" / "S6"
h = (S6 / "6_decisions.tsv").read_text(encoding="utf-8").splitlines()[0].split("\t")
row = {
    "id": "D53",
    "source": "roadmap S3.4 / S3.4b (U-1330, U-1331)",
    "function": "loadWorldAssets (loadFlic sites 0x00441e13 .. 0x00441e7f)",
    "summary": r"the exe names trees\Tropic\Tree Cerc / Tree Drac (with a space, v1.00 and v1.03), the install, the v1.03 patch and "
               r"the InstallShield cab hold Tree_Cerc / Tree_Drac: the original dies at the first tropical tree load",
    "class": "fault", "changes_save": "no", "changes_balance": "no",
    "changes_sim": "yes", "changes_frame": "yes", "hang": "no", "fault": "yes",
    "effect_note": "tropical courses only: the sheet's columns decide later cell allocations (sim state through the cell addresses "
                   "mainLoop stores) and the trees draw; the divergences.tsv line for tropical courses becomes: original profile = "
                   "Fault, as the original",
    "recommendation": "fix",
    "switch": "flic.treeAliasNames",
    "switch_state": "implemented",
    "evidence": r"STOCK-GAME MEASUREMENT scenario cov_b_s34_hawaii, run `stock`, hooks off, instance instances\s3_4, log "
                r"log/roadmap/S3/4_rec_hawaii_stock.log, recording port/corpus/flic/cov_b_s34_hawaii/stock: flicOpenStream on "
                r"`flics\trees\Tropic\Tree Cerc\Cerc_Small.flc` (slot 0x192) returns 0, count 0 stored, the game ends within 180 s; "
                r"c3fyb's exception report for the same load: access violation reading 0x8 at 0x00473e67 (Image_draw on the NULL "
                r"frame Surface, from 0x0043d990, called from 0x00441e18); the alias run (copies under the exe's names in the "
                r"instance only) 4_rec_hawaii_alias.log loads all 114 tropical world loads; port tests port/test/test_defects_flic.py "
                r"(8: fault probe at 1 with no backend, completing control byte-equal at 0, sandbox_basic identical at 1 and 0, "
                r"negctl 5 RED); decision D5 option a",
    "uncertain": "U-1330, U-1331",
}
assert set(row) == set(h), set(h) ^ set(row)
assert not any("\t" in v or "\n" in v for v in row.values())
(S6 / "s3_4b_decisions.tsv").write_text("\t".join(h) + "\n" + "\t".join(row[c] for c in h) + "\n", encoding="utf-8", newline="\n")
print("wrote", S6 / "s3_4b_decisions.tsv", len(h), "columns")
