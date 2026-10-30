// netServiceControl, netSendShutdown (included by net.cpp).

// netServiceControl(): the 2000 ms keep-alive and the silence timeouts. Returns the last value the original
// leaves in eax (a time difference, a guard, the flag word, a handler result, a time or 0). The two guards are
// g_netControl8400bc[0] (re-entry of the timeout check; stays set after the two early returns of the third arm, as in
// the original) and [1] (re-entry of the keep-alive).
uint32_t net_service_control(Ctx &c, uint32_t self) {
  S o{c.mem, self};
  GameState &g = c.gs();
  auto now = [&]() { return g_host->call(Call::ImpTimeGetTime, {}); };
  if (iface(c) == 0) return 0;
  const uint32_t t = sub32(now(), 0x7d0);
  if (t <= o.w(0xec)) return t;
  if (g.g_netControl8400bc[1] != 0) return static_cast<uint32_t>(g.g_netControl8400bc[1]);
  g.g_netControl8400bc[1] = 1;
  g_host->call(Call::NetSendMessages, {self, 0, 0, 0, 0x20, 0});
  o.w(0xec) = now();
  const uint32_t bc = static_cast<uint32_t>(g.g_netControl8400bc[0]);
  g.g_netControl8400bc[1] = 0;
  if (bc != 0) return bc;
  g.g_netControl8400bc[0] = 1;
  const uint32_t fl = o.w(0xe8);
  if (!(fl & 0x40000u)) {
    g.g_netControl8400bc[0] = 0;
    return fl;
  }
  const uint32_t host = o.w(0x77c);
  const uint32_t tmo = o.w(0xf0);
  if (host == o.w(0x780)) {
    for (uint32_t k = 0; k < kPeerCount; ++k) {
      const uint32_t id = o.pid(k);
      if (id == 0 || id == host) continue;
      const uint32_t t0 = c.mem.load<uint32_t>(o.peer(k) + 0x10);
      if (t0 == 0 || u32(sub32(now(), t0)) <= tmo) continue;
      if (net_handle_control(c, self, id, 0) != 0) continue;
      c.mem.store<uint32_t>(o.peer(k) + 0x10, now());
    }
    g.g_netControl8400bc[0] = 0;
    return 0;
  }
  if (host == o.pid(1)) {
    const uint32_t t0 = o.w(0x180);
    uint32_t r = host;
    if (t0 != 0) {
      r = sub32(now(), t0);
      if (r > tmo) {
        r = net_handle_control(c, self, o.pid(0), 0);
        if (r == 0) {
          r = now();
          o.w(0x180) = r;
        }
      }
    }
    g.g_netControl8400bc[0] = 0;
    return r;
  }
  uint32_t r = host;
  for (uint32_t k = 0; k < 2; ++k) {
    const uint32_t t0 = c.mem.load<uint32_t>(o.peer(k) + 0x10);
    if (t0 == 0) return r;
    r = sub32(now(), t0);
    if (r < tmo) return r;
  }
  r = net_handle_control(c, self, 0xffffffffu, 0);
  if (r == 0) {
    r = now();
    o.w(0x1d8) = r;
    o.w(0x180) = r;
  }
  g.g_netControl8400bc[0] = SG_NEGCTL(g_nc_svc_ctl) == 1 ? 1 : 0;
  return r;
}
