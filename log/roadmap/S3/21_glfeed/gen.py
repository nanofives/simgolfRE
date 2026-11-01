"""S3.21: writes glfeed.c (glreplay.c's state replay + the pass in GL_FEEDBACK mode) and build.bat next to this file."""
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[3]
src = (ROOT / "re/tools/glexact/glreplay.c").read_text(encoding="latin-1")
head = src[:src.index("int main(int argc, char **argv) {")]
head = ("/* S3.21 (U-1392): glreplay.c with the pass replayed in GL_FEEDBACK mode (GL_3D_COLOR_TEXTURE): every polygon the real\n"
        "   software GL would rasterise, after clipping and culling, as window coordinates, colour and texture coordinates.\n"
        "   Usage: glfeed <root> <step> <out.f32>   (out: float32 count, then the raw feedback buffer)\n"
        "   GLFEED_NOCULL=1 disables GL_CULL_FACE before the pass; GLFEED_POINTS=1 draws every block as GL_POINTS (one point token per vertex, in order). Derived from re/tools/glexact/glreplay.c. */\n" + head)
main = (HERE / "main.c.txt").read_text(encoding="latin-1")
(HERE / "glfeed.c").write_text(head + main, encoding="latin-1")
(HERE / "build.bat").write_text(
    "@echo off\r\n"
    "call \"C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Auxiliary\\Build\\vcvars32.bat\" >nul\r\n"
    "set CL=\r\nset _CL_=\r\ncd /d %~dp0\r\n"
    "cl /nologo /O2 glfeed.c /Fe:glfeed.exe opengl32.lib gdi32.lib user32.lib\r\n", encoding="latin-1")
print("wrote", HERE / "glfeed.c")
