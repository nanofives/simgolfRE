"""S4.10 gate (c) exploration: the exe's sound start-up in production mode (no host) over a segment start image, every
import answered by a sink; prints where the chain stops and the stream so far.

    ~/bin/py -3.12 log/roadmap/S4/10_device_probe.py [--sink record|device] [--steps init,table,play,tick,shutdown]
"""
import argparse
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from port.test import audio_stream as AS  # noqa: E402
from port.test import lib as L  # noqa: E402
from port.test import segments as S  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sink", default="device")
    ap.add_argument("--steps", default="init,table,play,tick,shutdown")
    ap.add_argument("--segment", default="sandbox_basic/00")
    ap.add_argument("--ticks", type=int, default=8)
    ap.add_argument("--play", type=int, default=None, help="sounds to play (default: --ticks)")
    ap.add_argument("--stream-out")
    a = ap.parse_args()
    lib = AS.load_lib()
    seg = S.load_segment(S.SEG_DIR / a.segment.split("/")[0] / a.segment.split("/")[1])
    with lib.state() as st:
        st.set_data_base(AS.DATA_VA)
        st.files_root(S.INSTALL)
        st.load_image(seg.start)
        r = st.call("audio_exe.deviceFixture", [1 if a.sink == "device" else 0])
        print("fixture:", r.eax)
        for step in a.steps.split(","):
            try:
                r = st.call("audio_exe.deviceStep", [{"init": 1, "table": 2, "play": 3, "tick": 4, "shutdown": 5}[step],
                                                     a.play if step == "play" and a.play is not None else a.ticks])
                print(f"step {step}: eax 0x{r.eax:x}")
            except L.SgError as e:
                print(f"step {step}: {e}")
                break
            finally:
                st.call("audio_exe.report", [])
                st.call("audio_core.report", [])
                print("  exe:", (st.files_read("$audio_exe/out") or b"").decode().replace("\n", " "))
                print("  core:", (st.files_read("$audio/out") or b"").decode().replace("\n", " ")[:600])
        st.call("audio_exe.streamLog", [])
        lines = (st.files_read("$audio_exe/stream") or b"").decode().splitlines()
        print(f"stream: {len(lines)} lines")
        if a.stream_out:
            pathlib.Path(a.stream_out).write_text("\n".join(lines) + "\n")
        for ln in lines[:200]:
            print("  " + ln)
        if len(lines) > 200:
            print("  ...")
            for ln in lines[-40:]:
                print("  " + ln)


if __name__ == "__main__":
    main()
