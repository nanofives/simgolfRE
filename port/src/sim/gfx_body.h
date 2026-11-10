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

// P10C fix 2 (log/roadmap/P10/P10C_notes.md): the root pixels a drawing primitive WRITES while a HUD panel layer is open
// (gfx.cpp gfx_layer_begin / gfx_layer_end). P9B took a layer's pixels from a diff with the root at the layer's open, so a pixel
// the panel drew with the word the terrain already had there (a tree icon over the course's own tree sprites) was transparent
// and showed the world at the panel's moved place. The primitives (draw16, copy8, draw_g16_lut, Surface_stretchBlit, the scaled
// drawB, the scene family's put) call hud_mark for each word they store; outside a panel layer `lo` is null and nothing is
// marked. A plain thread-local (the sim runs one context per thread): one compare per stored word, no state of the frame read.
struct HudWriteMark {
  uintptr_t lo = 0, hi = 0;  // the root surface's words [lo, hi) while a panel layer is open
  uint32_t *gen = nullptr;   // Gfx::HudCapture::wr_gen
  uint32_t cur = 0;          // the open layer's blend generation
  int32_t w = 0;
  int32_t x0 = 0, y0 = 0, x1 = -1, y1 = -1;  // the marked box (inclusive)
};
extern thread_local HudWriteMark g_hud_wm;
inline void hud_mark(const uint16_t *p) {
  HudWriteMark &m = g_hud_wm;
  const uintptr_t a = reinterpret_cast<uintptr_t>(p);
  if (a < m.lo || a >= m.hi) return;
  const size_t i = static_cast<size_t>((a - m.lo) / sizeof(uint16_t));
  m.gen[i] = m.cur;
  const int32_t x = static_cast<int32_t>(i % static_cast<size_t>(m.w)), y = static_cast<int32_t>(i / static_cast<size_t>(m.w));
  if (m.x1 < 0) {
    m.x0 = m.x1 = x;
    m.y0 = m.y1 = y;
    return;
  }
  if (x < m.x0) m.x0 = x;
  if (x > m.x1) m.x1 = x;
  if (y < m.y0) m.y0 = y;
  if (y > m.y1) m.y1 = y;
}
inline void hud_mark_span(const uint16_t *p, size_t n) {  // n stored words from p (a fill's row)
  if (n == 0) return;
  const uintptr_t a = reinterpret_cast<uintptr_t>(p);
  if (a + n * sizeof(uint16_t) <= g_hud_wm.lo || a >= g_hud_wm.hi) return;
  for (size_t k = 0; k < n; ++k) hud_mark(p + k);
}

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
  // a value a body computed for the frame code (roadmap S3.7: Text_put's width), taken once by gfx_take_value
  bool has_value = false;
  int32_t value = 0;
  bool hover_help = false;  // P11C (gfx.h gfx_set_hover_help): the hover help lines are drawn
  bool hover_help_course = true;  // P13A (gfx.h gfx_set_hover_help_course): and the course line with no tool among them
  bool meter_tips = false;  // P22D fix 3 (gfx.h gfx_set_meter_tips): the status boxes' tips are drawn
  int32_t meter_prev = -1, meter_count = 0;  // P22D fix 3: the box under the cursor at the last status bar, and for how many
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

  // P9B: the HUD layer capture (gfx.h GfxHudFrame; gfx.cpp gfx_layer_begin / end, published at Window::refreshFull)
  struct HudCapture {
    bool on = false;
    int depth = 0;            // nested scopes: only the outermost opens and closes a layer
    int32_t cur = -1;         // the open layer's id, -1 between layers
    bool any = false;         // a layer opened since the last publish (base and mark are valid)
    bool aborted = false;     // a Window::refreshFull ran inside a layer: its close makes no layer
    std::vector<uint16_t> base, mark;  // the root before the first layer; the root at the last layer boundary
    uint32_t gen = 0;         // blend records of the open layer: valid where rec_gen == gen
    std::vector<uint32_t> rec_gen;
    std::vector<GfxLayerPx> rec;   // kind 2 / 3 with px = the word the blend wrote
    std::vector<GfxHudLayer> layers;
    GfxHudFrame published;
    uint64_t serial = 0;
    int64_t seg = 0;          // P9C: the segment counter (a layer, or the centred gap between two), advanced at every boundary
    // P9D (gfx.h gfx_set_hud_modal): the modal screens over the HUD
    bool modal = false;       // the feature is on (the app's host)
    bool carry = false;       // the capture continues the last layered frame: base and layers are its, mark its root
    int64_t carry_seg = -1;   // the carried centred gap's segment (its text runs keep it across the carried publishes)
    std::vector<GfxHudLayer> carry_layers;  // the last layered frame's layers without its child-window composition
    uint32_t dim_gen = 0;     // a whole-frame dim was drawn in the segment of blend generation dim_gen (dim_set), source dim_src
    bool dim_set = false;
    uint16_t dim_src = 0;
    // P9E (gfx.h gfx_set_hud_popups): the popups since the frame's picture started
    bool popups = false;              // the feature is on (the app's host)
    std::vector<GfxHudPiece> pieces;  // box filled at a publish
    std::vector<int32_t> open;        // the open popups' piece indices (nested: innermost last)
    std::vector<uint8_t> owner;       // per root pixel: piece index + 1 of the popup open at its last observed change
    std::vector<uint16_t> own_mark;   // the root at the last ownership boundary
    // P9E fix 2 (gfx.h gfx_set_hud_close): the carry was armed by a publish inside an open panel scope (a modal screen the panel's
    // own code opened, the hire dialog): the next layer of the same frame continues the carried capture instead of ending it
    bool close_keep = false;
    bool cursor_sprites = false;      // P9E fix 4 (gfx.h gfx_set_hud_cursor): the wide scene around the cursor passes is kept
    int cursor_depth = 0;             // P9E fix 4: open cursor passes
    std::vector<uint8_t> cursor2d;    // P9E fix 4: the root pixels their sprite draws changed (cleared at the queue's start)
    bool lines_on = false;            // P10I fix 1 (gfx.h gfx_set_hud_lines): the tee lines of the cursor passes are recorded
    int bubble_depth = 0;             // P23D fix 3 (gfx.h GfxBubbleScope): inside the frame code's comment bubbles
    bool edge_on = false;             // P22E item 7 (gfx.h gfx_set_hud_edge): the placement box's edges too, and the edge-cut texts
    bool edge_frame = false;          // P23F (gfx.h gfx_set_frame_edge_text): the frame's own texts over the course too
    int32_t wide_e = 0;               // P23F fix 2: the wide scene's side columns (800-space) at this frame's queue (0: none)
    int32_t wide_in = 0;              // P23F fix 3: the 800-space columns each side of the frame that lie inside the window (floor)
    bool tip_edge = false;            // P23F fix 2: inside P11C's course tip drawn at a cursor near or past the frame's edge
    uint32_t wide_lines = 0;          // P23E (gfx.h gfx_set_hud_wide_lines): kGfxWide* bits, more still lines recorded
    uint32_t wide_scope = 0;          // P23E (gfx.h GfxWideLineScope): the kGfxWide* rows whose draws are open
    std::vector<GfxHudLine> lines;    // P10I fix 1: those lines (cleared at the queue's start, as cursor2d)
    std::vector<uint8_t> line800;     // P10I fix 1: the root pixels they changed
    std::vector<uint16_t> line_word;  // P10I fix 1: the word each left there
    bool carry_split = false;
    // P10C fix 2 (HudWriteMark): per root pixel the blend generation of the last panel layer that wrote it, and that layer's
    // marked box (inclusive; x1 < 0 = none); hud_diff keeps a written pixel as the layer's even when its word did not change
    std::vector<uint32_t> wr_gen;
    int32_t wr_box[4] = {0, 0, -1, -1};
    bool wr_on = true;                // the marking (gfx.h gfx_set_hud_writes; off = P9B's diff only)
    // P13D item 17 (log/roadmap/P13/P13D_notes.md): the marking also runs over a modal screen's centred gap (hud_carry_arm, a screen's
    // first call inside a panel), so a pixel the screen's art wrote with the word already under it stays the screen's: before, such
    // pixels were left out of the centred layer and the window showed what is beneath at that place (the panels at their moved
    // places, the course at the window's resolution): dark specks and arcs on the reports (the histograph's axis column)
    bool wr_gap = false;
  } hud;
  // P9C: the text runs (gfx.h GfxTextRun; gfx.cpp text_run_begin / end, peeled at Window::refreshFull)
  struct TextCapture {
    bool on = false;
    struct Run {
      GfxTextRun info;                     // box in back-buffer coordinates
      std::vector<uint16_t> before, after;  // the box's words before / after the call
      int64_t seg = -1;                    // the HUD segment it was drawn in, -1 = the base (before the frame's first layer)
      uint64_t frame = 0;                  // P23F fix 3: frame_gen when it was drawn (a run kept from an earlier frame loses ex0 / ex1)
    };
    std::vector<Run> live;                 // on the root and the window targets, oldest first
    std::vector<Run> *sink = nullptr;      // where new runs go (live, or the banner's own list while loading.banner runs)
    uint64_t serial = 0;
    GfxTextFrame published, banner;
    GfxTextStats stats;
    std::vector<GfxTextLayerPatch> patches_pending;  // the layer patches this frame's closed segments made
    // P9D: the runs (layer >= 0) and patches of the last layered publish's carried layers, published again with a carried frame
    std::vector<GfxTextRun> carry_runs;
    std::vector<GfxTextLayerPatch> carry_patches;
    // the wide scene (G32, gfx_scene.cpp on_wide): while `wide` is set, runs drawn on the swapped root are the scene's, kept in
    // wide_runs with their boxes moved by wide_ox (scene -> frame columns); at the queue's end they are peeled off the scene and
    // the ones not inside the frame's columns are published with each frame as world runs (GfxTextRun::target 2, layer -3)
    bool wide = false;
    int32_t wide_ox = 0;
    std::vector<Run> wide_runs;
    std::vector<GfxTextRun> wide_published;
    // P22E item 7 (gfx.h gfx_set_hud_edge): the cursor passes' texts whose whole ink lies past the root's left or right edge (they
    // changed no root pixel, so nothing peels them): the base's runs with an empty box in the root and their columns in ex0 / ex1,
    // published with every frame until the next queue's start clears them (as the tee lines)
    std::vector<GfxTextRun> edge_runs;
    // P23F (gfx.h gfx_set_frame_edge_text): the same for the frame's own texts over the course; cleared when the next frame's
    // terrain picture is made (text_frame_start), published with every frame until then
    std::vector<GfxTextRun> frame_edge_runs;
    // P23F fix 3: the frames whose terrain picture was made (text_frame_start); a run drawn in an earlier frame that the peel keeps
    // (a few of its pixels still hold its after words, e.g. a label that moved a few pixels over its old place) shows only those
    // pixels: text_publish clears its edge columns, which GfxTextRun::visible would count as visible whole
    uint64_t frame_gen = 0;
    bool queue_label = false;  // P23F fix 3: inside drawQueuedSprite.label's frame copy (gfx_scene.cpp b_queue_label)
  } text;
  // P9B: a blend primitive wrote word `res` at root pixel i: kind 2 (mask blend: src, k) or 3 (shade table k). Recorded only
  // from the first layer of a frame to its publish (in a layer or in the centred gap after one) while the pixel still holds
  // the segment's background (an earlier write of the same layer makes it opaque)
  void note_blend(size_t i, uint16_t before, uint16_t res, uint8_t kind, uint16_t src, uint8_t k) {
    if (!hud.any || i >= hud.mark.size()) return;  // a layer or a centred gap between two layers
    if (hud.rec_gen.size() != hud.mark.size()) {
      hud.rec_gen.assign(hud.mark.size(), 0);
      hud.rec.assign(hud.mark.size(), GfxLayerPx{});
    }
    GfxLayerPx &r = hud.rec[i];
    if (before != hud.mark[i]) {  // over the layer's own pixel: the result is the layer's (opaque)
      r.kind = 1;
    } else {
      r.kind = kind;
      r.src = src;
      r.k = k;
    }
    r.px = res;
    hud.rec_gen[i] = hud.gen;
  }
  // P9D (gfx.cpp, gfx.h gfx_set_hud_modal): the carried capture after a layered publish, its end at the next frame's start, a
  // modal screen's first call inside an open panel layer, the whole-frame dim
  void hud_carry_arm(bool again);
  void hud_carry_drop();
  void hud_modal_mark();
  void note_dim(uint16_t src);
  void hud_own_sync();     // P9E: the root's changes since own_mark become the open popup's (or nobody's)
  void hud_pieces_reset();  // P9E: a new frame picture: no popup pixels left (an open popup stays, owning nothing yet)
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
// the same over a name the caller holds (roadmap S3.6)
int load_image_named(Gfx &g, Ctx &ctx, std::string name, uint32_t pal, int32_t ofs, int32_t count, uint32_t flags, Image8 &img);
// cropSprite(self, src, x, y, w, h, kind, trim) into g.sprites[self]; the original's result code
int crop_sprite(Gfx &g, uint32_t self, const Image8 &src, int32_t x, int32_t y, int32_t w, int32_t h, bool trim);
// jgld Sprite::draw16 at scale 1: bytes >= 0xfe transparent, the others the sprite palette's word
void draw16(const Sprite8 &s, Surface16 &d, int32_t x, int32_t y);
// a 1:1 copy of an 8-bit image's rectangle (x, y, w, h) to (x2, y2) on a 16-bit surface through the image's colour table
void copy8(const Image8 &s, Surface16 &d, int32_t x, int32_t y, int32_t w, int32_t h, int32_t x2, int32_t y2);
// jgld Sprite::drawG at scale 1: 0xff transparent, 0xf8 shades the destination through shade_lut(), the others the palette
void draw_g16(Gfx &g, const Sprite8 &s, Surface16 &d, int32_t x, int32_t y);
// the same with the caller's shade table of lut_n words (roadmap S3.6)
void draw_g16_lut(Gfx &g, const Sprite8 &s, Surface16 &d, int32_t x, int32_t y, const uint16_t *lut, size_t lut_n);
// block 1 of build16BitColorTable in RGB555 (the table g_color16Lut points at)
const std::array<uint16_t, 0x8000> &shade_lut();
// the text drawers' common path (drawRichText's run drawer): plain text, a direct colour, through the context's TextMetrics
void draw_text_string(Gfx &g, Ctx &ctx, std::string text, uint32_t va, int32_t x0, int32_t y0, uint32_t colour, bool centre,
                      const char *who);
// drawCenteredText / drawTextWithColor over a[0] = text, a[1] = x, a[2] = y, a[3] = colour
void draw_text(Gfx &g, Ctx &ctx, const uint32_t *a, bool centre, const char *who);
// drawFrameR3(x, y, w, h, 1): the shaded panel and its blended border
void draw_frame_r3(Gfx &g, Ctx &ctx, int32_t x, int32_t y, int32_t w, int32_t h);
// drawMessageBuffer(cols, x, y, colour) over the text buffer
void draw_message_buffer(Gfx &g, Ctx &ctx, int32_t cols, int32_t x, int32_t y, uint32_t colour);

// P9C: a TextMetrics::draw_glyphs call on a surface (px, w x h words), kept as a text run when text capture is on. `ox, oy` is
// where the surface lands in the back buffer (0, 0 for the root; a window's position), `clip` the target's clip rectangle in the
// surface's coordinates (x0, y0, x1, y1). begin snapshots the surface, end diffs it and records the run.
struct TextRunScope {
  std::vector<uint16_t> snap;
  bool on = false;
  // P22E item 7: the string's whole 800-space columns [ink_x0, ink_x1) (its origin and the provider's measured advance), set by
  // draw_text_string before text_run_end (ink false: not measured)
  bool ink = false;
  int32_t ink_x0 = 0, ink_x1 = 0;
  bool frame = false;  // P23F: a frame text (gfx.h gfx_set_frame_edge_text): its edge run goes to frame_edge_runs
};
void text_run_begin(Gfx &g, const uint16_t *px, int32_t w, int32_t h, TextRunScope &s);
void text_run_end(Gfx &g, Ctx &ctx, TextRunScope &s, const uint16_t *px, int32_t w, int32_t h, int32_t ox, int32_t oy,
                  const int32_t clip[4], uint32_t fontref, const std::string &text, int32_t x, int32_t y, uint32_t colorref,
                  uint8_t target);
// the banner's runs (text family's loading.banner): new runs go to a list of their own until text_banner_end publishes them
// over the banner picture `img`
void text_banner_begin(Gfx &g);
// the wide scene's runs (gfx_scene.cpp, G32): forgotten at the queue's start, peeled off the scene at its end (gfx.cpp)
void text_frame_start(Gfx &g);  // P23F: the frame's terrain picture: the last frame's frame-text edge runs go
void text_wide_begin(Gfx &g);
void text_wide_end(Gfx &g, const std::vector<uint16_t> &scene, int32_t scene_w, int32_t scene_e, std::vector<uint16_t> &scene_text);
}  // namespace gfxb
namespace gfxe {
bool window_origin(Gfx &g, const Surface16 *surf, int32_t &x, int32_t &y);  // gfx_editbox.cpp (P9C)
}
namespace gfxb {
void text_banner_end(Gfx &g, const std::vector<uint16_t> &img);

// the body registered under `name` (core table or a family), or nullptr; `family` gets the owner ("gfx" for the core table)
GfxBody find_body(const char *name, const char **family = nullptr);

}  // namespace gfxb
}  // namespace portsim

#endif
