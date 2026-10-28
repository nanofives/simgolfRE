"""I13: append S3.13's U-1393 (13_uncertain.tsv) and S3.14's U-1394..U-1398 (14_uncertain.tsv) to <root>/UNCERTAINTIES.md after the
last `| U-` row, through 11_assemble.py's tsv_rows_by_id (rows_from_tsv + addr_cell) and Root.unc_rows (14_merge.md section 4).

    ~/bin/py -3.12 log/roadmap/merges/I13/unc_rows.py <root> [--dry]"""
import importlib.util
import pathlib
import sys

S3 = pathlib.Path(__file__).resolve().parents[2] / "S3"
spec = importlib.util.spec_from_file_location("s3_11_assemble", S3 / "11_assemble.py")
A = importlib.util.module_from_spec(spec)
spec.loader.exec_module(A)

root = pathlib.Path(sys.argv[1]).resolve()
dry = "--dry" in sys.argv[2:]
R = A.Root(root, dry)
R.unc_rows(list(A.tsv_rows_by_id(S3 / "13_uncertain.tsv").values()), "s313 U-1393")
R.unc_rows(list(A.tsv_rows_by_id(S3 / "14_uncertain.tsv").values()), "s314 U-1394..U-1398")
R.flush()
print("done", root, "dry" if dry else "")
