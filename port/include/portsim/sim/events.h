// Events, news and golfer stories (phase 3.5, 2026-10-19): the text builders that fill g_textBuffer, the bulletin event
// ring, the thought / story bookkeeping of the golfer records and the story file reader.
// Specifications: re/analysis/<subsystem>/<addr>_<name>.md of each function, the hook sources (reading reference) and the
// vector corpus. Strings are named as (pool member, byte offset); the text itself never appears here.
#ifndef PORTSIM_SIM_EVENTS_H
#define PORTSIM_SIM_EVENTS_H

#include <cstdint>

#include "portsim/registry.h"

namespace portsim {

// appendUpgradeText: strcat onto g_textBuffer of: literal A, class name table[i], literal B, class name table[i + 1],
// literal C, then a closing clause when i == 0 (clause 0) or i == 1 (clause 1). `i` is not bounds-checked.
void append_upgrade_text(Ctx &ctx, int32_t i);
// replaceInText: replaces the first occurrence of the string `key` in g_textBuffer by `repl` (nothing when absent).
void replace_in_text(Ctx &ctx, uint32_t key, uint32_t repl);
// thoughtBalance: over the 5 thought words of golfer g: +1 for each whose top two bits are 01, -1 for each 11.
int32_t thought_balance(Ctx &ctx, int32_t g);
// thoughtFlag: first thought word of golfer g: bit 15 set -> 2, else bit 14 clear -> 1, set -> 0.
int32_t thought_flag(Ctx &ctx, int32_t g);
// appendOpinion: strcat of the opinion phrase for `rating` (0..5 fixed phrases, 6 and 7 by `twist`, other 0..0x7f by twist,
// outside 0..0x7f a default phrase) onto g_textBuffer.
void append_opinion(Ctx &ctx, int32_t rating, int32_t twist);
// postEvent: records bulletin event `id` once (date, course, ticker words) unless events are muted or it is already posted.
void post_event(Ctx &ctx, int32_t id, int32_t a, int32_t b);

// runBulletinBoardWithSound: plays sound 0x7e, shows the bulletin board with its animation tick preset (f_w ticker word 99,
// tick 20), then clears both and starts the sound slot. The two screens/audio calls go to the front end and audio backend.
void run_bulletin_board_with_sound(Ctx &ctx);
// loadStoryStrings(file, key, sel, mode): reads story text from the story file of slot `file` of the current theme (through
// the context's GameFiles) into the 8-entry cache and appends / returns it as `mode` says (-1 append the entry to g_textBuffer,
// 1 and others cut at newlines, key -1 the first line). Returns what the original leaves in eax.
int32_t load_story_strings(Ctx &ctx, int32_t file, int32_t key, int32_t sel, int32_t mode);
// computeStoryReply(file, key, hole, g): picks the chapter of the partner's story reply and loads its text.
int32_t compute_story_reply(Ctx &ctx, int32_t file, int32_t key, int32_t hole, int32_t g);
// storyText(a, i, g): the story line of golfer g (or its partner) for hole i, left in g_textBuffer; returns its first character.
int32_t story_text(Ctx &ctx, int32_t a, int32_t i, int32_t g);
// substituteStoryNames(g): the two name tokens of g_textBuffer become the display names of g and its partner slot g ^ 1.
void substitute_story_names(Ctx &ctx, int32_t g);
// advanceStory(g, force): moves the story of a golfer pair one stage on; returns 1 when the pair advanced.
int32_t advance_story(Ctx &ctx, int32_t g, int32_t force);
// emitGolferThought(g, kind, arg): records a thought in the golfer's history, applies its mood effect and may post a tip.
void emit_golfer_thought(Ctx &ctx, int32_t g, int32_t kind, int32_t arg);
// buildGolferComment(kind, arg, place, g): appends the comment text of a thought to g_textBuffer.
void build_golfer_comment(Ctx &ctx, int32_t kind, int32_t arg, int32_t place, int32_t g);
// announceHoleType(hole, kind): the hole-type notice text for `kind` (1..7) in g_textBuffer; kinds 3, 5, 6, 7 also post an event.
void announce_hole_type(Ctx &ctx, int32_t hole, int32_t kind);
// holeMagazineEvent: the per-tick pass over the 18 holes (ratings, notices and the two magazine events).
void hole_magazine_event(Ctx &ctx);
// patronEvent(g): a patron's request or refusal for golfer g (CEO, commissioner, heiress) and the golfer's departure fields.
void patron_event(Ctx &ctx, int32_t g);
// announceBuilding: the construction-unlock pass (new buildings and amenities offered as the course grows).
void announce_building(Ctx &ctx);
// pairGolfersWithStory(p1, p2): pairs two golfers, draws their story and may turn p2 into a VIP.
int32_t pair_golfers_with_story(Ctx &ctx, int32_t p1, int32_t p2);
// announceLandmark: marks one landmark type of golfer g's neighbourhood as unlocked and appends its notice.
void announce_landmark(Ctx &ctx, int32_t g, int32_t who);

}  // namespace portsim

#endif  // PORTSIM_SIM_EVENTS_H
