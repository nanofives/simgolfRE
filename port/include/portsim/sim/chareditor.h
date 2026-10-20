// The character editor (phase 3.8, stream 38h): showCharacterEditor and its two pickers. Module `chareditor`
// (port/src/sim/chareditor.cpp), tests port/test/test_chareditor.py, notes log/p38/38h_notes.md.
//
// showCharacterEditor is a modal loop over the context's InputSource (commands.h, MODAL BLOCKS): every pass reads the
// cursor once (View_toLocal on the root window), asks for one Blocking dispatch, and acts on a press against the control
// under the cursor read in that pass; after a press it waits for the button's release with two pump loops, which deliver
// only what the original dispatched there (a trace-driven source by its cursor count; never a batch that belongs to the
// next pass). Its command effect: the edited character record (the golfer type record: title, name, personality,
// demographics, body, looks, the custom word, the bio), the phrase lines of that type, the member's skill bytes, the preview
// golfer 0x99, the pro record of a member, the comment tone and the text it builds for drawing in g_textBuffer.
// Drawing, sound and the screen Image are dropped. The bio entry (an EditBox in the window tree) is not ported: a press on
// it throws MissingResource before any of its writes.
#ifndef PORTSIM_SIM_CHAREDITOR_H
#define PORTSIM_SIM_CHAREDITOR_H

#include <cstdint>

#include "portsim/registry.h"

namespace portsim {

// showCharacterEditor (character, golfer): edits golfer type record `character` (golfer `golfer` is the member whose skills
// and pro record follow it); returns 0 (the original returns the screen Image destructor's eax, which no caller reads)
int32_t show_character_editor(Ctx &ctx, int32_t character, int32_t golfer);
// pickCharEditor (row): the body / outfit picker; returns the chosen preset index, -1 (right button or a press above the
// picker) or 0 (any other button value)
int32_t pick_char_editor(Ctx &ctx, int32_t row);
// nearestCharControl (x, y): the editor control nearest to the point (within 0x28), or -1
int32_t nearest_char_control(Ctx &ctx, int32_t x, int32_t y);
// charEditorZone (x, y): the picker zone under the point (0..9 presets, 10 / 11 the scroll arrows), or -1
int32_t char_editor_zone(Ctx &ctx, int32_t x, int32_t y);

}  // namespace portsim

#endif  // PORTSIM_SIM_CHAREDITOR_H
