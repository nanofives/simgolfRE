// Test-only entries to drive and read the recording NetHost through the C ABI (included by net.cpp, in namespace net).
// "net.hostReset"(): clear the log, the answers and the inbox. "net.hostAnswer"(call, value): the value the host answers
// for that Call (default 0). "net.hostPush"(user, va, len): queue a receive-queue entry with `len` bytes copied from `va`.
// "net.hostLogCount"() / "net.hostLogCall"(i) / "net.hostLogArgc"(i) / "net.hostLogArg"(i, j) / "net.hostPayloadLen"(i) /
// "net.hostPayloadByte"(i, j) read the log back.
std::map<uint32_t, uint32_t> g_answers;
uint32_t answer_script(Call c, const std::vector<uint32_t> &, void *) {
  auto it = g_answers.find(static_cast<uint32_t>(c));
  return it == g_answers.end() ? 0 : it->second;
}
void t_reset(Ctx &, const uint32_t *, Ret &) {
  g_default.log.clear();
  g_default.inbox.clear();
  g_answers.clear();
  g_default.script = answer_script;
  g_host = &g_default;
}
void t_answer(Ctx &, const uint32_t *a, Ret &) { g_answers[a[0]] = a[1]; }
void t_push(Ctx &c, const uint32_t *a, Ret &) {
  Pop p;
  p.user = a[0];
  for (uint32_t i = 0; i < a[2]; ++i) p.bytes.push_back(c.mem.load<uint8_t>(a[1] + i));
  g_default.inbox.push_back(p);
}
const Logged &lg(uint32_t i) {
  if (i >= g_default.log.size()) throw Fault{i, 0};
  return g_default.log[i];
}
void t_count(Ctx &, const uint32_t *, Ret &r) { r.set_u32(static_cast<uint32_t>(g_default.log.size())); }
void t_call(Ctx &, const uint32_t *a, Ret &r) { r.set_u32(static_cast<uint32_t>(lg(a[0]).c)); }
void t_argc(Ctx &, const uint32_t *a, Ret &r) { r.set_u32(static_cast<uint32_t>(lg(a[0]).args.size())); }
void t_arg(Ctx &, const uint32_t *a, Ret &r) { r.set_u32(lg(a[0]).args.at(a[1])); }
void t_plen(Ctx &, const uint32_t *a, Ret &r) { r.set_u32(static_cast<uint32_t>(lg(a[0]).payload.size())); }
void t_pbyte(Ctx &, const uint32_t *a, Ret &r) { r.set_u32(lg(a[0]).payload.at(a[1])); }
