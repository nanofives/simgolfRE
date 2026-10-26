"""S2.10 debug: run a boot scenario to record k, then find the single dword of one sim object that turns the port's hash into
the capture's (sgh64 is a sum of per-dword terms, so one changed dword can be solved for directly).

    cd <mirror root>; SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 <this> <scenario> <record> <object>
"""
import os
import sys

sys.path.insert(0, os.getcwd())
sys.path.insert(0, os.path.join(os.getcwd(), "re", "tools"))
import numpy as np  # noqa: E402

from port.test import boot as B  # noqa: E402
from port.test import lib as L  # noqa: E402
from port.test import segments as SG  # noqa: E402
from port.test import segment_files  # noqa: E402
import state_image as SI  # noqa: E402

M64 = (1 << 64) - 1


def unmix(h):
    # inverse of _mix (splitmix64's finaliser)
    def un_xorshift(v, s):
        r = v
        for _ in range(64 // s + 1):
            r = v ^ (r >> s)
        return r & M64
    h = un_xorshift(h, 31)
    h = h * pow(0x94D049BB133111EB, -1, 1 << 64) & M64
    h = un_xorshift(h, 27)
    h = h * pow(0xBF58476D1CE4E5B9, -1, 1 << 64) & M64
    h = un_xorshift(h, 30)
    return h


def main():
    scen, k, obj = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    lib = L.load()
    symbols, exp = B.expected_records(scen)
    names = [n for n, _, _ in lib.objects("sim")]
    j = names.index(obj)
    offs = {n: (o, s) for n, o, s in lib.objects()}
    tr = B.TRACES / f"{scen}.trace"
    assert lib.exe_load() == ""
    with lib.state() as st:
        st.set_data_base(SG.DATA_VA)
        st.fill_from_exe(True)
        if SG.INSTALL.is_dir():
            st.files_root(SG.INSTALL)
        for fname, data in segment_files.files_for(scen).items():
            st.files_put(fname, data)
        st.call("gditext.attach", [])
        code, done, ts, hs = st.run_boot(tr.read_bytes(), k + 1)
        img = st.image()
        st.call("gditext.detach", [])
    off, size = offs[obj]
    data = img[off:off + size]
    h = SI.sgh64(data)
    print(f"run code {code} done {done}; image hash {h:#x} port record {hs[k][j]:#x} capture {exp[k][1][j]:#x}")
    if h != hs[k][j]:
        print("the image is not the record's state: no solve")
        return
    s_port = unmix(h) ^ size
    s_cap = unmix(exp[k][1][j]) ^ size
    delta = (s_cap - s_port) & M64
    w = np.frombuffer(bytes(data) + b"\0" * ((-size) % 4), "<u4")
    for i in range(len(w)):
        mul = ((2 * i + 1) * SI.K_MUL) & M64
        x = delta * pow(mul, -1, 1 << 64) & M64
        new = ((int(w[i]) ^ SI.K_XOR) + x) & M64
        if new < (1 << 32):
            nw = new ^ SI.K_XOR
            if nw < (1 << 32):
                print(f"dword {i} (byte {4 * i:#x}): port {int(w[i]):#010x} capture {nw:#010x}")


if __name__ == "__main__":
    main()
