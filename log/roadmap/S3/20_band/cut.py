"""S3.20 band measurement: per-band cuts of a recorded pass (the 20_make_ref.py cut applied to horizontal bands).

For a corpus step and N, writes <out>/<step>/b<k>of<N>.txt: the pass's modelview calls, then, in the recorded order, every triangle whose
window bounding box comes within MARGIN pixels of band k's rows [H*k/N, H*(k+1)/N) (memory row = GL window y), with the texture binds
and the current normal it was drawn with (as 20_make_ref.py), then glPopMatrix / glFlush. glband.exe reads them with GLBAND_CUT=<out>.

    ~/bin/py -3.12 log/roadmap/S3/20_band/cut.py <out> <step> <N> [<N> ...]
"""
import importlib.util
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("mr", HERE.parent / "20_make_ref.py")
M = importlib.util.module_from_spec(spec)
spec.loader.exec_module(M)
import numpy as np  # noqa: E402

MARGIN = 3.0


def triangles(step):
    d = M.CORPUS / "c7" / "snaps" / step
    stream = (M.CORPUS / "c7" / "stream.gl.txt").read_text().splitlines()
    proj = M.projection_at(stream, M.meta(d)["stateIdx"])
    vtx = np.frombuffer((d / "vtx.bin").read_bytes(), dtype="<u4").reshape(-1, 3)
    lines = (d / "pass.gl.txt").read_text().splitlines()
    head, mv, tris, cur, bound_line, normal_line = [], np.eye(4), [], None, None, None
    state = {}   # glEnable / glDisable per capability and glBlendFunc: the last line of each, carried to every triangle
    for ln in lines:
        op = ln.split(" ", 1)[0]
        a = M.args_of(ln)
        if op in ("glLoadIdentity", "glPushMatrix", "glRotated", "glRotatef", "glTranslatef") and cur is None:
            head.append(ln)
            if op == "glLoadIdentity":
                mv = np.eye(4)
            elif op == "glRotated":
                mv = mv @ M.rot(*[M.f64(a[2 * i], a[2 * i + 1]) for i in range(4)])
            elif op == "glRotatef":
                mv = mv @ M.rot(*[M.f32(v) for v in a])
            elif op == "glTranslatef":
                t = np.eye(4); t[:3, 3] = [M.f32(v) for v in a]; mv = mv @ t
        elif op == "glBindTexture":
            bound_line = ln
        elif op in ("glEnable", "glDisable") and cur is None:
            state[("cap", a[0])] = ln
        elif op == "glBlendFunc" and cur is None:
            state[("blend",)] = ln
        elif op == "glBegin":
            cur = {"bind": bound_line, "normal_in": normal_line, "lines": [ln], "pts": [], "state": dict(state)}
        elif op == "glEnd":
            cur["lines"].append(ln); tris.append(cur); cur = None
        elif cur is not None:
            cur["lines"].append(ln)
            if op == "glArrayElement":
                cur["pts"].append([M.f32(v) for v in vtx[a[0]]])
                normal_line = None   # the array's normal (unknown as a line; restored only for immediate triangles below)
            elif op == "glVertex3fv":
                cur["pts"].append([M.f32(v) for v in M.vals_of(ln)])
            elif op == "glVertex3f":
                cur["pts"].append([M.f32(v) for v in a])
            elif op == "glNormal3fv":
                normal_line = ln
        elif op == "glNormal3fv":
            normal_line = ln
    full = proj @ mv
    for t in tris:
        ys = []
        for p in t["pts"]:
            c = full @ np.array([p[0], p[1], p[2], 1.0])
            ys.append((c[1] / c[3] + 1) * M.H / 2)
        t["ylo"], t["yhi"] = (min(ys), max(ys)) if ys else (float("inf"), float("-inf"))   # an empty glBegin / glEnd draws nothing
    return head, tris


def write(out, step, n, head, tris):
    d = pathlib.Path(out) / step
    d.mkdir(parents=True, exist_ok=True)
    for k in range(n):
        y0, y1 = M.H * k // n, M.H * (k + 1) // n
        lines, bound, kept, emitted = list(head), None, 0, {}
        for t in tris:
            if t["yhi"] < y0 - MARGIN or t["ylo"] > y1 + MARGIN:
                continue
            for k2, ln2 in t["state"].items():
                if emitted.get(k2) != ln2:
                    lines.append(ln2); emitted[k2] = ln2
            if t["bind"] != bound and t["bind"] is not None:
                lines.append(t["bind"]); bound = t["bind"]
            if any(l.startswith("glVertex3fv") for l in t["lines"]) and t["normal_in"] and not any(l.startswith("glNormal3fv") for l in t["lines"][:2]):
                lines.append(t["normal_in"])
            lines += t["lines"]; kept += 1
        lines += ["glPopMatrix ", "glFlush "]
        (d / f"b{k}of{n}.txt").write_text("\n".join(lines) + "\n", newline="\n")
        print(f"{step} band {k}/{n} rows {y0}..{y1}: {kept} of {len(tris)} triangles")


def main():
    out, step, ns = sys.argv[1], sys.argv[2], [int(x) for x in sys.argv[3:]]
    head, tris = triangles(step)
    for n in ns:
        write(out, step, n, head, tris)


if __name__ == "__main__":
    main()
