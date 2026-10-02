"""Single source of truth for A/B test vectors. Add an entry here; never write a one-off harness.

Each entry drives re/frida/diff_hook.py:
  addr      VA (golf_clean.exe) the SG_HOOK is registered at
  abi       Frida ABI name: "thiscall" | "stdcall" | "default" (cdecl) | "fastcall"
  ret/args  Frida NativeFunction types
  fixture   name of a JS fixture in js/diff_fixtures.js; it returns {obj, ...} pointers that vectors
            reference as "$obj" (and pointer returns are reported relative to $obj, so arms compare)
  vectors   list of argument tuples
"""

_GRID = [(x, y) for x in (-1, 0, 1, 5, 9, 10, 11, 0x7FFFFFFF, -0x80000000) for y in (-1, 0, 3, 7, 8, 9)]

HOOKS = {
    "Terrain_tileAt": dict(
        module="golf_clean.exe",
        addr=0x004490D0,
        abi="thiscall",
        ret="pointer",
        args=["pointer", "int", "int"],
        fixture="terrain_10x8",
        vectors=[("$obj", x, y) for x, y in _GRID],
        note="re/analysis/terrain/004490d0_tileAt.md",
    ),
    # Live copy in Terrain.dll (RVA of the body; module-relative). Recorded/replayed by record.py/replay.py.
    "TerrainDll_tileAt": dict(
        module="Terrain.dll",
        addr=0x00001D50,
        abi="thiscall",
        ret="pointer",
        args=["pointer", "int", "int"],
        nargs=2,  # stack args (this is in ecx)
        fixture="terrain_10x8",
        vectors=[("$obj", x, y) for x, y in _GRID],
    ),
}
