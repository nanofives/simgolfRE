"""38b round 2: insert log/p38/save_block.txt (saveCourse, saveGameDialog, promptAndSaveFile) into saveload.cpp."""
import pathlib

root = pathlib.Path(__file__).resolve().parents[2]
p = root / "port" / "src" / "sim" / "saveload.cpp"
s = p.read_text()
block = (root / "log" / "p38" / "save_block.txt").read_text()
block = block.replace("    vstrcpy(ctx.mem, text, dirs + 0x2C);\n    // the name goes first: text = name + the overwrite question\n", "    // text = name + the overwrite question\n")
marker = "// ------------------------------------------------------------------------------------------------ the browser"
assert marker in s and "int32_t save_course(Ctx" not in s
s = s.replace(marker, block + marker, 1)
s = s.replace("int g_negctl_refresh_terrain = 0;", "int g_negctl_save_course = 0;   // 1 = no course body, 2 = a 4-byte marker\nint g_negctl_save_game_dialog = 0;   // 1 = a refused name goes on, 2 = an existing file is not asked about\nint g_negctl_prompt_and_save_file = 0;   // 1 = the skill record's first byte flipped\nint g_negctl_refresh_terrain = 0;", 1)
s = s.replace("void a_browser(Ctx &c, const uint32_t *a, Ret &r) {", """void a_save_course(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(save_course(c)); }
void a_save_game_dialog(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(save_game_dialog(c)); }
void a_prompt_and_save_file(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_i32(prompt_and_save_file(c, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2])));
}
void a_browser(Ctx &c, const uint32_t *a, Ret &r) {""", 1)
s = s.replace('    {"tierPrice", "default", "int", 1, 0, a_tier_price, &g_negctl_tier_price},', '    {"tierPrice", "default", "int", 1, 0, a_tier_price, &g_negctl_tier_price},\n    {"saveCourse", "default", "int", 0, 0, a_save_course, &g_negctl_save_course},\n    {"saveGameDialog", "default", "int", 0, 0, a_save_game_dialog, &g_negctl_save_game_dialog},\n    {"promptAndSaveFile", "default", "int", 3, 0, a_prompt_and_save_file, &g_negctl_prompt_and_save_file},', 1)
if '#include "portsim/sim/text.h"' not in s:
    s = s.replace('#include "portsim/sim/util.h"', '#include "portsim/sim/text.h"\n#include "portsim/sim/util.h"', 1)
p.write_text(s)
h = root / "port" / "include" / "portsim" / "sim" / "saveload.h"
t = h.read_text()
t = t.replace("}  // namespace portsim\n\n#endif", """// saveCourse (re/analysis/save/0040b680): saves the course named in g_textBuffer (two header lines, the course body, the marker).
int32_t save_course(Ctx &ctx);
// saveGameDialog (re/analysis/save/00405b10): the name prompt of the save key, then saveGame.
int32_t save_game_dialog(Ctx &ctx);
// promptAndSaveFile (re/analysis/save/00437910): writes golfer type `index` as a character file (.chr / .pro); 1 when written.
int32_t prompt_and_save_file(Ctx &ctx, int32_t index, int32_t mode, int32_t championship);

}  // namespace portsim

#endif""")
h.write_text(t)
print("ok")
