# S5.4 merge lines: the text provider selection (SG_TEXT_METRICS=freetype)

Default stays `gdi`: with no SG_TEXT_METRICS every run attaches gditext as before (proven: segment gate x64 with the edits, no
variable, 205 / 205 PASS and 0 digests differ from log/p5/segs_x64_loc.tsv; log/roadmap/S5/4_segs_gdi_x64.*). The edits are
applied and run in log/roadmap/S5/4_harness/ (4_patch.py writes them from the working tree with asserted replacements; the copies
also carry two mirror-only changes, ROOT as a literal and boot's per-scenario child through 4_run.py, which are NOT part of this merge).
Requires the S5.1 / S5.2 / S5.3 lines already asked for (1_merge.md, 2_merge.md, 3_merge.md: `SG_MODULE(fttext)`,
`SG_MODULE(textlab)`, `SG_MODULE(fttext_raster)`, the CMake block).

## 1. port/test/segments.py
Before `class SegmentRunner:` (it follows the `# ---- runner` banner) insert:

```python
TEXT_PROVIDERS = {"gdi": "gditext", "freetype": "fttext"}


def text_provider(mode: str | None, lib: L.Lib) -> str | None:
    """Module of the TextMetrics provider SG_TEXT_METRICS / text_metrics `mode` selects ("gdi" = gditext, "freetype" =
    fttext), None for "none", an unknown mode, or a library without that module (S5.4; the default stays "gdi")."""
    mod = TEXT_PROVIDERS.get(mode or "")
    return mod if mod and lib.has(f"{mod}.attach") else None
```

In `SegmentRunner.run` replace

```python
            gdi = self.text_metrics == "gdi" and self.lib.has("gditext.attach")
            if gdi:
                st.call("gditext.attach", [])
```
with
```python
            tp = text_provider(self.text_metrics, self.lib)
            if tp:
                st.call(f"{tp}.attach", [])
```
and in its `finally:` replace `if gdi:` / `st.call("gditext.detach", [])` with `if tp:` / `st.call(f"{tp}.detach", [])`.
Also change the comment above `self.text_metrics` to say "gdi" | "freetype" | "none".

## 2. port/test/boot.py (`run`)
Replace
```python
        gdi = lib.has("gditext.attach") and os.environ.get("SG_TEXT_METRICS", "gdi") == "gdi"
        if gdi:
            st.call("gditext.attach", [])
```
with
```python
        tp = SG.text_provider(os.environ.get("SG_TEXT_METRICS", "gdi"), lib)
        if tp:
            st.call(f"{tp}.attach", [])
```
and in the `finally:` `if gdi:` / `st.call("gditext.detach", [])` with `if tp:` / `st.call(f"{tp}.detach", [])`.

## 3. port/test/integrator.py (`run_from`)
Replace
```python
        gdi = getattr(runner, "text_metrics", "none") == "gdi" and lib.has("gditext.attach")
        if gdi:
            st.call("gditext.attach", [])
```
with
```python
        tp = S.text_provider(getattr(runner, "text_metrics", "none"), lib)
        if tp:
            st.call(f"{tp}.attach", [])
```
and in the `finally:` the same detach change. Update the comment above (`SG_TEXT_METRICS=none|gdi|freetype`).

## 4. port/test/gfx.py: no edit
`gfx.render` runs through `boot.run`, so item 2 selects the provider for it (`SG_TEXT_METRICS=freetype`). Measured that way
(log/roadmap/S5/4_gfx_ft_*.log). port/test/framegate.py and labels.py are unchanged (labels.py already has its own `PROVIDERS` map).

## 5. Nothing else
No C++ file, CMake line or modules.inc line beyond S5.1 .. S5.3's.
