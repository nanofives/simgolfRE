// netSendShutdown, netHandleControl (included by net.cpp).

// netSendShutdown(): tears the session down. Stream slot +0x14, timeout +0xf0 = 20000, +0x77c / +0x780 / +0xec /
// +0x6f8 = 0. With an interface: inside the critical section +0x38 SetEvent(+0x20), DirectPlay Close and Release, the
// interface = 0, +0x28 = 0, Leave, Sleep(0), an empty Enter / Leave, +0x2c / +0x30 / +0xc = 0. Then the critical section
// is deleted when +0x34 is set, the handles +0x14..+0x24 closed and the blocks +0x720 +0x724 +0x6fc +4 freed (each only
// when nonzero, then cleared), the peer slots' +0 +4 +8 and flag byte cleared, the VoiceRx list drained, the queues
// +0xf8 +0x120 +0x148 cleared and the counters reset. The host performs the imports and frees.
void net_send_shutdown(Ctx &c, uint32_t self) {
  S o{c.mem, self};
  g_host->call(stream(0x14), {self + 0x64});
  o.w(0xf0) = SG_NEGCTL(g_nc_shutdown) == 1 ? 10000 : 20000;
  o.w(0x77c) = 0;
  o.w(0x780) = 0;
  o.w(0xec) = 0;
  o.w(0x6f8) = 0;
  if (iface(c) != 0) {
    const uint32_t cs = self + 0x38;
    g_host->call(Call::ImpEnterCs, {cs});
    g_host->call(Call::ImpSetEvent, {o.w(0x20)});
    g_host->call(Call::DpClose, {iface(c)});
    g_host->call(Call::DpRelease, {iface(c)});
    iface(c) = 0;
    o.w(0x28) = 0;
    g_host->call(Call::ImpLeaveCs, {cs});
    g_host->call(Call::ImpSleep, {0});
    g_host->call(Call::ImpEnterCs, {cs});
    g_host->call(Call::ImpLeaveCs, {cs});
    o.w(0x2c) = 0;
    o.w(0x30) = 0;
    o.w(0xc) = 0;
  }
  if (o.w(0x34) != 0) {
    o.w(0x34) = 0;
    g_host->call(Call::ImpDeleteCs, {self + 0x38});
  }
  for (uint32_t off : {0x14u, 0x18u, 0x1cu, 0x20u, 0x24u})
    if (o.w(off) != 0) {
      g_host->call(Call::ImpCloseHandle, {o.w(off)});
      o.w(off) = 0;
    }
  for (uint32_t off : {0x720u, 0x724u, 0x6fcu, 4u})
    if (o.w(off) != 0) {
      g_host->call(Call::ImpFree, {o.w(off)});
      o.w(off) = 0;
    }
  o.w(8) = 0;
  o.w(0x718) = 0xc8;
  o.w(0xe8) = 0;
  for (uint32_t k = 0; k < kPeerCount; ++k) {
    o.pid(k) = 0;
    c.mem.store<uint32_t>(o.peer(k) + 4, 0);
    c.mem.store<uint32_t>(o.peer(k) + 8, 0);
    c.mem.store<uint8_t>(o.peer(k) + 0x14, 0);
  }
  const uint32_t L = self + 0xc0;
  const Memory &m = c.mem;
  if (m.load<uint32_t>(L + 8) != 0) {
    for (int32_t k = 0, n = m.load<int32_t>(L + 0x10); n > 0 && k < m.load<int32_t>(L + 0x10); ++k) {
      const uint32_t node = m.load<uint32_t>(L + 8), payload = m.load<uint32_t>(node + 8);
      m.store<uint32_t>(L + 0xc, m.load<uint32_t>(node + 0xc));
      g_host->call(Call::ListSlot4, {L, payload});
      if (payload != 0) g_host->call(Call::ListItemDelete, {payload, 1});
      m.store<uint32_t>(node + 8, 0);
      const uint32_t n2 = m.load<uint32_t>(L + 8);
      if (n2 != 0) g_host->call(Call::ListNodeDelete, {u32(add32(n2, m.load<uint32_t>(m.load<uint32_t>(n2) + 4))), 1});
      m.store<uint32_t>(L + 8, m.load<uint32_t>(L + 0xc));
    }
    m.store<uint32_t>(L + 8, 0);
    m.store<uint32_t>(L + 0x14, 0);
    m.store<uint32_t>(L + 0x10, 0);
  }
  o.w(0xe4) = 0;
  o.w(0xe8) = 0;
  for (uint32_t off : {0xf8u, 0x120u, 0x148u}) g_host->call(Call::ListClear, {self + off});
  for (uint32_t off : {0x708u, 0x70cu, 0x710u, 0x714u}) o.w(off) = 0;
  o.w(0xf4) = 1;
  for (uint32_t off : {0x54u, 0x58u, 0x5cu, 0x60u}) o.w(off) = 0;
}
