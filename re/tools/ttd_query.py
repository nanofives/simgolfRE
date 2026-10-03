"""Run debugger commands against a TTD trace (log/ttd/...) with cdb, no GUI.

Usage:
  py -3.12 re/tools/ttd_query.py <trace.run> --index                 # build the .idx once (queries get fast)
  py -3.12 re/tools/ttd_query.py <trace.run> "<cmd>" ["<cmd>" ...]
Examples (no symbols for the game: use addresses):
  'dx @$cursession.TTD.Memory(0x40afa0,0x40afa1,"e").Count()'              # executions of an address
  'dx -g @$cursession.TTD.Memory(0x543d10,0x55d738,"r").Select(x => x.IP).Distinct()'   # readers of a range
  'dx @$cursession.TTD.Memory(0x40afa0,0x40afa1,"e")[0].TimeStart.SeekTo(); dd esp L2'  # args at a call
Caveat: data the kernel writes (ReadFile into a buffer) is not a recorded user-mode write; TTD shows such
memory only from the first recorded read, so `dd` at an earlier position may print 0.
"""
import pathlib, re, subprocess, sys

WINDBG = pathlib.Path(r"C:\Program Files\WindowsApps")


def cdb():
    hits = sorted(WINDBG.glob("Microsoft.WinDbg_*_x64__8wekyb3d8bbwe/amd64/cdb.exe"))
    if not hits:
        sys.exit("cdb.exe not found: install WinDbg (winget install Microsoft.WinDbg)")
    return str(hits[-1])


def main():
    if len(sys.argv) < 3:
        print(__doc__); return 2
    trace, cmds = sys.argv[1], sys.argv[2:]
    if cmds == ["--index"]:
        cmds = ["!index"]
    r = subprocess.run([cdb(), "-z", trace, "-c", "; ".join(cmds + ["q"])], capture_output=True, text=True,
                       errors="replace")
    out = r.stdout.split("Reading initial command", 1)[-1]
    out = "\n".join(l for l in out.splitlines()[1:]
                    if not re.search(r"script unloaded|^quit:|^NatVis|^JavaScript", l))
    print(out.strip())
    return r.returncode


if __name__ == "__main__":
    sys.exit(main())
