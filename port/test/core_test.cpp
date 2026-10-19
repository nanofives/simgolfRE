// Unit tests of the phase 3.0 core helpers (ctest portsim_core; also run by port/test/test_core.py):
// portsim/arith.h, portsim/ptr.h, portsim/gamefiles.h, portsim/execonst.h ($SG_EXE_PATH or argv[1]: the user's exe).
#define _CRT_SECURE_NO_WARNINGS  // std::getenv (MSVC /W4 /WX)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "portsim/arith.h"
#include "portsim/execonst.h"
#include "portsim/gamefiles.h"
#include "portsim/ptr.h"

using namespace portsim;

static int g_fail = 0;
#define CHECK(c)                                                    \
  do {                                                              \
    if (!(c)) {                                                     \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);      \
      ++g_fail;                                                     \
    }                                                               \
  } while (0)

template <class E, class F> static bool throws(F f) {
  try {
    f();
  } catch (const E &) {
    return true;
  } catch (...) {
    return false;
  }
  return false;
}

static void test_arith() {
  CHECK(add32(INT32_MAX, 1) == INT32_MIN);
  CHECK(sub32(INT32_MIN, 1) == INT32_MAX);
  CHECK(mul32(0x10000, 0x10000) == 0);
  CHECK(mul32(-3, 0x55555556) == -2);  // low dword of the 64-bit product
  CHECK(neg32(INT32_MIN) == INT32_MIN);
  CHECK(abs32(INT32_MIN) == INT32_MIN && abs32(-5) == 5);
  CHECK(shl32(1, 33) == 2 && shl32(-1, 31) == INT32_MIN);
  CHECK(sar32(-8, 1) == -4 && sar32(-1, 31) == -1 && sar32(-16, 36) == -1 && sar32(16, 36) == 1);
  CHECK(shr32(0x80000000u, 31) == 1u && shr32(0x80000000u, 63) == 1u);
  CHECK(div32(-7, 2) == -3 && mod32(-7, 2) == -1 && div32(7, -2) == -3 && mod32(7, -2) == 1);
  CHECK(throws<DivideError>([] { div32(1, 0); }));
  CHECK(throws<DivideError>([] { div32(INT32_MIN, -1); }));
  CHECK(throws<DivideError>([] { mod32(INT32_MIN, -1); }));
  CHECK(throws<DivideError>([] { udiv32(1, 0); }));
  CHECK(sx8(0x80) == -128 && sx8(0x17F) == 127 && sx16(0xFFFF) == -1 && zx8(0xFFFFFF80u) == 0x80u);
  CHECK(ftol_low32(-1.9) == 0xFFFFFFFFu && ftol_low32(2.9) == 2u);
  CHECK(ftol_low32(4294967296.0 + 5.0) == 5u);  // fistp m64 keeps the low dword
  volatile double zero = 0.0;
  CHECK(ftol_low32(zero / zero) == 0u);  // NaN -> integer indefinite, low dword 0
  CHECK(ftol_low32(1e300) == 0u);
}

static void test_ptr() {
  Ctx ctx;
  CHECK(ctx.mem.set_data_base(0x20000000u));
  std::vector<uint8_t> blk(64, 0);
  CHECK(ctx.mem.map(0x30000000u, blk.data(), 64, false));
  Ptr<int32_t> p(ctx.mem, 0x30000000u);
  p.set(-5);
  CHECK(p.get() == -5 && p[0] == -5);
  (p + 3).set(7);
  CHECK(p[3] == 7 && (p + 3).va() == 0x3000000Cu && (p + 4 - 1).va() == 0x3000000Cu);
  CHECK(throws<Fault>([&] { (void)p[16]; }));    // one past the block
  CHECK(throws<Fault>([&] { (void)p[-1]; }));    // wraps below the block
  CHECK(p.as<uint8_t>()[0] == 0xFB);
  // GameState members: element access wraps into the neighbouring object like the original
  Ptr<int8_t> types = state_arr(ctx, &GameState::g_tileType);
  CHECK(types.va() == ctx.mem.data_base() + static_cast<uint32_t>(offsetof(GameState, g_tileType)));
  ctx.gs().g_tileType[49 * 50 + 49] = 9;
  CHECK(state_grid(ctx, &GameState::g_tileType, 50)(49, 49) == 9);
  Ptr<TileTypeAttr> attrs = state_arr(ctx, &GameState::g_tileTypeAttrs);
  ctx.gs().g_tileTypeAttrs[2].walk = 4;
  CHECK((attrs + 2).field(&TileTypeAttr::walk).get() == 4);
  CHECK((attrs + 2).field(&TileTypeAttr::walk).va() == attrs.va() + 2 * 0x30 + 0x22);
  CHECK((attrs + 1).field(&TileTypeAttr::name).va() == attrs.va() + 0x30);
  CHECK(state_obj(ctx, &GameState::g_rng).field(&Random::state).va() ==
        ctx.mem.data_base() + static_cast<uint32_t>(offsetof(GameState, g_rng)));
  // string idioms
  const char *s = "abc";
  std::memcpy(blk.data() + 32, s, 4);
  std::memcpy(blk.data() + 16, "xy", 3);
  CHECK(vstrlen(ctx.mem, 0x30000020u) == 3);
  vstrcat(ctx.mem, 0x30000010u, 0x30000020u);
  CHECK(std::strcmp(reinterpret_cast<char *>(blk.data() + 16), "xyabc") == 0);
  std::memcpy(blk.data() + 40, "q?z", 4);
  std::memcpy(blk.data() + 48, "*?", 3);
  CHECK(vstrpbrk(ctx.mem, 0x30000028u, 0x30000030u) == 0x30000029u);
  CHECK(vstrpbrk(ctx.mem, 0x30000020u, 0x30000030u) == 0u);
  // rep movs: dword units, so an overlap of 1 byte forward differs from a byte copy
  for (int i = 0; i < 8; ++i) blk[i] = static_cast<uint8_t>(i + 1);
  rep_movs(ctx.mem, 0x30000001u, 0x30000000u, 4);
  CHECK(blk[1] == 1 && blk[2] == 2 && blk[3] == 3 && blk[4] == 4);
  blk[63] = 1;
  CHECK(throws<Fault>([&] { vstrlen(ctx.mem, 0x3000003Fu); }));  // no NUL before the end of the mapping
}

static void test_gamefiles() {
  CHECK(GameFiles::normalize("./Data\\Golfers.PRO") == "data/golfers.pro");
  MemoryGameFiles m;
  m.put("Data/a.txt", {1, 2, 3});
  CHECK(m.size("data\\A.TXT") == 3 && m.size("b") == -1);
  uint8_t buf[4] = {};
  CHECK(m.read("data/a.txt", 1, buf, 4) == 2 && buf[0] == 2 && buf[1] == 3);
  CHECK(m.read("data/a.txt", 3, buf, 4) == 0);
  CHECK(m.read_all("DATA/a.txt").size() == 3);
  CHECK(throws<MissingResource>([&] { m.read_all("nope"); }));
  NullGameFiles n;
  CHECK(!n.exists("data/a.txt"));
  Ctx ctx;
  CHECK(throws<MissingResource>([&] { ctx.files(); }));
  CHECK(throws<MissingResource>([&] { ctx.exe(); }));
  ctx.files_ptr = &m;
  CHECK(ctx.files().exists("data/a.txt"));
}

static void test_execonst(const char *path) {
  ExeConstants bad;
  std::string why;
  const uint8_t junk[128] = {};
  CHECK(!bad.load_bytes(junk, sizeof(junk), &why) && !why.empty() && !bad.loaded());
  CHECK(throws<MissingResource>([&] { bad.bytes("f_tab_dirDx_4c2878"); }));
  CHECK(ExeConstants::descriptors().size() > 300);
  CHECK(ExeConstants::find("f_tab_dirDx_4c2878") != nullptr && ExeConstants::find("no such") == nullptr);
  if (!path) {
    std::printf("execonst: no exe (set SG_EXE_PATH), file checks skipped\n");
    return;
  }
  ExeConstants e;
  if (!e.load_file(path, &why)) {
    std::printf("FAIL execonst: %s refused: %s\n", path, why.c_str());
    ++g_fail;
    return;
  }
  // the direction tables: eight int32 each, read by name
  int32_t dx[8], dy[8];
  for (uint32_t i = 0; i < 8; ++i) {
    dx[i] = e.get<int32_t>("f_tab_dirDx_4c2878", i);
    dy[i] = e.get<int32_t>("f_tab_dirDy_4c2898", i);
  }
  int unit = 0;
  for (int i = 0; i < 8; ++i) unit += (abs32(dx[i]) <= 1 && abs32(dy[i]) <= 1 && (dx[i] | dy[i]) != 0) ? 1 : 0;
  CHECK(unit == 8);  // eight unit steps (the compass directions)
  CHECK(throws<MissingResource>([&] { e.get<int32_t>("f_tab_dirDx_4c2878", 8); }));
  // fill_state: const objects equal the file's .data bytes, the bss table g_sinTable stays zero
  auto gs = std::make_unique<GameState>();
  e.fill_state(*gs, ExeConstants::Fill::ConstObjects);
  CHECK(std::memcmp(gs->f_tab_dirDx_4c2878, dx, sizeof(dx)) == 0);
  CHECK(gs->g_sinTable[64] == 0);
  // a byte changed inside a descriptor is refused by name
  FILE *fp = std::fopen(path, "rb");
  std::vector<uint8_t> all;
  if (fp) {
    uint8_t b[65536];
    size_t n;
    while ((n = std::fread(b, 1, sizeof(b), fp)) > 0) all.insert(all.end(), b, b + n);
    std::fclose(fp);
  }
  const ExeConstDesc *d = ExeConstants::find("f_tab_dirDx_4c2878");
  all[d->file_off] ^= 1;
  ExeConstants e2;
  CHECK(!e2.load_bytes(all.data(), all.size(), &why) && why.find("f_tab_dirDx_4c2878") != std::string::npos);
}

int main(int argc, char **argv) {
  test_arith();
  test_ptr();
  test_gamefiles();
  const char *path = argc > 1 ? argv[1] : std::getenv("SG_EXE_PATH");
  test_execonst(path);
  std::printf("portsim core tests: %s (%d failures)\n", g_fail ? "FAIL" : "ok", g_fail);
  return g_fail ? 1 : 0;
}
