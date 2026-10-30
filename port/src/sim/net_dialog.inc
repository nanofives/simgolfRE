// netHandleControl (included by net.cpp).

// netHandleControl(id, flag): drops a peer. Returns 0 without removing (id not found, or the dialogs
// answered "no"), 1 when the peer was removed. flag 0 = a timeout asked for it: the host runs the drop dialogs through the
// NetHost (DialogRun(kind, answer)): answer 0 on a client that lost the host = "drop" (returns 1 via the dialog flag word,
// leaves the table alone), 1 = keep, 2 = adjust the tolerance (second dialog; answers 0..3 pick 5000 / 10000 / 20000 /
// 30000 ms into +0xf0), anything else removes. Removing moves slots k+1..15 down one (0x58 bytes each), copies the name,
// leaves slot 15 as it was (the original's defect, switch netHandleControl.slot15Stays: 0 clears it), stores a dropped
// host id in g_net8400ac and makes slot 0 the host, decrements +0x6f8, calls session slot +0xc (name, id), removes the id
// from the VoiceRx list and, for flag 0, broadcasts type 0x80 with the id (guaranteed).
uint32_t net_handle_control(Ctx &c, uint32_t self, uint32_t id, uint32_t flag) {
  S o{c.mem, self};
  const Memory &m = c.mem;
  uint32_t k = 0;
  while (o.pid(k) != id && ++k < kPeerCount) {}
  if (k == kPeerCount) return 0;
  static const uint32_t kMs[4] = {5000, 10000, 20000, 30000};
  if (flag == 0) {
    const uint32_t a = o.w(0x77c), b = o.w(0x780);
    if (a != b && a != o.pid(1)) {
      g_host->call(Call::HudText, {0, o.peer(0) + 0x15});
      g_host->call(Call::HudText, {1, o.peer(1) + 0x15});
      const uint32_t r = g_host->call(Call::DialogRun, {0, 0});
      if (r == 0) return 1;   // the original also sets the render flag word g_flag_83afcc (not net state)
      if (r == 1) return 0;
      if (r == 2) {
        const uint32_t r2 = g_host->call(Call::DialogRun, {0, 1});
        if (r2 <= 3) o.w(0xf0) = kMs[r2];
        return 0;
      }
      return 0;
    }
    g_host->call(Call::HudText, {0, o.peer(k) + 0x15});
    const uint32_t r = g_host->call(Call::DialogRun, {a == b ? 1u : 2u, 0});
    if (r == 1) return 0;
    if (r == 2) {
      const uint32_t r2 = g_host->call(Call::DialogRun, {0, 1});
      if (r2 <= 3) o.w(0xf0) = kMs[r2];
      return 0;
    }
  }
  uint8_t text[0x100];   // the removed peer's name (the original keeps up to 0x100 bytes on its frame)
  uint32_t n = 0;
  for (; m.load<uint8_t>(o.peer(k) + 0x15 + n) && n < 0xff; ++n) text[n] = m.load<uint8_t>(o.peer(k) + 0x15 + n);
  text[n] = 0;
  if (k < 15) {
    for (uint32_t i = 0; i < 0x58u * (15 - k); ++i) m.store<uint8_t>(o.peer(k) + i, m.load<uint8_t>(o.peer(k) + 0x58 + i));
    if (!SG_DEFECT(g_defect_slot15))
      for (uint32_t i = 0; i < 0x58; ++i) m.store<uint8_t>(o.peer(15) + i, 0);
  }
  if (id == o.w(0x780)) {
    c.gs().g_net8400ac = static_cast<int32_t>(id);
    o.w(0x780) = o.pid(0);
    g_host->call(Call::SessOnHostMoved, {self});
  }
  o.w(0x6f8) = sub32(o.w(0x6f8), 1);
  const std::vector<uint8_t> name_bytes(text, text + n + 1);
  g_host->call(Call::SessPeerRemoved, {self, id}, &name_bytes);
  net_list_remove(c, self + 0xc0, id);
  if (flag == 0) {
    const std::vector<uint8_t> drop = {uint8_t(id), uint8_t(id >> 8), uint8_t(id >> 16), uint8_t(id >> 24)};
    g_host->call(Call::NetSendMessages, {self, 0, 4, 0, 0x80, 1}, &drop);
  }
  return SG_NEGCTL(g_nc_handle) == 1 ? 0 : 1;
}
