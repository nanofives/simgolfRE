# C3 batch c3c of golf_clean.exe: ui/audio/video leaves. Reimplementations in shim/src/re/c3c.cpp,
# fixtures in re/frida/js/fixtures.d/c3c.js. Keys are the hooks.csv names. Format: top of hooks_registry.py.
# __thiscall functions take `this` as the first (pointer) arg under abi="thiscall" (as Terrain_tileAt does).

# Varied argument tuples for the plain-global writers (>= 10, so the no-callee leaves meet the C3 vector minimum).
_QUADS = [(1, 2, 3, 4), (-1, -2, -3, -4), (0, 0, 0, 0), (0x7fffffff, -0x80000000, 1, -1),
          (100, 200, 300, 400), (-5, 17, -333, 9), (0x11223344, 0x55667788, -0x12345678, 0x0badf00d),
          (255, -256, 4096, -4096), (0x40000000, -0x40000000, 7, -7), (12345, -6789, 2, 0)]

HOOKS = {
    # Plain-global writers: no fixture; the destination globals are the state region so the run restores them.
    "MsgBox::setColorA": dict(module="golf_clean.exe", addr=0x00490CC0, abi="default", ret="void",
                              args=["int", "int", "int", "int"],
                              state=[(0x004E4504, 0, 4), (0x004E4514, 0, 4), (0x004E4524, 0, 4), (0x004E4534, 0, 4)],
                              vectors=_QUADS),
    "MsgBox::setColorB": dict(module="golf_clean.exe", addr=0x00490D20, abi="default", ret="void",
                              args=["int", "int", "int", "int"],
                              state=[(0x004E4544, 0, 4), (0x004E4550, 0, 4), (0x004E455C, 0, 4), (0x004E4568, 0, 4)],
                              vectors=_QUADS),
    # Conditional return + global writer. Objects keep +4 == 0 so the pointer global 0x0083b9b4 is never written
    # (only the three int globals), and a null object exercises the early `return 3`.
    "MsgBox::setButtonA": dict(module="golf_clean.exe", addr=0x00490C80, abi="default", ret="int",
                               args=["pointer", "int", "int", "int"], fixture="c3c_buttons",
                               state=[(0x0083B9B8, 0, 4), (0x0083B9BC, 0, 4), (0x0083B9C0, 0, 4)],
                               vectors=[(0, 7, 8, 9), (0, -1, -1, -1)] +  # two null-p vectors -> return 3
                                       [(f"$b{i % 4}", a, b, c) for i, (a, b, c) in enumerate(
                                           [(1, 2, 3), (-1, -2, -3), (0, 0, 0), (0x7fffffff, -0x80000000, 123),
                                            (100, -200, 300), (-7, 8, -9), (0x12345678, 0x7ffffffe, -0x7fffffff),
                                            (42, 0, -42)])]),

    # Thiscall field writer: 6 garbage-filled widget slots; state is the whole arena.
    "Widget_setQuad": dict(module="golf_clean.exe", addr=0x00476310, abi="thiscall", ret="void",
                           args=["pointer", "int", "int", "int", "int"], fixture="c3c_widgets_quad",
                           state=[("$obj", 0, 0x100 * 10)],
                           vectors=[(f"$w{i}",) + q for i, q in enumerate(_QUADS)]),

    # Thiscall readers (no state): return varies across the fixture objects.
    "Window::visible": dict(module="golf_clean.exe", addr=0x004801F0, abi="thiscall", ret="int",
                            args=["pointer"], fixture="c3c_windows", vectors=[(f"$v{i}",) for i in range(10)]),
    "Widget_value": dict(module="golf_clean.exe", addr=0x00477580, abi="thiscall", ret="int",
                         args=["pointer"], fixture="c3c_widgets_value", vectors=[(f"$w{i}",) for i in range(10)]),

    # Thiscall transforms: mutate one (x, y) pair each; the pair run is the state region.
    "View::toLocal": dict(module="golf_clean.exe", addr=0x0047B290, abi="thiscall", ret="void",
                          args=["pointer", "pointer", "pointer"], fixture="c3c_view",
                          state=[("$coords", 0, 10 * 8)],
                          vectors=[("$view", f"$px{i}", f"$py{i}") for i in range(10)]),
    "View::toGlobal": dict(module="golf_clean.exe", addr=0x0047B2D0, abi="thiscall", ret="void",
                           args=["pointer", "pointer", "pointer"], fixture="c3c_view",
                           state=[("$coords", 0, 10 * 8)],
                           vectors=[("$view", f"$px{i}", f"$py{i}") for i in range(10)]),

    # Field-initialising constructors: each runs on a garbage-filled slot and returns `this`; the arena is the
    # state region, so an omitted field keeps its garbage and reads RED.
    # Cursor and BinkPlayer ctors have no callees, so they carry the 10-vector leaf minimum.
    "Cursor::ctor": dict(module="golf_clean.exe", addr=0x00488490, abi="thiscall", ret="pointer",
                         args=["pointer"], fixture="c3c_cursor_objs", state=[("$obj", 0, 0x20 * 10)],
                         vectors=[(f"$o{i}",) for i in range(10)]),
    "BinkPlayer::ctor": dict(module="golf_clean.exe", addr=0x00487000, abi="thiscall", ret="pointer",
                             args=["pointer"], fixture="c3c_bink_objs", state=[("$obj", 0, 0x20 * 10)],
                             vectors=[(f"$o{i}",) for i in range(10)]),
    "Snd::ctorBase": dict(module="golf_clean.exe", addr=0x00484150, abi="thiscall", ret="pointer",
                          args=["pointer"], fixture="c3c_snd_base_objs", state=[("$obj", 0, 0x80 * 6)],
                          vectors=[(f"$o{i}",) for i in range(6)]),
    "Snd::ctorDerived": dict(module="golf_clean.exe", addr=0x00484820, abi="thiscall", ret="pointer",
                             args=["pointer"], fixture="c3c_snd_derived_objs", state=[("$obj", 0, 0x80 * 6)],
                             vectors=[(f"$o{i}",) for i in range(6)]),
}
