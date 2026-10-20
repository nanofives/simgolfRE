"""38a: produce log/p38/38a/segments_py.patch, the SegmentRunner hook that attaches the host GDI TextMetrics provider,
from the CURRENT port/test/segments.py (applied to a copy; the shared file is not edited). The run's body after the game
files are attached moves into _run_loaded so the provider is detached in a finally before the context is freed."""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
SRC = ROOT / "port" / "test" / "segments.py"
out_dir = pathlib.Path(sys.argv[1])
dst = out_dir / "a" / "port" / "test" / "segments.py"
new = out_dir / "b" / "port" / "test" / "segments.py"
dst.parent.mkdir(parents=True, exist_ok=True)
new.parent.mkdir(parents=True, exist_ok=True)
s = SRC.read_text(encoding="utf-8")
dst.write_text(s, encoding="utf-8", newline="\n")
lines = s.splitlines()


def find(prefix, start=0):
    for i in range(start, len(lines)):
        if lines[i].startswith(prefix):
            return i
    raise SystemExit(f"not found: {prefix!r}")


i_init = find("    def __init__(self, lib: L.Lib | None = None):")
assert lines[i_init + 1] == "        self.lib = lib or L.load()"
assert lines[i_init + 2] == '        self.sim = self.lib.objects("sim")'
i_run = find("    def run(self, seg: Segment) -> SegmentResult:")
i_check = find("            why = self.load_check(seg, st)", i_run)
i_end = find("    def _detail(self, st: L.State) -> str:", i_run)
body = lines[i_check:i_end]
while body and body[-1] == "":
    body.pop()
moved = [(l[4:] if l.startswith("    ") else l) for l in body]
out = lines[:i_init]
out += ["    def __init__(self, lib: L.Lib | None = None, text_metrics: str | None = None):",
        "        self.lib = lib or L.load()",
        '        self.sim = self.lib.objects("sim")',
        "        # phase 3.8 (38a): \"gdi\" attaches the host GDI TextMetrics provider (module gditext) to every run when the",
        "        # library has it; \"none\" (or SG_TEXT_METRICS=none) runs without one (U-0944, U-0947)",
        '        self.text_metrics = text_metrics or os.environ.get("SG_TEXT_METRICS", "gdi")']
out += lines[i_init + 3:i_check]
out += ["            # phase 3.8 (38a): the widgets' Text_put widths from the host GDI provider, detached before the context is",
        "            # freed (the provider maps are keyed by the context's address, which a later context can reuse)",
        '            gdi = self.text_metrics == "gdi" and self.lib.has("gditext.attach")',
        "            if gdi:",
        '                st.call("gditext.attach", [])',
        "            try:",
        "                return self._run_loaded(seg, st, res)",
        "            finally:",
        "                if gdi:",
        '                    st.call("gditext.detach", [])',
        "",
        "    def _run_loaded(self, seg: Segment, st: L.State, res: SegmentResult) -> SegmentResult:"]
out += moved + [""]
out += lines[i_end:]
t = "\n".join(out) + "\n"
if "\nimport os\n" not in t:
    t = t.replace("\nimport pathlib\n", "\nimport os\nimport pathlib\n", 1)
new.write_text(t, encoding="utf-8", newline="\n")
d = subprocess.run(["git", "diff", "--no-index", "--src-prefix=", "--dst-prefix=", "--", "a/port/test/segments.py", "b/port/test/segments.py"], cwd=out_dir,
                   capture_output=True, text=True).stdout
(ROOT / "log" / "p38" / "38a" / "segments_py.patch").write_text(d, encoding="utf-8", newline="\n")
print(d)
