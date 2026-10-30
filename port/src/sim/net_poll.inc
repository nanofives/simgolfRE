// netPoll (included by net.cpp). The receive queue is fed by a receive thread the exe does not contain, so the host
// supplies the entries (RecordingNetHost::inbox).

uint32_t net_handle_control(Ctx &c, uint32_t self, uint32_t id, uint32_t flag);
void net_send_shutdown(Ctx &c, uint32_t self);

// netPoll(): 0 without an interface, the interface pointer when the receive buffer +4 is NULL; else every
// queued (buffer, user) pair is dispatched. user != 0: a Jackal packet, bit 0x80 of byte 0 = peer drop (+8, 1), bit
// 0x10 = roster (byte +8 records of 0x28 from +0xc: queueMessage(rec+4, id, flag dword)). user == 0: a DirectPlay system
// message by its dword +0: 0x31 session lost, 3 create (host only, +4 == 1: GetPlayerName then queueMessage), 5 destroy
// (+4 == 1: handleControl(+8, 1)), 0x102 peer flag byte, 0x103 session slot +0x10. Returns queuePop's last result (0).
uint32_t net_poll(Ctx &c, uint32_t self) {
  S o{c.mem, self};
  const Memory &m = c.mem;
  if (iface(c) == 0) return 0;
  const uint32_t buf = o.w(4);
  if (buf == 0) return iface(c);
  Pop p;
  while (g_host->queue_pop(self + 0x148, p)) {
    for (size_t i = 0; i < p.bytes.size(); ++i) m.store<uint8_t>(buf + static_cast<uint32_t>(i), p.bytes[i]);
    if (p.user != 0) {
      const uint8_t b = m.load<uint8_t>(buf);
      if (b & 0x80) {
        net_handle_control(c, self, m.load<uint32_t>(buf + 8), 1);
      } else if (b & 0x10) {
        const uint8_t n = m.load<uint8_t>(buf + 8);
        for (uint32_t k = 0; k < n; ++k) {
          const uint32_t e = buf + 0xc + 0x28 * k;
          net_queue_message(c, self, e + 4, m.load<uint32_t>(e), m.load<uint32_t>(e + 0x24));
        }
      }
      continue;
    }
    const uint32_t t = m.load<uint32_t>(buf);
    if (t == 0x31) {
      g_host->call(Call::SessOnHostLost, {self});
      net_send_shutdown(c, self);
    } else if (t == 3) {
      if (o.w(0x77c) == o.w(0x780) && m.load<uint32_t>(buf + 4) == 1) {
        g_host->call(Call::DpGetPlayerName, {iface(c), m.load<uint32_t>(buf + 8), 0x100});
        const uint32_t fp = m.load<uint32_t>(buf + 0x10);
        const uint32_t fl = fp != 0 ? m.load<uint8_t>(fp) : 0;
        net_queue_message(c, self, c.gs().g_ui_83ff9c[3], m.load<uint32_t>(buf + 8), fl);
      }
    } else if (t == 5) {
      if (m.load<uint32_t>(buf + 4) == 1) net_handle_control(c, self, m.load<uint32_t>(buf + 8), 1);
    } else if (t == 0x102) {
      const uint32_t id = m.load<uint32_t>(buf + 8);
      uint32_t k = 0;
      while (o.pid(k) != id && ++k < kPeerCount) {}
      const uint32_t fp = m.load<uint32_t>(buf + 0xc);
      if (k != kPeerCount && fp != 0) m.store<uint8_t>(o.peer(k) + 0x14, SG_NEGCTL(g_nc_poll) == 1 ? 0 : m.load<uint8_t>(fp));
    } else if (t == 0x103) {
      g_host->call(Call::SessSetName, {self, m.load<uint32_t>(buf + 0x18), m.load<uint32_t>(buf + 8)});
    }
  }
  return 0;
}
