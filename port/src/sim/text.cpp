// Text leaves (see portsim/sim/text.h). Phase 3.0, written from the specification; no original address, code or text.
#include "portsim/sim/text.h"

#include "portsim/ptr.h"

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   appendClubName    1 = clubs 11 and 12 swapped, 2 = strcpy instead of strcat
//   sanitizeFileName  1 = keeps one trailing space, 2 = an empty result returns 1
int g_negctl_append_club_name = 0;
int g_negctl_sanitize_file_name = 0;

// byte offsets of the club names in f_strpool_4c4f98, club 0..13 (the switch's case strings)
constexpr uint32_t kClubName[14] = {0x288, 0x280, 0x278, 0x270, 0x268, 0x260, 0x258,
                                    0x250, 0x248, 0x240, 0x238, 0x22C, 0x220, 0x218};
}  // namespace

void append_club_name(Ctx &ctx, int32_t club) {
  if (u32(club) > 13u) return;
  uint32_t k = u32(club);
  if (SG_NEGCTL(g_negctl_append_club_name) == 1 && (k == 11 || k == 12)) k = 23 - k;
  const uint32_t src = state_va(ctx, &GameState::f_strpool_4c4f98) + kClubName[k];
  const uint32_t dst = state_va(ctx, &GameState::g_textBuffer);
  if (SG_NEGCTL(g_negctl_append_club_name) == 2)
    vstrcpy(ctx.mem, dst, src);
  else
    vstrcat(ctx.mem, dst, src);
}

int32_t sanitize_file_name(Ctx &ctx, uint32_t s) {
  if (vstrpbrk(ctx.mem, s, state_va(ctx, &GameState::f_strpool_4c3ee0)) != 0) return 0;
  const int mode = SG_NEGCTL(g_negctl_sanitize_file_name);
  int32_t len;
  for (;;) {
    len = i32(vstrlen(ctx.mem, s));
    if (len == 0) break;
    if (ctx.mem.load<uint8_t>(s + u32(len) - 1) != ' ') break;
    if (mode == 1 && (len < 2 || ctx.mem.load<uint8_t>(s + u32(len) - 2) != ' ')) break;
    ctx.mem.store<uint8_t>(s + u32(len) - 1, 0);
    if (!(len > 0)) break;
  }
  if (mode == 2 && len == 0) return 1;
  return len > 0 ? 1 : 0;
}

// ------------------------------------------------------------------------------------------------ adapters
namespace {
void a_append_club_name(Ctx &c, const uint32_t *a, Ret &) { append_club_name(c, static_cast<int32_t>(a[0])); }
void a_sanitize_file_name(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(sanitize_file_name(c, a[0])); }

const FuncDef kFuncs[] = {
    {"appendClubName", "default", "void", 1, 0, a_append_club_name, &g_negctl_append_club_name},
    {"sanitizeFileName", "default", "bool", 1, 0, a_sanitize_file_name, &g_negctl_sanitize_file_name},
};
}  // namespace

extern const ModuleDef kModule_text = {"text", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
