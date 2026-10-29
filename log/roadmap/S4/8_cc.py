"""S4.8 fast compile check: cl /c of one port module file against the port headers (x64, /W4, the port's flags).

    ~/bin/py -3.12 log/roadmap/S4/8_cc.py port/src/sim/audio_ds.cpp [--max 60]
"""
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import snd_hook2port as H  # noqa: E402


def main():
    src = pathlib.Path(sys.argv[1]).resolve()
    mx = int(sys.argv[sys.argv.index("--max") + 1]) if "--max" in sys.argv else 60
    inc = [ROOT / "port" / "include", ROOT / "re" / "model" / "include", ROOT / "port" / "src" / "sim"]
    with tempfile.TemporaryDirectory() as d:
        bat = pathlib.Path(d) / "cc.bat"
        bat.write_text(f'@call "{H.VCVARS}" >nul\r\ncl /nologo /c /std:c++17 /W4 /permissive- /EHsc /DSG_NEGCTL_ON=1 '
                       + " ".join(f'/I"{p}"' for p in inc) + f' /Fo"{d}\\tu.obj" "{src}"\r\n')
        r = subprocess.run(["cmd", "/c", str(bat)], capture_output=True, text=True)
    lines = [ln for ln in (r.stdout + r.stderr).splitlines() if "error" in ln or "warning" in ln]
    for ln in lines[:mx]:
        print(ln.replace(str(src), src.name))
    print(f"rc {r.returncode}  errors {sum('error' in ln for ln in lines)}  warnings {sum('warning' in ln for ln in lines)}")
    return r.returncode


if __name__ == "__main__":
    sys.exit(main())
