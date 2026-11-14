"""S11.5: endstate.py's dead checks refuse the PE entry points, TLS callbacks and functions containing an export (U-2020).

Target: SG_ENDSTATE_PATH, else re/tools/endstate.py when its source holds the new check, else the patched copy
log/roadmap/S11/5_endstate.py (before the merge; log/ is gitignored). Every control is built from the target's source with
one line disabled and must read RED (the assert it breaks is checked to fail).
"""
import csv, importlib.util, os, pathlib, sys, types

import pytest

pytestmark = pytest.mark.static

ROOT = pathlib.Path(__file__).resolve().parents[1]
TOOL = ROOT / "re" / "tools" / "endstate.py"
PATCHED = ROOT / "log" / "roadmap" / "S11" / "5_endstate.py"
ENTRY = {"golf_clean.exe": "0x004a682f", "jgld.dll": "0x00082770", "sound.dll": "0x000430fe", "Terrain.dll": "0x00019b80"}
NAME = "not the PE entry point or a TLS callback"
LINE_CHECK = '("not the PE entry point or a TLS callback", not ldr,'
LINE_EXPORT = "exp += sorted(e for e in exps if _container(m, e) == va and e not in exp)"
LINE_TLS = 'out[v] = f"TLS callback {n}"'


def target():
    if os.environ.get("SG_ENDSTATE_PATH"):
        return pathlib.Path(os.environ["SG_ENDSTATE_PATH"]).resolve()
    if TOOL.exists() and NAME in TOOL.read_text(encoding="utf-8"):
        return TOOL
    return PATCHED


def load(path, tag, src=None):
    """Import `path` (or `src` text standing in for it, with __file__ resolved as `path`)."""
    sys.path.insert(0, str(ROOT / "re" / "tools"))
    if src is None:
        spec = importlib.util.spec_from_file_location(f"endstate_{tag}", path)
        m = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(m)
        return m
    m = types.ModuleType(f"endstate_{tag}")
    m.__file__ = str(path)
    exec(compile(src, str(path), "exec"), m.__dict__)
    return m


def variant(old, tag, new="pass"):
    path = target()
    src = path.read_text(encoding="utf-8")
    assert old in src, old
    if new == "pass":
        # keep the indentation of the removed statement
        out = []
        for line in src.splitlines():
            out.append(line.replace(old, "pass") if old in line else line)
        src2 = "\n".join(out)
    else:
        src2 = src.replace(old, new)
    assert src2 != src
    return load(path, tag, src2)


def entry_results(es):
    out = {}
    for mod, a in ENTRY.items():
        ok, checks = es.run(mod, a, "dead")
        out[mod] = (ok, [n for n, good, _ in checks if not good])
    return out


def entries_refused(es):
    return all(not ok and NAME in fails for ok, fails in entry_results(es).values())


def end_state_rows():
    rows = []
    for r in csv.DictReader(open(ROOT / "hooks.csv", newline="")):
        if r["status"] in ("dead", "trivial"):
            rows.append((r.get("module") or "golf_clean.exe", f"{int(r['addr'], 16):08x}", r["status"]))
    return rows


def failing_end_states(es):
    return [(m, a, st) for m, a, st in end_state_rows() if not es.run(m, a, st)[0]]


# ---- real modules --------------------------------------------------------------------------------------------------

def test_entry_points_refused():
    assert entries_refused(load(target(), "t"))


def test_recorded_end_states_still_pass():
    rows = end_state_rows()
    assert rows, "hooks.csv holds no dead/trivial rows"
    bad = failing_end_states(load(target(), "t2"))
    assert not bad, bad[:10]          # a failing row is a finding: report it, do not widen the check


def test_control_check_disabled_reads_red():
    assert not entries_refused(variant(LINE_CHECK, "off", '("not the PE entry point or a TLS callback", True,'))


def test_control_unpatched_reads_red():
    if target() == TOOL:
        pytest.skip("re/tools/endstate.py is already patched; the disabled-check control above covers it")
    assert not entries_refused(load(TOOL, "orig"))


# ---- stubbed module: TLS callbacks and export containment --------------------------------------------------------

BASE = 0x10000000


class FakeMod:
    name = "fake.dll"
    base = BASE
    thunks = {}
    text = [(BASE + 0x1000, BASE + 0x2000)]
    img = bytes(0x3000)
    # two functions: A 0x1000-0x1040 (an export sits inside it at 0x1010), B 0x1100-0x1140
    ranges = [(BASE + 0x1000, BASE + 0x1040, BASE + 0x1000), (BASE + 0x1100, BASE + 0x1140, BASE + 0x1100)]
    _starts = [r[0] for r in ranges]
    tls = {0x2000: BASE + 0x1100, 0x2004: BASE + 0x1000 + 0x20, 0x2008: 0}      # callbacks array at VA BASE+0x2000

    def function_at(self, va):
        for lo, hi, e in self.ranges:
            if lo <= va < hi:
                return e

    def dword(self, va):
        return self.tls.get(va - BASE, 0)


class FakePE:
    class OPTIONAL_HEADER:
        AddressOfEntryPoint = 0x1100

    class DIRECTORY_ENTRY_TLS:
        class struct:
            AddressOfCallBacks = BASE + 0x2000

    def __init__(self, *a, **k):
        pass

    def parse_data_directories(self, **k):
        pass


def stub(es, monkeypatch, tls=True):
    import pefile
    monkeypatch.setattr(pefile, "PE", FakePE)
    pe = FakePE
    if not tls:
        pe = type("NoTls", (FakePE,), {"DIRECTORY_ENTRY_TLS": property(lambda s: (_ for _ in ()).throw(AttributeError()))})
    monkeypatch.setattr(pefile, "PE", pe)
    es._LDR.clear()
    es._EXP.clear()
    return FakeMod()


def test_tls_callbacks_exact(monkeypatch):
    es = load(target(), "u1b")
    m = stub(es, monkeypatch)
    # entry point and callback 0 (0x1100) coincide: the dict keeps one label per address; callback 1 is 0x1020
    assert set(es._loader_entries(m)) == {BASE + 0x1100, BASE + 0x1020}


def test_tls_control_loop_disabled_reads_red(monkeypatch):
    es = variant(LINE_TLS, "u1c")
    m = stub(es, monkeypatch)
    assert set(es._loader_entries(m)) != {BASE + 0x1100, BASE + 0x1020}


def dead_for(es, monkeypatch, m, va):
    monkeypatch.setattr(es.pc, "mod", lambda name: m)
    monkeypatch.setattr(es.pc, "norm", lambda mm, a: a)
    monkeypatch.setattr(es.pc, "references", lambda mm, v: [])
    monkeypatch.setattr(es, "_rel_targets", lambda mm: {})
    return {n: ok for n, ok, _ in es.dead_checks("fake.dll", va)}


def test_callback_in_function_refuses_dead(monkeypatch):
    es = load(target(), "u2")
    m = stub(es, monkeypatch)
    monkeypatch.setattr(es, "_exports", lambda mm: set())
    # function A contains TLS callback 1 (0x1020, not its entry): refused
    assert dead_for(es, monkeypatch, m, BASE + 0x1000)[NAME] is False


def test_export_inside_function_refuses_dead(monkeypatch):
    es = load(target(), "u3")
    m = stub(es, monkeypatch, tls=False)
    monkeypatch.setattr(es, "_exports", lambda mm: {BASE + 0x1010})       # inside A, not at its entry
    monkeypatch.setattr(es, "_loader_entries", lambda mm: {})
    res = dead_for(es, monkeypatch, m, BASE + 0x1000)
    assert res["not exported"] is False
    assert dead_for(es, monkeypatch, m, BASE + 0x1100)["not exported"] is True       # B holds none


def test_export_control_line_disabled_reads_red(monkeypatch):
    es = variant(LINE_EXPORT, "u3c")
    m = stub(es, monkeypatch, tls=False)
    monkeypatch.setattr(es, "_exports", lambda mm: {BASE + 0x1010})
    monkeypatch.setattr(es, "_loader_entries", lambda mm: {})
    assert dead_for(es, monkeypatch, m, BASE + 0x1000)["not exported"] is True       # the guard is gone: RED for the asserts above


def test_tls_control_container_check_disabled_reads_red(monkeypatch):
    # the check line disabled: the callback inside A no longer refuses dead
    es = variant(LINE_CHECK, "u2c", '("not the PE entry point or a TLS callback", True,')
    m = stub(es, monkeypatch)
    monkeypatch.setattr(es, "_exports", lambda mm: set())
    assert dead_for(es, monkeypatch, m, BASE + 0x1000)[NAME] is True
