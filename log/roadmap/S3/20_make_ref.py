"""S3.20: the reference pass of the host-GL startup self-check (port/src/sim/gl_wgl_ref.inc), cut from one pass the original drew.

The ORIGINAL's recorded pass (port/corpus/gl/<label>/snaps/<step>: pass.gl.txt = its GL calls, vtx.bin / nrm.bin = Terrain.dll's
vertex and normal arrays, dib_pre.bin / dib_post.bin = the 16-bit 555 DIB before the pass and after its glFlush) is cut to a small
rectangle R of the window: every triangle whose window-space bounding box (the recorded modelview of the pass, the projection the
recorded state stream left, the 800 x 600 viewport) comes within MARGIN pixels of R is kept, in the recorded order, with the
texture binds and the current normal it was drawn with; every other triangle is left out. A triangle left out does not reach R, and
no depth test or stencil runs in a terrain pass (Terrain::initGL enables neither), so R's pixels after the cut pass equal R's pixels
after the whole pass. The expected value pinned in the port is SHA-256 of the ORIGINAL's dib_post over R (rows y0..y1 in memory
order, each row's 555 words little-endian): the original's output, not a host's. The textures are not in the file: the port loads
them from the user's install through its own GL setup (terrain_gl_setup.h), as the app does.

    ~/bin/py -3.12 log/roadmap/S3/20_make_ref.py [--label c7] [--step z2_r1] [--rect x,y,w,h] [--out <inc>] [--check]

--check regenerates in memory and fails when the committed file differs. The data written is the cut geometry (float bit patterns
as decimal integers: vertex positions, normals, texture coordinates), the pass's matrix calls, the texture names it binds and the
expected hash; no texture, lighting value or other byte of the game's files.
"""
from __future__ import annotations

import argparse
import hashlib
import math
import pathlib
import struct
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[3]
CORPUS = ROOT / "port" / "corpus" / "gl"
W, H = 800, 600
MARGIN = 3.0

# op codes of the reference program (gl_wgl.cpp ref_run reads them)
OP_BIND, OP_BEGIN, OP_END, OP_TC2FV, OP_ARRAY, OP_NORMAL, OP_TC2F, OP_VERTEX = range(1, 9)


def f32(u: int) -> float:
    return struct.unpack("<f", struct.pack("<I", u & 0xFFFFFFFF))[0]


def f64(lo: int, hi: int) -> float:
    return struct.unpack("<d", struct.pack("<II", lo & 0xFFFFFFFF, hi & 0xFFFFFFFF))[0]


def args_of(line: str) -> list[int]:
    parts = line.split(" ", 1)
    if len(parts) < 2 or not parts[1].strip():
        return []
    return [int(x, 16) for x in parts[1].split(" ")[0].split(",") if x]


def vals_of(line: str) -> list[int]:
    i = line.find("v=")
    return [int(x, 16) for x in line[i + 2:].split(",")] if i >= 0 else []


def rot(angle_deg: float, x: float, y: float, z: float) -> np.ndarray:
    n = math.sqrt(x * x + y * y + z * z)
    x, y, z = x / n, y / n, z / n
    c, s = math.cos(math.radians(angle_deg)), math.sin(math.radians(angle_deg))
    m = np.eye(4)
    m[:3, :3] = [[x * x * (1 - c) + c, x * y * (1 - c) - z * s, x * z * (1 - c) + y * s],
                 [y * x * (1 - c) + z * s, y * y * (1 - c) + c, y * z * (1 - c) - x * s],
                 [x * z * (1 - c) - y * s, y * z * (1 - c) + x * s, z * z * (1 - c) + c]]
    return m


def ortho(l, r, b, t, n, f) -> np.ndarray:
    m = np.eye(4)
    m[0, 0], m[1, 1], m[2, 2] = 2 / (r - l), 2 / (t - b), -2 / (f - n)
    m[0, 3], m[1, 3], m[2, 3] = -(r + l) / (r - l), -(t + b) / (t - b), -(f + n) / (f - n)
    return m


def projection_at(stream: list[str], end: int) -> np.ndarray:
    """The projection matrix the recorded state stream leaves at line `end` (matrix mode, identity, ortho, push / pop)."""
    mode = 0x1700
    stk = {0x1700: [np.eye(4)], 0x1701: [np.eye(4)]}
    for ln in stream[:end]:
        op = ln.split(" ", 1)[0]
        if op == "glMatrixMode":
            mode = args_of(ln)[0]
        elif op == "glLoadIdentity":
            stk[mode][-1] = np.eye(4)
        elif op == "glPushMatrix":
            stk[mode].append(stk[mode][-1].copy())
        elif op == "glPopMatrix":
            stk[mode].pop()
        elif op == "glOrtho":
            a = args_of(ln)
            d = [f64(a[2 * i], a[2 * i + 1]) for i in range(6)]
            stk[mode][-1] = stk[mode][-1] @ ortho(*d)
    return stk[0x1701][-1]


def meta(d: pathlib.Path) -> dict[str, int]:
    out = {}
    for ln in (d / "meta.txt").read_text().splitlines():
        k, _, v = ln.partition(" ")
        out[k] = int(v)
    return out


def build(label: str, step: str, rect: tuple[int, int, int, int]):
    root = CORPUS / label
    d = root / "snaps" / step
    stream = (root / "stream.gl.txt").read_text().splitlines()
    m = meta(d)
    proj = projection_at(stream, m["stateIdx"])
    vtx = np.frombuffer((d / "vtx.bin").read_bytes(), dtype="<u4").reshape(-1, 3)
    nrm = np.frombuffer((d / "nrm.bin").read_bytes(), dtype="<u4").reshape(-1, 3)
    pre = np.frombuffer((d / "dib_pre.bin").read_bytes(), dtype="<u2").reshape(H, W)
    post = np.frombuffer((d / "dib_post.bin").read_bytes(), dtype="<u2").reshape(H, W)
    x0, y0, rw, rh = rect
    if pre[y0:y0 + rh, x0:x0 + rw].any():
        sys.exit("the rectangle's pixels before the pass are not all 0 (the self-check starts from a cleared DIB)")
    lines = (d / "pass.gl.txt").read_text().splitlines()
    head, mv, bound, normal = [], np.eye(4), None, None
    tris = []          # (bound name, normal before it, [(kind, tc bits, vertex payload)])
    cur = None
    for ln in lines:
        op = ln.split(" ", 1)[0]
        a = args_of(ln)
        if op == "glLoadIdentity":
            head.append(("li",)); mv = np.eye(4)
        elif op == "glPushMatrix":
            head.append(("push",))
        elif op == "glPopMatrix":
            pass
        elif op == "glRotated":
            head.append(("rotd", a))
            mv = mv @ rot(*[f64(a[2 * i], a[2 * i + 1]) for i in range(4)])
        elif op == "glRotatef":
            head.append(("rotf", a))
            mv = mv @ rot(*[f32(v) for v in a])
        elif op == "glTranslatef":
            head.append(("trf", a))
            t = np.eye(4); t[:3, 3] = [f32(v) for v in a]; mv = mv @ t
        elif op == "glBindTexture":
            bound = a[1]
        elif op == "glBegin":
            if a[0] != 4:
                sys.exit(f"glBegin {a[0]:#x}: only GL_TRIANGLES is cut")
            cur = {"bound": bound, "normal_in": normal, "verts": [], "tc": None}
        elif op == "glTexCoord2fv":
            cur["tc"] = ("fv", tuple(vals_of(ln)))
        elif op == "glTexCoord2f":
            cur["tc"] = ("f", tuple(a))
        elif op == "glArrayElement":
            i = a[0]
            cur["verts"].append(("array", cur["tc"], i))
            normal = tuple(int(v) for v in nrm[i])
        elif op == "glNormal3fv":
            normal = tuple(vals_of(ln))
            if cur is not None:
                cur.setdefault("normals", []).append((len(cur["verts"]), normal))
        elif op == "glVertex3fv":
            cur["verts"].append(("imm", cur["tc"], tuple(vals_of(ln))))
        elif op == "glEnd":
            tris.append(cur); cur = None
        elif op == "glFlush":
            pass
        else:
            sys.exit(f"pass line not handled: {ln}")
    full = proj @ mv

    def win(p):
        c = full @ np.array([p[0], p[1], p[2], 1.0])
        return (c[0] / c[3] + 1) * W / 2, (c[1] / c[3] + 1) * H / 2

    keep = []
    for t in tris:
        pts = [win([f32(v) for v in (vtx[x[2]] if x[0] == "array" else x[2])]) for x in t["verts"]]
        xs, ys = [p[0] for p in pts], [p[1] for p in pts]
        if max(xs) >= x0 - MARGIN and min(xs) <= x0 + rw + MARGIN and max(ys) >= y0 - MARGIN and min(ys) <= y0 + rh + MARGIN:
            keep.append(t)
    # the program: compact arrays, binds only when the name changes, the current normal restored before an immediate vertex
    vmap, verts, norms, prog = {}, [], [], []
    cur_bound, cur_normal = None, None   # the program's state as it runs
    for t in keep:
        if t["bound"] != cur_bound:
            prog += [OP_BIND, t["bound"]]; cur_bound = t["bound"]
        need_n = any(x[0] == "imm" for x in t["verts"]) and not t.get("normals")
        if need_n and t["normal_in"] is not None and t["normal_in"] != cur_normal:
            prog += [OP_NORMAL, *t["normal_in"]]; cur_normal = t["normal_in"]
        prog.append(OP_BEGIN)
        nlist = dict(t.get("normals", []))
        for k, (kind, tc, payload) in enumerate(t["verts"]):
            if k in nlist:
                prog += [OP_NORMAL, *nlist[k]]; cur_normal = nlist[k]
            prog += ([OP_TC2FV] if tc[0] == "fv" else [OP_TC2F]) + list(tc[1])
            if kind == "array":
                if payload not in vmap:
                    vmap[payload] = len(verts)
                    verts.append([int(v) for v in vtx[payload]]); norms.append([int(v) for v in nrm[payload]])
                prog += [OP_ARRAY, vmap[payload]]
                cur_normal = tuple(norms[vmap[payload]])
            else:
                prog += [OP_VERTEX, *payload]
        if len(nlist) > 0 and max(nlist) >= len(t["verts"]):
            sys.exit("a glNormal3fv after the last vertex of a triangle")
        prog.append(OP_END)
    rect_px = post[y0:y0 + rh, x0:x0 + rw]
    sha = hashlib.sha256(rect_px.astype("<u2").tobytes()).hexdigest()
    return {"head": head, "verts": verts, "norms": norms, "prog": prog, "sha": sha, "kept": len(keep), "of": len(tris),
            "zoom": int.from_bytes((d / "tobj.bin").read_bytes()[0x1C:0x20], "little", signed=True), "rect": rect,
            "label": label, "step": step, "changed": int((rect_px != 0).sum())}


def emit(r) -> str:
    x0, y0, rw, rh = r["rect"]
    out = [
        f"// GENERATED by log/roadmap/S3/20_make_ref.py from port/corpus/gl/{r['label']}/snaps/{r['step']} (the original's recorded",
        "// terrain pass); do not edit. The reference pass of the host-GL startup self-check (gl_wgl.h wgl_self_check): the",
        f"// {r['kept']} of the pass's {r['of']} triangles that reach the rectangle below, in the recorded order. Float values are",
        "// their bit patterns (decimal). kRefSha256 = SHA-256 of the ORIGINAL's DIB after the pass over the rectangle (rows y0 .. y0 + h - 1",
        "// in memory order, each 555 word little-endian).",
        f"constexpr int kRefZoom = {r['zoom']};   // Terrain +0x1c of the recorded pass (0 / 1 / 2)",
        f"constexpr int kRefX = {x0}, kRefY = {y0}, kRefW = {rw}, kRefH = {rh};",
        f"constexpr int kRefChanged = {r['changed']};   // pixels of the rectangle the original's pass wrote (nonzero after it)",
        f"constexpr char kRefSha256[] = \"{r['sha']}\";",
    ]
    hd = []
    for h in r["head"]:
        if h[0] == "li":
            hd.append("0")
        elif h[0] == "push":
            hd.append("1")
        elif h[0] == "rotd":
            hd.append("2, " + ", ".join(str(v) for v in h[1]))
        elif h[0] == "rotf":
            hd.append("3, " + ", ".join(str(v) for v in h[1]))
        elif h[0] == "trf":
            hd.append("4, " + ", ".join(str(v) for v in h[1]))
    out.append("// the pass's modelview calls: 0 glLoadIdentity, 1 glPushMatrix, 2 glRotated (4 doubles as lo, hi dwords), 3 glRotatef,")
    out.append("// 4 glTranslatef")
    out.append("constexpr uint32_t kRefHead[] = {" + ", ".join(hd) + "};")
    flat = lambda rows: [v for row in rows for v in row]

    def arr(name, vals, per=12):
        s = [f"constexpr uint32_t {name}[] = {{"]
        for i in range(0, len(vals), per):
            s.append("    " + ", ".join(str(v) for v in vals[i:i + per]) + ",")
        s.append("};")
        return s

    out += ["// the vertex and normal arrays (3 floats per element) the program's array elements index"]
    out += arr("kRefVertices", flat(r["verts"]), 9)
    out += arr("kRefNormals", flat(r["norms"]), 9)
    out += ["// the program: 1 bind(name), 2 begin(GL_TRIANGLES), 3 end, 4 texcoord2fv(s, t), 5 array element(i), 6 normal3fv(x, y, z),",
            "// 7 texcoord2f(s, t), 8 vertex3fv(x, y, z)"]
    out += arr("kRefProgram", r["prog"], 16)
    return "\n".join(out) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--label", default="c7")
    ap.add_argument("--step", default="z2_r1")
    ap.add_argument("--rect", default="616,528,32,32")
    ap.add_argument("--out", default=str(ROOT / "port" / "src" / "sim" / "gl_wgl_ref.inc"))
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    r = build(a.label, a.step, tuple(int(v) for v in a.rect.split(",")))
    text = emit(r)
    print(f"{a.label}/{a.step} rect {r['rect']}: {r['kept']} of {r['of']} triangles, {len(r['verts'])} array vertices, "
          f"{len(r['prog'])} program words, {r['changed']} px written, sha256 {r['sha']}")
    out = pathlib.Path(a.out)
    if a.check:
        old = out.read_text() if out.exists() else ""
        if old != text:
            sys.exit(f"{out} differs from the generator's output")
        print("check ok")
        return
    out.write_text(text, newline="\n")
    print("wrote", out)


if __name__ == "__main__":
    main()
