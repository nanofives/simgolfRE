"""S6.5: images where generateRandomCourse (location 1) hits NoReturn with the switch at 1; what does 0 give?"""
import sys, itertools
sys.path.insert(0, "C:/Users/maria/Desktop/Proyectos/SimGolf")
from port.test import lib as L
from port.test.test_terrain_gen import _fixed_image, port_call
lib = L.load()
for loc in (1, 3):
  for setting, ctype, diff, relief in itertools.product([0, 1, 2], [0, 1, 2, 3], [0, 3, 7], [0, 1]):
    img = _fixed_image(lib, n=3, loc=loc, acres=40, relief=relief, setting=setting, ctype=ctype, difficulty=diff)
    lib.defect_reset()
    r1, _ = port_call(lib, img, 3)
    if r1 != ("error", -10): continue
    lib.defect("generateRandomCourse.searchBound", False)
    r0, o = port_call(lib, img, 3)
    lib.defect_reset()
    print(loc, setting, ctype, diff, relief, "on:", r1, "off:", r0 if isinstance(r0, tuple) else "ok", flush=True)
