// Callees of the phase 3.3 bodies that belong to other owners (included by golfers.cpp; not a module of its own).
//
// Peers are called by their hooks.csv names through the registry (MissingResource{name} when nobody registered them).
// The view / simulation bridge (worldToScreen, playSoundAt, queueMessage, pointsPopup) is module `bridge`
// (portsim/sim/bridge.h), called directly. cellAt and enqueueSprite are drawing: without a registered backend they are
// dropped (U-0861).
namespace gl {

bool registered(const char *name) { return registry_find(name) >= 0; }

Ret call_peer(Ctx &ctx, const char *name, std::initializer_list<uint32_t> args) {
  const int i = registry_find(name);
  if (i < 0) throw MissingResource{name};
  const FuncDef *f = registry_at(i, nullptr);
  uint32_t a[16] = {0};
  size_t n = 0;
  for (uint32_t v : args) a[n++] = v;
  Ret r;
  f->fn(ctx, a, r);
  return r;
}

int32_t rnd(Ctx &ctx, int32_t n) { return i32(random_range(rng(ctx), u32(n))); }

// the CRT's __itoa for radix 10 into `buf`; returns buf
uint32_t itoa10(Ctx &ctx, int32_t v, uint32_t buf) {
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
  return buf;
}

// playHabitatSound (audio backend, no owner): a switch over the habitat kind to playSoundAt; any other kind returns it
int32_t play_habitat_sound(Ctx &ctx, int32_t kind, int32_t a, int32_t b) {
  switch (kind) {
    case 0: return portsim::play_sound_at(ctx, 0x3b, a, b, 0);
    case 1: return portsim::play_sound_at(ctx, 0x3a, a, b, 0);
    case 3: return portsim::play_sound_at(ctx, 0xc3, a, b, 0);
    case 4: return portsim::play_sound_at(ctx, 0xc4, a, b, 0);
    case 6: return portsim::play_sound_at(ctx, 0xc5, a, b, 0);
    case 5: return portsim::play_sound_at(ctx, 0xc6, a, b, 0);
    default: return kind;
  }
}

// drawing: no simulation effect
int32_t cell_at(Ctx &ctx, int32_t sprite, int32_t frame, int32_t dir) {
  if (registered("cellAt")) return i32(call_peer(ctx, "cellAt", {u32(sprite), u32(frame), u32(dir)}).eax);
  return 0;
}

}  // namespace gl
