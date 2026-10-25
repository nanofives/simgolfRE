// What a drawing body of the software graphics backend needs (module `gfx`, portsim/sim/gfx.h; roadmap S2.1). The backend's
// objects (Gfx and its 8-bit images, cropped sprites and 16-bit surfaces), the helpers the bodies share, and the BODY FAMILY
// table: a family is a file port/src/sim/gfx_<family>.cpp that defines
//
//   int g_negctl_gfx_<family> = 0;                       // its negative-control mode (sg_negctl_set "gfx.family.<family>")
//   extern const portsim::GfxFamily kGfxFamily_<family> = {"<family>", kBodies, n, kNotDrawing, m};
//
// and is named once in port/src/sim/gfx_families.inc (`SG_GFX_FAMILY(<family>)`). gfx.cpp references every listed family by
// name (an explicit extern, as port/src/registry.cpp does for modules.inc: portsim is a static library, so a self-registering
// initializer in an otherwise unreferenced object would be dropped by the linker) and dispatches a drop to the body of that
// name. A drop name must have one body over the core table and all families (gfx.check counts duplicates). How to add a
// family: port/test/README.md, "Body families".
//
// Bodies write only the backend's own objects (never the GameState or its address space) and report a case they do not model
// with Gfx::unsup (gfx.log lists every distinct reason in first-occurrence order). Cite functions by name, never an address
// of the original (port/test/test_static.py).
#ifndef PORTSIM_SIM_GFX_BODY_H
#define PORTSIM_SIM_GFX_BODY_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "portsim/registry.h"
#include "portsim/sim/gfx.h"

namespace portsim {

// An 8 bpp image: jgld allocDrawBuffer's top-down DIB section (pitch (w + 3) & ~3) and the colour table the palette gives it.
struct Image8 {
  int32_t w = 0, h = 0, pitch = 0;
  std::vector<uint8_t> px;
  std::array<uint16_t, 256> pal{};
};
// A cropSprite result: the kept rectangle, its trims (the SpriteRef's +0x20 left, +0x24 top) and the source's palette.
struct Sprite8 {
  int32_t w = 0, h = 0, left = 0, top = 0;
  int32_t cut_w = 0, cut_h = 0;   // the requested rectangle's size (the SpriteRef's +0x18 / +0x1c; roadmap S2.8)
  std::vector<uint8_t> px;
  std::array<uint16_t, 256> pal{};
};
// A 16 bpp RGB555 surface, top-down, clip = the whole surface (allocDrawBuffer's setClip of (0, 0, w, h)).
struct Surface16 {
  int32_t w = 0, h = 0;
  std::vector<uint16_t> px;
  void make(int32_t nw, int32_t nh) {
    w = nw;
    h = nh;
    px.assign(static_cast<size_t>(nw) * static_cast<size_t>(nh), 0);   // a new DIB section is zero-filled
  }
};

class Gfx {
 public:
  std::map<uint32_t, Image8> images;    // exe Image objects (loadImage)
  std::map<uint32_t, Sprite8> sprites;  // exe SpriteRef objects (cropSprite)
  Surface16 root, back;                 // the root window's surface (g_rootWindow + 0x274) and the presented frame
  uint32_t fonts[4] = {0, 0, 0, 0};     // the root surface's +0x5c..+0x68 (setDrawTarget)
  std::vector<uint32_t> snap_at;        // clock ordinals to snapshot past, ascending
  size_t snap_next = 0;
  std::vector<size_t> snap_unsupported;  // per snapshot: the distinct unsupported reasons that had occurred before it
  std::map<std::string, uint64_t> handled, unsupported;
  std::vector<std::string> notes;       // every distinct unsupported reason, in first-occurrence order (gfx.log `first`)
  // terrain on the surfaces (roadmap S3.2, S0 recommendation 2): set on the root surface by a terrain pass or the restore of
  // the terrain picture (gfx_terrain.cpp), cleared by a 2D write that covers the whole root surface, copied to the back buffer
  // at Window::refreshFull; each snapshot keeps the back buffer's (gfx.log `snapshot_terrain`)
  int root_terrain = 0, back_terrain = 0;
  std::vector<int> snap_terrain;
  // a 2D write of the rectangle (x, y, w, h) onto the root surface: clears root_terrain when it covers the whole surface
  void note_root_write(int32_t x, int32_t y, int32_t w, int32_t h) {
    if (x <= 0 && y <= 0 && static_cast<int64_t>(x) + w >= root.w && static_cast<int64_t>(y) + h >= root.h) root_terrain = 0;
  }
  // per-family state (roadmap S2.3): a family keeps its own objects under its name, so they live and die with this Gfx
  std::map<std::string, std::shared_ptr<void>> family_state;
  template <class T>
  T &state_of(const char *family) {
    std::shared_ptr<void> &p = family_state[family];
    if (!p) p = std::make_shared<T>();
    return *static_cast<T *>(p.get());
  }

  // a case the backend does not model (counted; the first occurrence of a reason is appended to `notes`)
  void unsup(const std::string &why);
  // the root surface, made at its first use (Window::create's size scaleX(0x140) x scaleX(0xf0))
  void ensure_root(Ctx &ctx);
  // the root window's surface object (the address the frame code passes as a drawing target)
  uint32_t root_va(Ctx &ctx);
};

// ------------------------------------------------------------------------------------------------ the family table
using GfxBody = void (*)(Gfx &g, Ctx &ctx, const uint32_t *a, int n);   // a[0..12): the drop's argument slots, zero-padded
struct GfxBodyDef {
  const char *name;  // the drop's hooks.csv name (or a sub-body name a module hands to gfx_drop, e.g. "drawWindow.frame")
  GfxBody body;
};
struct GfxFamily {
  const char *family;
  const GfxBodyDef *bodies;
  size_t count;
  const char *const *not_drawing;  // drops of this family's area that draw nothing (never reported unsupported); may be null
  size_t not_drawing_count;
};

// ------------------------------------------------------------------------------------------------ shared helpers (gfx.cpp)
namespace gfxb {

inline uint16_t rgb555(uint8_t r, uint8_t g, uint8_t b) {   // Palette::apply: (B >> 3) | (G << 2) & 0x3e0 | (R << 7) & 0x7c00
  return static_cast<uint16_t>(((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
}
// the NUL-terminated string at va in the context's address space
std::string vstring(const Memory &m, uint32_t va);
// a file the backend writes for the harness (snapshots, the log): the files overlay, never the install
bool put_file(Ctx &ctx, const std::string &name, const std::vector<uint8_t> &bytes);
std::vector<uint8_t> surface_bytes(const Surface16 &s);

// loadImage(this, name, pal, ofs, count, flags) into `img` (8-bit RLE PCX through the context's GameFiles); the original's
// result code (0 = loaded)
int load_image(Gfx &g, Ctx &ctx, uint32_t name_va, uint32_t pal, int32_t ofs, int32_t count, uint32_t flags, Image8 &img);
// cropSprite(self, src, x, y, w, h, kind, trim) into g.sprites[self]; the original's result code
int crop_sprite(Gfx &g, uint32_t self, const Image8 &src, int32_t x, int32_t y, int32_t w, int32_t h, bool trim);
// jgld Sprite::draw16 at scale 1: bytes >= 0xfe transparent, the others the sprite palette's word
void draw16(const Sprite8 &s, Surface16 &d, int32_t x, int32_t y);
// a 1:1 copy of an 8-bit image's rectangle (x, y, w, h) to (x2, y2) on a 16-bit surface through the image's colour table
void copy8(const Image8 &s, Surface16 &d, int32_t x, int32_t y, int32_t w, int32_t h, int32_t x2, int32_t y2);
// jgld Sprite::drawG at scale 1: 0xff transparent, 0xf8 shades the destination through shade_lut(), the others the palette
void draw_g16(Gfx &g, const Sprite8 &s, Surface16 &d, int32_t x, int32_t y);
// block 1 of build16BitColorTable in RGB555 (the table g_color16Lut points at)
const std::array<uint16_t, 0x8000> &shade_lut();
// the text drawers' common path (drawRichText's run drawer): plain text, a direct colour, through the context's TextMetrics
void draw_text_string(Gfx &g, Ctx &ctx, std::string text, uint32_t va, int32_t x0, int32_t y0, uint32_t colour, bool centre,
                      const char *who);
// drawCenteredText / drawText4049d0 over a[0] = text, a[1] = x, a[2] = y, a[3] = colour
void draw_text(Gfx &g, Ctx &ctx, const uint32_t *a, bool centre, const char *who);
// drawFrameR3(x, y, w, h, 1): the shaded panel and its blended border
void draw_frame_r3(Gfx &g, Ctx &ctx, int32_t x, int32_t y, int32_t w, int32_t h);
// drawMessageBuffer(cols, x, y, colour) over the text buffer
void draw_message_buffer(Gfx &g, Ctx &ctx, int32_t cols, int32_t x, int32_t y, uint32_t colour);

// the body registered under `name` (core table or a family), or nullptr; `family` gets the owner ("gfx" for the core table)
GfxBody find_body(const char *name, const char **family = nullptr);

}  // namespace gfxb
}  // namespace portsim

#endif
