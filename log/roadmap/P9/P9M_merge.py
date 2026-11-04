"""P9 merge: port/build/P9M = HEAD 2fd3553c + P9A + P9B by 3-way merge-file (base = the archive)."""
import pathlib, shutil, subprocess, sys
R = pathlib.Path.cwd(); B = R / "port/build"
A = ["port/app/CMakeLists.txt","port/app/stdio_log.h","port/app/stdio_log.cpp","port/app/main.cpp","port/app/present.h",
     "port/app/present.cpp","port/src/sim/tick.cpp","port/src/sim/tick_menu.inc","port/src/sim/commands.cpp",
     "port/src/sim/commands_internal.h","port/src/sim/commands_popup.inc","port/src/sim/saveload.cpp","port/src/sim/buyland.cpp",
     "port/src/sim/chareditor.cpp","port/src/sim/dialogs.cpp","re/port/play_profile.tsv","port/test/conftest.py"]
Bf = ["port/include/portsim/sim/gfx.h","port/src/sim/gfx_body.h","port/src/sim/gfx.cpp","port/src/sim/tick.cpp","port/src/sim/tick_frame.inc",
     "port/app/present.h","port/app/present.cpp","port/app/host.h","port/app/host.cpp","port/app/main.cpp","port/app/window.cpp"]
def rd(p): return p.read_bytes().replace(b"\r\n", b"\n") if p.exists() else None
bad = 0
for f in sorted(set(A) | set(Bf)):
    base, a, b = rd(B/"P9M"/f), rd(B/"P9A"/f), rd(B/"P9B"/f)
    if f in A and f in Bf:
        tmp = pathlib.Path(R/"log/roadmap/P9/_m"); tmp.mkdir(exist_ok=True)
        for n, d in (("base", base), ("a", a), ("b", b)): (tmp/n).write_bytes(d)
        r = subprocess.run(["git","merge-file","-p","-L","P9A","-L","base","-L","P9B",str(tmp/"a"),str(tmp/"base"),str(tmp/"b")],capture_output=True)
        out = r.stdout; print(f, "merge conflicts" if r.returncode else "merged", r.returncode); bad += r.returncode != 0
    else:
        out = a if f in A else b; print(f, "from", "P9A" if f in A else "P9B")
    (B/"P9M"/f).parent.mkdir(parents=True, exist_ok=True); (B/"P9M"/f).write_bytes(out)
sys.exit(1 if bad else 0)
