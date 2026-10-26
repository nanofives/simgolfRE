"""S5.1 gate (c): the five words Font::create stores, for all 15 FontRefs, from the FreeType provider and the GDI provider
(and, where a segment start image carries them, the words the original stored), plus the face each FontRef resolves to.

    SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 log/roadmap/S5/1_metrics_check.py [--win32 <build dir>] [--negctl N]
"""
import argparse
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from port.test import lib as L  # noqa: E402
from port.test import widths_check as WC  # noqa: E402

FONTREFS = ["g_font83ad20", "g_font51b320", "g_fontRef_519fd8", "g_font51b360", "g_font519928", "g_font51a028",
            "g_fontRef_519a40", "g_font519948", "g_font51b340", "g_font519968", "g_fontRef_821020", "g_fontRef_821f08",
            "g_fontRef_821ec8", "g_fontRef_821f28", "g_fontRef_821ee8"]


def words(st, attach, ref_va, clear):
    st.call(attach, [])
    out = []
    for i in range(5):
        try:
            out.append(WC.i32(st.call(attach.split(".")[0] + ".fontMetrics", [ref_va, i]).eax))
        except L.SgError as e:
            out.append(f"ERR {e}")
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--win32")
    ap.add_argument("--negctl", type=int, default=0)
    ap.add_argument("--scenario", default="cov_a_play")
    a = ap.parse_args()
    lib = WC.load_win32(pathlib.Path(a.win32)) if a.win32 else L.load()
    offs = {n: o for n, o, _ in lib.objects()}
    img = WC.start_image(a.scenario)
    ok = 0
    for name in FONTREFS:
        st = lib.state()
        st.set_data_base(WC.DATA_VA)
        st.load_image(img)
        st.files_root(WC.INSTALL)
        va = WC.DATA_VA + offs[name]
        stored = list(struct.unpack("<5i", st.read(offs[name] + 0xc, 20)))
        # measure the provider's own font: clear the stored words so the safety check does not stop the call
        st.write(offs[name] + 0xc, bytes(20))
        gdi = words(st, "gditext.attach", va, True)
        st.call("gditext.detach", [])
        if a.negctl:
            lib.negctl("fttext.fontWidth", a.negctl)
        ft = words(st, "fttext.attach", va, True)
        buf = (L.C.c_uint8 * 256)()
        st.map(0x30000000, buf)
        try:
            bold = st.call("fttext.faceOf", [va, 0x30000000, 256]).eax
            face = bytes(buf).split(b"\0")[0].decode("latin-1")
        except L.SgError as e:
            bold, face = None, f"ERR {e}"
        st.call("fttext.detach", [])
        if a.negctl:
            lib.negctl("fttext.fontWidth", 0)
        same = ft == gdi
        ok += same
        print(f"{name:18} {'SAME' if same else 'DIFF'} freetype {ft} gdi {gdi} stored "
              f"{stored if any(stored) else '-'} face {face} bold+{bold}", flush=True)
        st.close()
    print(f"five words equal: {ok}/{len(FONTREFS)}")
    return 0 if ok == len(FONTREFS) else 1


if __name__ == "__main__":
    sys.exit(main())
