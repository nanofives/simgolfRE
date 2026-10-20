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

// startMessage / appendCents: phase 3.7a's registered functions (module tick); the local copies stage B kept were replaced
// (2026-10-19). MissingResource when no module registers them.
int32_t start_message(Ctx &ctx, int32_t arg, int32_t prio, int32_t sound) {
  return i32(call_peer(ctx, "startMessage", {u32(arg), u32(prio), u32(sound)}).eax);
}

void append_cents(Ctx &ctx, int32_t n) { call_peer(ctx, "appendCents", {u32(n)}); }

void backend_call(Ctx &ctx, const char *name, std::initializer_list<uint32_t> args) {
  if (registered(name)) call_peer(ctx, name, args);   // the front end / audio backend's function when it exists
}

// logTick: phase 3.7a's registered function (module tick; the local copy was replaced, 2026-10-19)
void log_tick(Ctx &ctx, int32_t a, int32_t b) { call_peer(ctx, "logTick", {u32(a), u32(b)}); }

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
