---
name: test-suite
description: Run and extend the SimGolf RE test suite (pytest) — anchors, SafeDisc removal, portability, shim, windowed boot, Frida instrumentation, input, in-game, patches, A/B tool, confidence gate, Ghidra. Use before and after any change to original/, shim/, re/frida/, scripts/, or when asked to "run the tests", "is everything still working", "check the setup".
---

# test-suite (SimGolf)

```
py -3.12 -m pytest -m static                 # ~5 s, no game launched: anchors, imports, shim surface, gate unit tests
py -3.12 -m pytest -m "runtime and not slow" # ~1 min: boots the game windowed, Frida, input, in-game, patches, A/B
py -3.12 -m pytest -m ghidra                 # ~15 s: master project, headless facts via a pool slot, MCP tools/list
py -3.12 -m pytest -m slow                   # ~1.5 min: 5x boot reliability + full intro with the Bink fix
py -3.12 -m pytest                           # everything
```

Artifacts (screenshots) land in `log/test_artifacts/`.

## Before running runtime tests

- No SimGolf may be running: the game is single-instance and the suite fails fast rather than kill
  someone else's process. Close it yourself or wait.
- If `test_shim_deployed_and_current` fails: `shim\build.bat`.

## Extending

- Screen checks use `re/tools/imgcmp.py` (160x120 grayscale mean-abs distance, threshold 12) against
  `tests/golden/*.png`. For randomized screens compare fixed regions only (see the in-game HUD test).
  Measure the distance between "same screen" and "other screens" before picking a region.
- Clicks go through `Game.click(x, y)` in 800x600 game space. It moves the shim's virtual cursor, so
  the user's real mouse is never touched.
- New runtime flows that change state use the `fresh_game` fixture; read-only checks share
  `menu_game` (module scope).
- A new patch in `patches/` is covered automatically by `test_committed_patch_matches_anchor`.
