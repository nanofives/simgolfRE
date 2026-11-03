"""the two census rows' dumped calls (brush_465: cov_a_build 6630 / 6631 / 6633; k_dlg_R: cov_a_keys 2397) through the real GL and the
C++ body under rules 2, 8, 9: differing px against the real GL"""
import importlib.util, sys, pathlib
import numpy as np
sys.path.insert(0, str(pathlib.Path.cwd()))
HERE = pathlib.Path("../../../log/roadmap/S3").resolve()
spec = importlib.util.spec_from_file_location("cc", HERE / "31_cppcheck.py"); CC = importlib.util.module_from_spec(spec); sys.argv = ["x"]; spec.loader.exec_module(CC)
P, L = CC.P, CC.L
lib = L.load()
for dump, idx in (("cov_a_build", [6630, 6631, 6633]), ("cov_a_keys", [2397])):
    recs = P.read_dump(str(HERE / "24_data" / f"dump_{dump}.bin"))
    sel = [recs[i] for i in idx]
    real = P.run_real(sel, "s331_rows")
    with lib.state() as st:
        st.map(CC.BUF, CC.CBUF)
        prev = st.call("overlay.setRule", [9]).eax
        for i, r, rl in zip(idx, sel, real):
            r["real"] = rl
            out = []
            for rule in (2, 8, 9):
                c = CC.cpp(lib, st, r, rule)
                d = P.diff5(c, rl)
                out.append("rule %d: %d px" % (rule, int((d > 0).sum())))
            print(dump, i, (r["x1"], r["y1"], r["x2"], r["y2"]), "w", r["width"], "; ".join(out))
        st.call("overlay.setRule", [prev])
