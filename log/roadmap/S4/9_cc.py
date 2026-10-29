"""compile check of a TU against the port headers: 9_cc.py <file.cpp> [errmap] -> compiler output on stdout.
Given `errmap`, prints the errors and warnings per `// ---- <name>` block instead (cl stops at 100 errors: fix and re-run)."""
import collections
import pathlib
import re
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[3]
VCVARS = pathlib.Path(r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat")
inc = [ROOT / "port" / "include", ROOT / "re" / "model" / "include"]
tu = pathlib.Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory() as d:
    bat = pathlib.Path(d) / "cc.bat"
    bat.write_text(f'@call "{VCVARS}" >nul\r\ncl /nologo /c /std:c++17 /W4 /permissive- /EHsc /utf-8 '
                   + " ".join(f'/I"{p}"' for p in inc) + f' /Fo"{d}\\tu.obj" "{tu}"\r\n')
    r = subprocess.run(["cmd", "/c", str(bat)], capture_output=True)
txt = (r.stdout + r.stderr).decode("cp1252", "replace")
if len(sys.argv) > 2:
    src = tu.read_text(encoding="utf-8").split("\n")
    blocks = [(i, m.group(1)) for i, ln in enumerate(src, 1) for m in [re.match(r"// ---- (\S+)", ln)] if m]

    def blk(n):
        r_ = None
        for i, nm in blocks:
            if i <= n:
                r_ = nm
        return r_
    pat = re.compile(re.escape(tu.name) + r"\((\d+)\): (error|warning) (C\d+): ([^\n]*)")
    c = collections.defaultdict(list)
    for m in pat.finditer(txt):
        c[(blk(int(m.group(1))), m.group(2))].append((int(m.group(1)), m.group(3), m.group(4)[:110]))
    for (b, k), v in sorted(c.items(), key=lambda x: (x[1][0][0], x[0][1])):
        print(f"{b} {k} {len(v)}: line {v[0][0]} {v[0][1]} {v[0][2]}")
    print(f"compile rc={r.returncode}")
else:
    print(txt)
