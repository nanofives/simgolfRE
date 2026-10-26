"""S5.4: the SG_TEXT_METRICS=freetype selection as asserted replacements over the working tree's harness files.

    ~/bin/py -3.12 log/roadmap/S5/4_patch.py write     # writes log/roadmap/S5/4_harness/{segments,boot,integrator}.py
    from 4_patch import install; install()             # (used by 4_run.py) loads those copies as port.test.<name>

The shared files port/test/segments.py, boot.py, integrator.py and gfx.py are NOT edited. The same replacements are the merge
lines in 4_merge.md. gfx.py needs none: gfx.render and the checkpoint runners reach the provider through boot.run.
"""
import importlib.util
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
TEST = ROOT / "port" / "test"
OUT = pathlib.Path(__file__).resolve().parent / "4_harness"

EDITS = {
    "segments": [
        ('''# ------------------------------------------------------------------------------------------- runner
class SegmentRunner:''',
         '''# ------------------------------------------------------------------------------------------- runner
TEXT_PROVIDERS = {"gdi": "gditext", "freetype": "fttext"}


def text_provider(mode: str | None, lib: L.Lib) -> str | None:
    """Module of the TextMetrics provider SG_TEXT_METRICS / text_metrics `mode` selects ("gdi" = gditext, "freetype" =
    fttext), None for "none", an unknown mode, or a library without that module (S5.4; the default stays "gdi")."""
    mod = TEXT_PROVIDERS.get(mode or "")
    return mod if mod and lib.has(f"{mod}.attach") else None


class SegmentRunner:'''),
        ('''            gdi = self.text_metrics == "gdi" and self.lib.has("gditext.attach")
            if gdi:
                st.call("gditext.attach", [])''',
         '''            tp = text_provider(self.text_metrics, self.lib)
            if tp:
                st.call(f"{tp}.attach", [])'''),
        ('''                if gdi:
                    st.call("gditext.detach", [])

    def _run_loaded''',
         '''                if tp:
                    st.call(f"{tp}.detach", [])

    def _run_loaded'''),
    ],
    "boot": [
        ('''        gdi = lib.has("gditext.attach") and os.environ.get("SG_TEXT_METRICS", "gdi") == "gdi"
        if gdi:
            st.call("gditext.attach", [])''',
         '''        tp = SG.text_provider(os.environ.get("SG_TEXT_METRICS", "gdi"), lib)
        if tp:
            st.call(f"{tp}.attach", [])'''),
        ('''            if gdi:
                st.call("gditext.detach", [])
    res.records = done''',
         '''            if tp:
                st.call(f"{tp}.detach", [])
    res.records = done'''),
    ],
    "integrator": [
        ('''        gdi = getattr(runner, "text_metrics", "none") == "gdi" and lib.has("gditext.attach")
        if gdi:
            st.call("gditext.attach", [])''',
         '''        tp = S.text_provider(getattr(runner, "text_metrics", "none"), lib)
        if tp:
            st.call(f"{tp}.attach", [])'''),
        ('''            if gdi:
                st.call("gditext.detach", [])


def _run_loaded''',
         '''            if tp:
                st.call(f"{tp}.detach", [])


def _run_loaded'''),
    ],
}


ROOT_EXPR = "pathlib.Path(__file__).resolve().parents[2]"
BOOT_CMD = '[sys.executable, "-m", "port.test.boot", s, "--out", "-"]'


def patched(name):
    text = (TEST / f"{name}.py").read_text(encoding="utf-8")
    # the copies live outside port/test: keep ROOT on the real tree
    assert text.count(ROOT_EXPR) == 1, name
    text = text.replace(ROOT_EXPR, f"pathlib.Path({str(ROOT)!r})")
    if name == "boot":   # `boot --all --jobs N` re-runs itself per scenario: through 4_run.py, which loads these copies
        assert text.count(BOOT_CMD) == 1
        text = text.replace(BOOT_CMD, f'[sys.executable, {str(OUT.parent / "4_run.py")!r}, "boot", s, "--out", "-"]')
    for old, new in EDITS[name]:
        assert text.count(old) == 1, (name, old[:60], text.count(old))
        text = text.replace(old, new)
    return text


def write():
    OUT.mkdir(exist_ok=True)
    for n in EDITS:
        (OUT / f"{n}.py").write_text(patched(n), encoding="utf-8")
        print("wrote", OUT / f"{n}.py")


def install():
    """Replace port.test.segments / boot / integrator by the patched copies (names kept, so relative imports work)."""
    import port.test  # noqa: F401
    for n in ("segments", "boot", "integrator"):
        full = f"port.test.{n}"
        spec = importlib.util.spec_from_file_location(full, OUT / f"{n}.py")
        mod = importlib.util.module_from_spec(spec)
        sys.modules[full] = mod
        spec.loader.exec_module(mod)
        setattr(sys.modules["port.test"], n, mod)


if __name__ == "__main__":
    write()
