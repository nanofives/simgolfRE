"""helper for the S4.7 in-place edits: replace the code between a block's `// ---- name` line and its `int g_negctl_` line."""
import pathlib, re
P = pathlib.Path("port/src/sim/audio_sound.cpp")

def replace_body(name, new, keep_header=True):
    t = P.read_text(encoding="utf-8")
    m = re.search(r"^// ---- " + re.escape(name) + r" [^\n]*\n", t, re.M)
    assert m, name
    n = re.compile(r"^int g_negctl_\w+ = 0;", re.M).search(t, m.end())
    assert n, name
    hdr = m.group(0) if keep_header else ""
    t = t[:m.start()] + hdr + new.rstrip("\n") + "\n" + t[n.start():]
    P.write_text(t, encoding="utf-8")

def sub(old, new, count=1):
    t = P.read_text(encoding="utf-8")
    assert t.count(old) == count, (old[:80], t.count(old))
    P.write_text(t.replace(old, new), encoding="utf-8")
