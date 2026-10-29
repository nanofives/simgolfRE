"""S4.7: re-run another accepted step's oracle keys (layout A, no --update) on THIS step's mirror build, so the callees that step
served through the development fallback and S4.7 now owns are served by the port. Prints the fallback calls by owner.

    SG_EMU_ORACLE=1 ~/bin/py -3.12 log/roadmap/S4/7_fbcount.py 5      # S4.5 (audio_seq), also 6 (audio_chan)
"""
import importlib.util
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
n = sys.argv[1]
spec = importlib.util.spec_from_file_location(f"s4{n}_run", ROOT / "log" / "roadmap" / "S4" / f"{n}_run.py")
RUN = importlib.util.module_from_spec(spec)
spec.loader.exec_module(RUN)
RUN.DLL64 = ROOT / "port" / "build" / "S4_7_mirror" / "x64" / "Release" / "portsim_c.dll"
out = ROOT / "log" / "roadmap" / "S4" / f"7_fb_s4{n}.tsv"
sys.exit(RUN.oracle(RUN.keys_for_step(), False, "A", str(out)))
