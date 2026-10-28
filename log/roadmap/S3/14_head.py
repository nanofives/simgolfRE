"""S3.14: a plain archive of a commit (default d66aa755) with 11_assemble.py's junctions, no S3 files or edits.

    ~/bin/py -3.12 log/roadmap/S3/14_head.py [--dst S3_14_head] [--commit d66aa755] [--recopy]
"""
import argparse
import importlib.util
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("s3_11_assemble", HERE / "11_assemble.py")
A = importlib.util.module_from_spec(spec)
spec.loader.exec_module(A)

ap = argparse.ArgumentParser()
ap.add_argument("--dst", default="S3_14_head")
ap.add_argument("--commit", default="d66aa755")
ap.add_argument("--recopy", action="store_true")
a = ap.parse_args()
A.make_tree(A.ROOT / "port" / "build" / a.dst, a.commit, a.recopy)
