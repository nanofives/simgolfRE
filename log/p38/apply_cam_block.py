"""38b: insert log/p38/cam_block.txt (tierPrice, courseActionMenu) into port/src/sim/saveload.cpp before the test entries."""
import pathlib

root = pathlib.Path(__file__).resolve().parents[2]
p = root / "port" / "src" / "sim" / "saveload.cpp"
s = p.read_text()
block = (root / "log" / "p38" / "cam_block.txt").read_text()
marker = "// ------------------------------------------------------------------------------------------------ test entries"
assert marker in s and "int32_t course_action_menu(Ctx" not in s
s = s.replace(marker, block + marker, 1)
s = s.replace("int g_negctl_refresh_terrain = 0;", "int g_negctl_tier_price = 0;          // 1 = a tier outside 0..15 costs 600, 2 = tier 15 costs 9000\nint g_negctl_course_action_menu = 0;  // 1 = the purchase takes the next site's record, 2 = unlock bit c + 1, 4 = spot offset 0x18\nint g_negctl_refresh_terrain = 0;", 1)
s = s.replace("void a_browser(Ctx &c, const uint32_t *a, Ret &r) {", "void a_tier_price(Ctx &, const uint32_t *a, Ret &r) { r.set_i32(tier_price(static_cast<int32_t>(a[0]))); }\nvoid a_course_action_menu(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(course_action_menu(c)); }\nvoid a_browser(Ctx &c, const uint32_t *a, Ret &r) {", 1)
s = s.replace('    {"loadGameBrowserScreen", "default", "int", 2, 0, a_browser, &g_negctl_browser},', '    {"loadGameBrowserScreen", "default", "int", 2, 0, a_browser, &g_negctl_browser},\n    {"tierPrice", "default", "int", 1, 0, a_tier_price, &g_negctl_tier_price},\n    {"courseActionMenu", "default", "int", 1, 0, a_course_action_menu, &g_negctl_course_action_menu},', 1)
p.write_text(s)
print("applied")
