// commands.cpp part: panelListSelect, the action of an item of a list panel. mainLoop's H1 calls it for the letter keys of
// the list panels (seven sites) with (item, direct). It had no C3 reimplementation (C2); this is transcribed from its
// instructions (the addresses are in log/p3/p37b_notes.txt and the names row re/names/model_h1.tsv).

// panelListSelect (item, direct): g_flags bit 1 cleared, then by g_panelListMode:
//   3  the shot style: items 3..7 set g_playerShotStyle to 0, -1, 1, 3, 4 (other items nothing); eax = item - 3
//   2  a golfer of the list: an item at or above g_panelListCount clears g_msgState53df54 (eax = the count); otherwise
//      id = g_panelListIds[item]; with g_msgState53df54 <= 0 and the golfer record's word +8 == -1 the state is cleared,
//      else it becomes id + 1; the camera tile pair moves to the record's position >> 10 (eax = the y tile)
//   1  a hole: f_w_4c2848[3] = g_panelListIds[item], or -1 when that is not below g_buildingUnlockLevel or it is 4 with no
//      amenity unlocked; g_panelSelectPending is saved into unk_0056a514 and set to -1; sound 0xfc (eax = playSound's)
//   else the build tool: f_w_4c2848[0] = item (direct != 0) or g_themeSlots59bfc4[item]; sound 0xfb; a tool whose
//      g_tileTypeAttrs record has byte +0x26 == 0xd at g_difficulty above 3 is refused: the refusal text into
//      g_textBuffer, startMessage(0x80004210, 1, -1), sound 0x18 and f_w_4c2848[0] restored (eax = playSound's); otherwise
//      eax = the new f_w_4c2848[0]
int32_t panel_list_select(Ctx &ctx, int32_t item, int32_t direct) {
  GameState &gs = ctx.gs();
  const Memory &m = ctx.mem;
  const int mode = SG_NEGCTL(g_negctl_panel_list);
  gs.g_flags &= ~2u;
  const int32_t list_mode = gs.g_panelListMode;
  if (list_mode == 3) {
    const int32_t k = sub32(item, 3);
    static const int32_t kStyle[5] = {0, -1, 1, 3, 4};
    if (u32(k) <= 4) gs.g_playerShotStyle = (mode == 1 && k == 4) ? 3 : kStyle[k];
    return k;
  }
  const uint32_t ids = GS(g_panelListIds);
  if (list_mode == 2) {
    if (item >= gs.g_panelListCount) {
      gs.g_msgState53df54 = 0;
      return gs.g_panelListCount;
    }
    const int32_t id = m.load<int32_t>(ids + 4 * u32(item));
    const uint32_t rec = GS(g_golfers) + (u32(id) << 8);
    if (gs.g_msgState53df54 <= 0 && m.load<int32_t>(rec + 8) == -1) gs.g_msgState53df54 = 0;
    else gs.g_msgState53df54 = add32(id, 1);
    gs.gCameraTileX = sar32(m.load<int32_t>(rec), 10);
    gs.gCameraTileY = sar32(m.load<int32_t>(rec + 4), 10);
    return gs.gCameraTileY;
  }
  if (list_mode == 1) {
    const int32_t id = m.load<int32_t>(ids + 4 * u32(item));
    gs.f_w_4c2848[3] = id;
    if (id >= gs.g_buildingUnlockLevel || (id == 4 && gs.g_amenityUnlockMask == 0)) gs.f_w_4c2848[3] = -1;
    gs.unk_0056a514 = gs.g_panelSelectPending;
    gs.g_panelSelectPending = -1;
    return i32(call_optional(ctx, "playSound", {0xfc, 0x50, 0, 0, 0}).eax);
  }
  const int32_t old = gs.f_w_4c2848[0];
  gs.f_w_4c2848[0] = direct != 0 ? item : m.load<int32_t>(GS(g_themeSlots59bfc4) + 4 * u32(item));
  call_optional(ctx, "playSound", {0xfb, 0x50, 0, 0, 0});
  const int32_t tool = gs.f_w_4c2848[0];
  if (m.load<uint8_t>(GS(g_tileTypeAttrs) + u32(mul32(tool, 0x30)) + 0x26) != 0xd || gs.g_difficulty <= (mode == 2 ? 2 : 3))
    return tool;
  cpy(ctx, GS(g_textBuffer), GS(f_strpool_4c5344));
  start_message(ctx, i32(0x80004210u), 1, -1);
  const int32_t r = i32(call_optional(ctx, "playSound", {0x18, 0x64, 0, 0, 0}).eax);
  gs.f_w_4c2848[0] = old;
  return r;
}
