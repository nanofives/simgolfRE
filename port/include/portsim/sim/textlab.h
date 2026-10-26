// The label pixel lab (roadmap step S5.2, notes log/roadmap/S5/2_notes.md): a provider-neutral entry that draws text with
// whatever TextMetrics provider the context has attached (commands.h: gditext's GdiTextMetrics, or another one), so one
// harness (port/test/labels.py) rasterizes the label corpus with any provider and compares the results pixel by pixel.
// Module `textlab` (port/src/sim/textlab.cpp), tests port/test/test_labels.py.
//
// ENTRIES (registered functions, all through the attached provider; none attached = MissingResource{"text provider"}):
//   textlab.draw(fontref, text, n, x, y, colorref, surf, w, h)
//       TextMetrics::draw_glyphs of the n bytes at `text` at (x, y) (TA_BASELINE, BkMode TRANSPARENT, SetTextColor
//       colorref) on the RGB555 surface of w x h words at `surf` (top-down, pitch w), all in the context's address
//       space; 1 when the provider drew, 0 when it declined.
//   textlab.batch(jobs, count, out, out_size)
//       `count` jobs of the block at `jobs` (layout in textlab.cpp, JOB FORMAT), each drawn on its own surface filled
//       with the job's background word, the surfaces written one after another into `out`; returns the jobs drawn.
//   textlab.width(fontref, text, n) / textlab.ascent(fontref)
//       the provider's glyph_width / glyph_ascent (-1 when it declines).
//   textlab.recordStart() / textlab.recordStop()
//       attach a recorder in place of the provider that logs each draw_glyphs call (font, colour, origin, text, the ink
//       box, the surface words of that box before and after) to the files overlay as `$textlab/calls.bin` (RECORD
//       FORMAT in textlab.cpp); recordStop restores the provider and writes the file. For the GDI provider the
//       recorder is a GdiTextMetrics of its own (gditext's registered entries look the provider up by that class);
//       any other provider is wrapped and every call forwarded.
//
// NEGATIVE CONTROLS (g_negctl_textlab, read RED by port/test/test_labels.py): 1 the last glyph of every run drawn one pixel
// to the right, 2 the text colour's red channel changed by 8 (one RGB555 step), 3 the last character dropped, 4 the whole
// run one pixel to the right.
#ifndef PORTSIM_SIM_TEXTLAB_H
#define PORTSIM_SIM_TEXTLAB_H

#include <cstdint>

#include "portsim/registry.h"

namespace portsim {

// textlab.draw's body: false when the provider declined (no provider attached throws MissingResource{"text provider"})
bool textlab_draw(Ctx &ctx, uint32_t fontref, uint32_t text, int32_t n, int32_t x, int32_t y, uint32_t colorref,
                  uint32_t surf, int32_t w, int32_t h);

}  // namespace portsim

#endif  // PORTSIM_SIM_TEXTLAB_H
