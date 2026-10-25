"""S3.1 private build: the S3 mirror's port/ (port/build/S3_head = git archive of c6d7d78b) plus this step's own files plus exactly
the C ABI edit of log/roadmap/S3/1_merge.md section 1 (portsim_c.cpp routes every render entry kind through
portsim::terrain_render_entry). The S3 mirror itself is left untouched except for this step's own files, copied in.

    ~/bin/py -3.12 log/roadmap/S3/1_mirror.py [--w32] [--no-build]

Source copy: port/build/S3_1_src/port (re/ beside it is a junction to the mirror's re/, for the model header). Builds:
port/build/S3_head/port/build/S3_1 (x64) and S3_1_w32 (Win32). Prints SG_PORTSIM_DLL for the tests, which run in the mirror:
    cd port/build/S3_head && SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 -m pytest port/test/test_terrain_render.py
"""
import argparse
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
MIRROR = ROOT / "port" / "build" / "S3_head"
SRC = ROOT / "port" / "build" / "S3_1_src"
BUILD = {"x64": MIRROR / "port" / "build" / "S3_1", "Win32": MIRROR / "port" / "build" / "S3_1_w32"}
OWN = ["port/src/sim/terrain_render.cpp", "port/include/portsim/sim/terrain_render.h", "port/test/test_terrain_render.py",
       "port/test/terraincorpus.py", "port/test/glcorpus.py", "re/tools/gl_record.py", "re/frida/js/gl_rec.js"]
CMAKE = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

OLD_RENDER = """  portsim::gl::GlStreamSink sink;
  try {
    switch (entry) {
      case 2: {
        float angle;
        std::memcpy(&angle, &a1, 4);
        portsim::terrain_render(ctx->ctx, sink, ctx->gl_globals, terrain, a0, angle);
        break;
      }
      default: return SG_E_NO_FUNCTION;
    }
  }"""
NEW_RENDER = """  portsim::gl::GlStreamSink sink;
  try {
    if (!portsim::terrain_render_entry(ctx->ctx, sink, ctx->gl_globals, entry, terrain, a0, a1, a2)) return SG_E_NO_FUNCTION;
  }"""
OLD_RASTER = """  try {
    switch (entry) {
      case 2: {
        float angle;
        std::memcpy(&angle, &a1, 4);
        // the vertex and normal arrays the faces index are the renderer's globals; the rasteriser reads them for glArrayElement
        const portsim::TerrainGlobals &g = ctx->gl_globals;
        rast->r.set_arrays(g.vertices.data(), g.vertices.size() / 3, g.normals.data(), g.normals.size() / 3);
        portsim::terrain_render(ctx->ctx, rast->r, ctx->gl_globals, terrain, a0, angle);
        break;
      }
      default: return SG_E_NO_FUNCTION;
    }
  }"""
NEW_RASTER = """  try {
    // the vertex and normal arrays the faces index are the renderer's globals; the rasteriser reads them for glArrayElement
    const portsim::TerrainGlobals &g = ctx->gl_globals;
    rast->r.set_arrays(g.vertices.data(), g.vertices.size() / 3, g.normals.data(), g.normals.size() / 3);
    if (!portsim::terrain_render_entry(ctx->ctx, rast->r, ctx->gl_globals, entry, terrain, a0, a1, a2)) return SG_E_NO_FUNCTION;
  }"""
OLD_DOC = """ * and write the GL calls it makes as text (portsim/sim/gl_sink.h): entry 2 = Terrain::render(a0 = tile address,
 * a1 = angle bits). out gets the text"""
NEW_DOC = """ * and write the GL calls it makes as text (portsim/sim/gl_sink.h): entry 1 = stripRender(a0 = pivot, a1 = direction,
 * a2 = angle bits), 2 = Terrain::render(a0 = tile address, a1 = angle bits), 3 = localRender(a0 = tile, a1 = pivot,
 * a2 = angle bits), 4 = pathUpdateRender(a0 = pivot, a1 = angle bits); SG_E_NO_FUNCTION otherwise. out gets the text"""


def patch(text: str, old: str, new: str, what: str) -> str:
    old, new = old.replace("\n", "\r\n") if "\r\n" in text else old, new.replace("\n", "\r\n") if "\r\n" in text else new
    if text.count(old) != 1:
        raise SystemExit(f"{what}: anchor found {text.count(old)} times")
    return text.replace(old, new)


def copy_tree():
    dst = SRC / "port"
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(MIRROR / "port", dst, ignore=shutil.ignore_patterns("build", "corpus", "__pycache__"))
    if not (SRC / "re").exists():
        subprocess.run(["cmd", "/c", "mklink", "/J", str(SRC / "re"), str(MIRROR / "re")], check=True, capture_output=True)
    for rel in OWN:                       # this step's files, into the source copy and into the mirror (allowed: own files)
        src = ROOT / rel
        if rel.startswith("port/"):
            shutil.copyfile(src, SRC / rel)
        shutil.copyfile(src, MIRROR / rel)
    c = dst / "src" / "portsim_c.cpp"
    t = c.read_bytes().decode()
    t = patch(t, OLD_RENDER, NEW_RENDER, "sg_gl_render")
    t = patch(t, OLD_RASTER, NEW_RASTER, "sg_gl_render_to_raster")
    c.write_bytes(t.encode())
    h = dst / "include" / "portsim_c.h"
    h.write_bytes(patch(h.read_bytes().decode(), OLD_DOC, NEW_DOC, "portsim_c.h").encode())


def build(arch: str):
    b = BUILD[arch]
    subprocess.run([CMAKE, "-S", str(SRC / "port"), "-B", str(b), "-A", arch], check=True)
    subprocess.run([CMAKE, "--build", str(b), "--config", "Release", "--target", "portsim_c", "--", "/m"], check=True)
    if arch == "Win32":
        subprocess.run([CMAKE, "--build", str(b), "--config", "Release", "--target", "portsim_runner32", "--", "/m"], check=True)
    return b / "Release" / "portsim_c.dll"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--w32", action="store_true")
    ap.add_argument("--no-build", action="store_true")
    a = ap.parse_args()
    copy_tree()
    if a.no_build:
        return 0
    print("SG_PORTSIM_DLL=" + str(build("x64")))
    if a.w32:
        print("Win32:", build("Win32"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
