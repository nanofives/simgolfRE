"""I6: gfx.check() on the mirror's DLL (count of body names registered twice) and the body list."""
import pathlib, sys
sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import lib as L
for be in sys.argv[1:] or ["x64"]:
    lib = L.load(backend=be) if be != "x64" else L.load()
    with lib.state() as st:
        st.call("gfx.attach", [])
        n = st.call("gfx.check", []).eax
        b = st.files_read("$gfx/bodies.txt").decode()
        print(be, "duplicates", n, "bodies", len(b.splitlines()))
        print("\n".join(l for l in b.splitlines() if "Surface_drawSprite" in l or "Widget_fillRect" in l))
