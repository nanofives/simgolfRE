// netQueueMessage, S3List_addVoiceRx (delegated), netListRemove, netPoll (included by net.cpp).

bool RecordingNetHost::queue_pop(uint32_t, Pop &out) {
  if (inbox.empty()) return false;
  out = inbox.front();
  inbox.erase(inbox.begin());
  return true;
}

// netQueueMessage(name, id, flags): adds a peer. A slot already holding `id` returns the id; the first empty
// slot (id 0) takes id, flag byte, name, and flags bit 0 makes it the host (+0x780). With all 16 taken, index 16 (the
// record at +0x6f0) is used from here on and nothing is stored. Then +0x6f8 grows, the host re-sends the roster, the
// session slot +8 gets (name, id), the VoiceRx list +0xc0 adds the id, and slot +0x84 of the list's current payload
// ([[+0xc8... see below]) is started with (0x1000, 0, 0); its result is returned.
uint32_t net_queue_message(Ctx &c, uint32_t self, uint32_t name, uint32_t id, uint32_t flags) {
  S o{c.mem, self};
  uint32_t k = 0;
  for (;;) {
    const uint32_t v = o.pid(k);
    if (v == 0) break;
    if (v == id) return v;
    if (++k >= kPeerCount) break;
  }
  if (k < kPeerCount) {
    const uint32_t s = o.peer(k);
    c.mem.store<uint32_t>(s, id);
    c.mem.store<uint8_t>(s + 0x14, static_cast<uint8_t>(flags));
    c.mem.store<uint8_t>(s + 0x15, 0);
    copy_name(c.mem, s + 0x15, name);
    if (flags & 1) o.w(0x780) = SG_NEGCTL(g_nc_queue) == 1 ? o.w(0x77c) : id;
  }
  o.w(0x6f8) = add32(o.w(0x6f8), 1);
  if (o.w(0x77c) == o.w(0x780)) net_send_list(c, self);
  const uint32_t s = o.peer(k);
  g_host->call(Call::SessAddPeer, {self, s + 0x15, c.mem.load<uint32_t>(s)});
  const uint32_t list = self + 0xc0;
  g_host->call(Call::ListAddVoiceRx, {list, c.mem.load<uint32_t>(s)});
  const uint32_t obj = c.mem.load<uint32_t>(list + 8) != 0 ? c.mem.load<uint32_t>(c.mem.load<uint32_t>(list + 0xc) + 8) : 0;
  return g_host->call(Call::VoiceRxStart, {obj, 0x1000, 0, 0});
}

// netListRemove(key) on the ring list (head +8, cursor +0xc, count +0x10; node: key +4, item +8, next +0xc,
// prev +0x10): the ring is walked from the cursor, a miss leaves the cursor where it started, a hit unlinks the node, moves
// head and cursor on, calls list slot +4 with the item, deletes the item and the node through the host and drops the count.
void net_list_remove(Ctx &c, uint32_t list, uint32_t key) {
  const Memory &m = c.mem;
  auto &head = m.ref<uint32_t>(list + 8), &cur = m.ref<uint32_t>(list + 0xc);
  auto &cnt = m.ref<int32_t>(list + 0x10);
  if (head == 0) return;
  const int32_t count = cnt;
  for (int32_t i = 0; i < count; ++i) {
    const uint32_t node = cur;
    if (m.load<uint32_t>(node + 4) == key) {
      const uint32_t next = m.load<uint32_t>(node + 0xc), prev = m.load<uint32_t>(node + 0x10);
      m.store<uint32_t>(next + 0x10, prev);
      m.store<uint32_t>(prev + 0xc, next);
      if (node == head) head = next;
      cur = next;
      const uint32_t item = m.load<uint32_t>(node + 8);
      g_host->call(Call::ListSlot4, {list, item});
      if (item != 0) g_host->call(Call::ListItemDelete, {item, 1});
      m.store<uint32_t>(node + 8, 0);
      g_host->call(Call::ListNodeDelete, {u32(add32(node, m.load<uint32_t>(m.load<uint32_t>(node) + 4))), 1});
      cnt = SG_NEGCTL(g_nc_list_remove) == 1 ? cnt : cnt - 1;
      break;
    }
    cur = m.load<uint32_t>(node + 0xc);
  }
  // netListRemove: an empty ring clears the head; +0x14 becomes count - 1 (also after a miss)
  const int32_t left = cnt;
  if (left == 0) head = 0;
  m.store<int32_t>(list + 0x14, left - 1);
}
