// netNameOf, netBeginOp0 / 1, netFlush, netPump, netSendList (included by net.cpp).

// netNameOf(id): the address of the name of the first of the 16 peer slots whose id equals `id`, else 0.
uint32_t net_name_of(Ctx &c, uint32_t self, uint32_t id) {
  S o{c.mem, self};
  for (uint32_t k = 0; k < kPeerCount; ++k)
    if (o.pid(k) == id) return o.peer(k) + (SG_NEGCTL(g_nc_name_of) == 1 ? 0x14 : 0x15);
  return 0;
}

// netBeginOp0 / 1(a): 7 without an interface; with none of bits 29 / 30 of the flag word set,
// mode +0x60 = 0 / 1, destination +0xe4 = a, stream slot +0x90, flag bit 29; 0.
static uint32_t begin_op(Ctx &c, uint32_t self, uint32_t a, uint32_t mode, int nc) {
  if (iface(c) == 0) return 7;
  S o{c.mem, self};
  if ((o.w(0xe8) & 0x60000000u) == 0) {
    o.w(0x60) = SG_NEGCTL(nc) == 1 ? mode ^ 1 : mode;
    o.w(0xe4) = a;
    g_host->call(stream(0x90), {self + 0x64});
    o.w(0xe8) |= 0x20000000u;
  }
  return 0;
}
uint32_t net_begin_op0(Ctx &c, uint32_t self, uint32_t a) { return begin_op(c, self, a, 0, g_nc_begin0); }
uint32_t net_begin_op1(Ctx &c, uint32_t self, uint32_t a) { return begin_op(c, self, a, 1, g_nc_begin1); }

// netFlush(): bit 29 clear returns the caller's eax (0 here); else stream slot +0x20, bit 29 cleared, then
// netSendMessages(0, 0, +0xe4, 0x200, 0) and its result.
uint32_t net_flush(Ctx &c, uint32_t self, uint32_t eax_in) {
  S o{c.mem, self};
  if ((o.w(0xe8) & 0x20000000u) == 0) return eax_in;
  g_host->call(stream(0x20), {self + 0x64});
  const uint32_t dest = o.w(0xe4);
  if (SG_NEGCTL(g_nc_flush) != 1) o.w(0xe8) &= 0xdfffffffu;
  return g_host->call(Call::NetSendMessages, {self, 0, 0, dest, 0x200, 0});
}

// netPump(): throttles the stream source. With bit 30 of +0xe8 set: pending >= 0x32 returns it, else the bit
// is cleared and the flag word returned. Clear: pending > 0x64 sets the bit and returns stream slot +0x20; else the next
// message (slot +0x88; none = 0) is sent through netService (mode +0x60 != 0) or netSendMessages with its bytes
// [msg+0x18, msg+0x20) to +0xe4 as type 0x100, and stream slot +0x8c releases it.
uint32_t net_pump(Ctx &c, uint32_t self) {
  S o{c.mem, self};
  const uint32_t lim = SG_NEGCTL(g_nc_pump) == 1 ? 0x33 : 0x32;
  if (o.w(0xe8) & 0x40000000u) {
    const uint32_t n = g_host->call(stream(0x94), {self + 0x64});
    if (n >= lim) return n;
    o.w(0xe8) &= 0xbfffffffu;
    return o.w(0xe8);
  }
  const uint32_t n = g_host->call(stream(0x94), {self + 0x64});
  if (n > 0x64) {
    o.w(0xe8) |= 0x40000000u;
    return g_host->call(stream(0x20), {self + 0x64});
  }
  const uint32_t msg = g_host->call(stream(0x88), {self + 0x64});
  if (msg == 0) return 0;
  const uint32_t b = c.mem.load<uint32_t>(msg + 0x18), len = sub32(c.mem.load<uint32_t>(msg + 0x20), b);
  g_host->call(o.w(0x60) != 0 ? Call::NetService : Call::NetSendMessages, {self, b, len, o.w(0xe4), 0x100, 0});
  return g_host->call(stream(0x8c), {self + 0x64, msg});
}

// netSendList(): the host (+0x77c == +0x780) with n = +0x6f8 peers sends the roster (type 0x10, flags 1):
// n * 0x2c + 4 bytes, byte 0 = n, per peer from +4 a 0x28-byte record {id, name (31 characters, byte 0x1f cleared in the
// slot first), flag byte as a dword at +0x24}. Bytes the original never writes (1..3) are 0 here (U-1771).
void net_send_list(Ctx &c, uint32_t self) {
  S o{c.mem, self};
  if (o.w(0x77c) != o.w(0x780)) return;
  const int32_t n = static_cast<int32_t>(o.w(0x6f8));
  if (n == 0) return;
  const uint32_t size = mul32(static_cast<uint32_t>(n), SG_NEGCTL(g_nc_send_list) == 1 ? 0x28 : 0x2c) + 4;
  std::vector<uint8_t> buf(size, 0);
  buf[0] = static_cast<uint8_t>(n);
  for (int32_t k = 0; k < n; ++k) {
    const uint32_t s = o.peer(static_cast<uint32_t>(k));
    c.mem.store<uint8_t>(s + 0x34, 0);
    const uint32_t rec = 4 + 0x28u * static_cast<uint32_t>(k);
    if (rec + 0x28 > size) throw Fault{self, 0};
    *reinterpret_cast<uint32_t *>(&buf[rec]) = c.mem.load<uint32_t>(s);
    for (uint32_t i = 0; i < 0x20; ++i) {
      const uint8_t ch = c.mem.load<uint8_t>(s + 0x15 + i);
      buf[rec + 4 + i] = ch;
      if (!ch) break;
    }
    *reinterpret_cast<uint32_t *>(&buf[rec + 0x24]) = c.mem.load<uint8_t>(s + 0x14);
  }
  g_host->call(Call::NetSendMessages, {self, 0, size, 0, 0x10, 1}, &buf);
}
