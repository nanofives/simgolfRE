"""38b: insert log/p38/browser_block.txt into port/src/sim/saveload.cpp before the adapters (one-shot helper)."""
import pathlib

root = pathlib.Path(__file__).resolve().parents[2]
p = root / "port" / "src" / "sim" / "saveload.cpp"
s = p.read_text()
block = (root / "log" / "p38" / "browser_block.txt").read_text()
marker = "// ------------------------------------------------------------------------------------------------ adapters"
assert marker in s and "load_game_browser_screen(Ctx &ctx, int32_t resume" not in s.split(marker)[0]
s = s.replace(marker, block + marker, 1)
s = s.replace("""void a_load_course(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(load_course(c, static_cast<int32_t>(a[0]))); }
""", """void a_load_course(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(load_course(c, static_cast<int32_t>(a[0]))); }
void a_browser(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_i32(load_game_browser_screen(c, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1])));
}
""", 1)
s = s.replace("""    {"loadCourse", "default", "int", 1, 0, a_load_course, &g_negctl_load_course},
""", """    {"loadCourse", "default", "int", 1, 0, a_load_course, &g_negctl_load_course},
    {"loadGameBrowserScreen", "default", "int", 2, 0, a_browser, &g_negctl_browser},
    {"saveload.setScript", "default", "void", 1, 0, a_set_script, nullptr},
    {"saveload.clearScript", "default", "void", 0, 0, a_clear_script, nullptr},
""", 1)
s = s.replace("#include <cstring>\n#include <string>", "#include <cstring>\n#include <map>\n#include <memory>\n#include <string>", 1)
p.write_text(s)
print("applied")
