// Callee wrappers of the phase 3.5 builders: the game's generator, the message ticker, the number formatter and the audio
// requests. Not a module of its own (no functions registered here). See events_hook.h.
namespace portsim {
namespace ev {

namespace {
bool registered(const char *name) { return registry_find(name) >= 0; }

// the CRT's __itoa for radix 10: the digits of `v` (with a leading '-' when negative) and a NUL, written at `buf`
void itoa10(Ctx &ctx, int32_t v, uint32_t buf) {
  uint32_t p = buf;
  uint32_t mag = u32(v);
  if (v < 0) {
    ctx.mem.store<uint8_t>(p++, '-');
    mag = u32(neg32(v));
  }
  uint8_t digits[12];
  int n = 0;
  do {
    digits[n++] = static_cast<uint8_t>('0' + mag % 10u);
    mag /= 10u;
  } while (mag != 0);
  while (n > 0) ctx.mem.store<uint8_t>(p++, digits[--n]);
  ctx.mem.store<uint8_t>(p, 0);
}
}  // namespace

void itoa_into(Ctx &ctx, int32_t v) { itoa10(ctx, v, state_va(ctx, &GameState::g_textBuf)); }

uint32_t itoa_to(Ctx &ctx, int32_t v, uint32_t buf, int32_t radix) {
  (void)radix;   // every caller passes 10
  itoa10(ctx, v, buf);
  return buf;
}

int32_t rng_range(Ctx &ctx, int32_t n) {
  return i32(random_range(state_obj(ctx, &GameState::g_rng).field(&Random::state).ref(), u32(n)));
}

int32_t start_message(Ctx &ctx, int32_t arg, int32_t prio, int32_t sound) {
  if (registered("startMessage")) return i32(call_peer(ctx, "startMessage", {u32(arg), u32(prio), u32(sound)}).eax);
  GameState &gs = ctx.gs();
  const Ptr<uint8_t> dir = state_obj(ctx, &GameState::g_ui_569498).as<uint8_t>();
  if (((dir.get() != 0 || gs.g_msgState53df54 != 0) && prio <= 0) || gs.g_panelListMode == 3) return 0;
  gs.g_msgState53df54 = 0;
  const uint32_t text = text_va(ctx);
  vstrcpy(ctx.mem, state_va(ctx, &GameState::unk_005a6d40), text);
  dir.set(1);
  (state_arr(ctx, &GameState::g_ui_5a34e4) + 2).set(arg);
  gs.g_messageTimer = static_cast<int32_t>(vstrlen(ctx.mem, text)) / ((gs.g_difficulty != 0 ? 1 : 0) + 3) + 16;
  (state_arr(ctx, &GameState::f_w_4c2e00) + 2).set(sound);
  if (prio < 0) (state_arr(ctx, &GameState::g_ui_5694a0) + 1).set(neg32(prio));
  const Ptr<int32_t> out = state_arr(ctx, &GameState::g_ui_56d1a8);
  out.set(rng_range(ctx, 600));
  (out + 1).set(add32(rng_range(ctx, 200), 200));
  return 1;
}

void append_cents(Ctx &ctx, int32_t n) {
  if (registered("appendCents")) {
    call_peer(ctx, "appendCents", {u32(n)});
    return;
  }
  const Memory &m = ctx.mem;
  const uint32_t buf = text_va(ctx);
  state_obj(ctx, &GameState::g_ui_569628).set(n);
  if (n < 0) {
    const uint32_t len = vstrlen(m, buf);
    if (m.load<uint8_t>(buf + len - 1) == '+') m.store<uint8_t>(buf + len - 1, 0);
    uint32_t end = buf + vstrlen(m, buf);
    m.store<uint8_t>(end, '-');
    m.store<uint8_t>(end + 1, 0);
    n = neg32(n);
  }
  const uint32_t num = state_va(ctx, &GameState::g_textBuf);
  itoa10(ctx, n / 100, num);
  vstrcat(m, buf, num);
  uint32_t end = buf + vstrlen(m, buf);
  m.store<uint8_t>(end, '.');
  m.store<uint8_t>(end + 1, 0);
  if (n % 100 < 10) {
    end = buf + vstrlen(m, buf);
    m.store<uint8_t>(end, '0');
    m.store<uint8_t>(end + 1, 0);
  }
  itoa10(ctx, n % 100, num);
  vstrcat(m, buf, num);
}

void backend_call(Ctx &ctx, const char *name, std::initializer_list<uint32_t> args) {
  if (registered(name)) call_peer(ctx, name, args);   // the front end / audio backend's function when it exists
}

void log_tick(Ctx &ctx, int32_t a, int32_t b) {
  if (registered("logTick")) {
    call_peer(ctx, "logTick", {u32(a), u32(b)});
    return;
  }
  // logTick (phase 3.7's): the event history slot of the current game month gets (a | b)
  const int32_t tick = ctx.gs().g_date;
  (state_arr(ctx, &GameState::g_eventHistory) + (tick / 1024 % 500)).set(static_cast<uint16_t>(a | b));
}

void play_sound(Ctx &ctx, int32_t idx, int32_t volume, int32_t pan, int32_t pitch, int32_t f34) {
  if (registered("playSound")) {
    call_peer(ctx, "playSound", {u32(idx), u32(volume), u32(pan), u32(pitch), u32(f34)});
    return;
  }
  // the audio backend is not part of the simulation: without one the request has no effect on simulation state
  (void)volume;
  (void)pan;
  (void)pitch;
  (void)f34;
}

void play_sound_at(Ctx &ctx, int32_t sound, int32_t wx, int32_t wy, int32_t f34) {
  if (registered("playSoundAt")) {
    call_peer(ctx, "playSoundAt", {u32(sound), u32(wx), u32(wy), u32(f34)});
    return;
  }
  (void)wx;
  (void)wy;
  (void)f34;
}

}  // namespace ev
}  // namespace portsim
