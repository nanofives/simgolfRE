// Animals and habitats (phase 3.6 rows of phase 3.3; included by golfers.cpp). Specifications:
// re/analysis/sim/004017d0_placeAnimal.md, 00401c00_tickAnimals.md, 00405e30_placeHabitat.md, re/analysis/backend/worldToScreen
// (the hook source's comment) and the vector corpus.

void place_animal(Ctx &ctx, int32_t idx) {
  GameState &gs = ctx.gs();
  const Ptr<Animal> a = state_arr(ctx, &GameState::g_animalRecords) + idx;
  const auto type = a.field(&Animal::type);
  const auto kind = a.field(&Animal::kind);
  const auto tx = a.field(&Animal::x), ty = a.field(&Animal::y);
  const auto wait = a.field(&Animal::countdown), st = a.field(&Animal::sprite_base);
  const auto subx = a.field(&Animal::sub_x), suby = a.field(&Animal::sub_y);
  const auto heading = a.field(&Animal::heading).as<uint8_t>();
  const auto steps = a.field(&Animal::walk_steps);
  const auto frame = a.field(&Animal::frame);
  const bool paused = (gs.g_flags & 4u) != 0;
  const int mode = SG_NEGCTL(g_negctl_place_animal);
  const int32_t wx = add32(shl32(tx.get(), 10), subx.get());
  const int32_t wy = add32(shl32(ty.get(), 10), suby.get());
  const int32_t sprite = type.get() + st.get();
  int32_t sx = 0, sy = 0;  // the original's stack slots; read only for a rotation outside 0/2/4/6 (U-0862)
  if (world_to_screen(ctx, wx, wy, sx, sy, 0) != 0) {
    if (!(wait.get() >= 0x80)) {
      int32_t n = 0;
      const Ptr<PlacedObject> objs = state_arr(ctx, &GameState::g_placed_objects);
      for (int32_t i = 0; i < 256; ++i) {
        const Ptr<PlacedObject> o = objs + i;
        if (o.field(&PlacedObject::type).get() == 5 && o.field(&PlacedObject::value).get() > 0 &&
            o.field(&PlacedObject::x).get() == tx.get() && o.field(&PlacedObject::y).get() == ty.get())
          n = o.field(&PlacedObject::value).get() - 1;
      }
      const int32_t dir = (gs.g_cameraRot - heading.get() - 2) & 7;
      const Ptr<int32_t> cols = state_arr(ctx, &GameState::g_sheetCols) + sprite;
      const int8_t fr = static_cast<int8_t>(mod32(frame.get(), cols.get()));  // idiv: a frame count 0 faults
      frame.set(fr);
      const int32_t cell = gl::cell_at(ctx, sprite, fr, dir);
      gs.g_animalCell = cell;                 // drawing state (the sprite queue itself is the renderer's)
      (void)n;
      if (!paused) {
        const int8_t f1 = static_cast<int8_t>(frame.get() + 1);
        frame.set(f1);
        if (!(f1 < cols.get()) && mode != 1) frame.set(0);
      }
      const uint32_t text = text_va(ctx), buf = state_va(ctx, &GameState::g_itoaBuf4e9a70);
      ctx.mem.store<uint8_t>(text, 0);
      vstrcat(ctx.mem, text, gl::itoa10(ctx, sar32(shl32(subx.get(), 1), 10), buf));
      vstrcat(ctx.mem, text, state_va(ctx, &GameState::f_strpool_4c1430));
      vstrcat(ctx.mem, text, gl::itoa10(ctx, sar32(shl32(suby.get(), 1), 10), buf));
    }
  } else if (!paused) {
    if (static_cast<int16_t>(gl::rnd(ctx, 500)) == 0) wait.set(500);
  }
  if (wait.get() != 0) {
    st.set(0x140);
    if (!paused && mode != 2) wait.set(static_cast<int16_t>(wait.get() - 1));
    return;
  }
  if (frame.as<uint8_t>().get() == 0 && st.get() != 0x15a && st.get() != 0x140) {
    st.set(0x140);
    wait.set(static_cast<int16_t>(gl::rnd(ctx, 0x40) + 0x40));
  }
  if (steps.get() == 0) {
    if (st.get() == 0x140 && static_cast<int16_t>(gl::rnd(ctx, 8)) == 0) {
      int32_t nx = sar32(shl32(subx.get(), 1), 10), ny = sar32(shl32(suby.get(), 1), 10);
      const int32_t r = gl::rnd(ctx, 3) & 0xffff;
      const uint8_t hd = static_cast<uint8_t>((heading.get() + static_cast<uint8_t>(r) - 1) & 7);
      heading.set(hd);
      nx = add32(nx, state_arr(ctx, &GameState::f_tab_dirDx_4c2878)[static_cast<int8_t>(hd)]);
      ny = add32(ny, state_arr(ctx, &GameState::f_tab_dirDy_4c2898)[static_cast<int8_t>(hd)]);
      if (nx < 0 || ny < 0 || nx >= 4 || ny >= 4) {
        const int32_t lim = (gs.g_flags & 0x200000u) ? 0x40 : 0x18;
        if (static_cast<int16_t>(gl::rnd(ctx, lim)) == 0) {
          play_sound_at(ctx, type.get() + 0x82, wx, wy, 0);
          st.set(0x14d);
          frame.set(0);
        }
      } else {
        const Ptr<int8_t> map = (state_arr(ctx, &GameState::g_recordBanks) + kind.get()).field(&SgRecordBank::occupancy);
        if (static_cast<uint8_t>(map[add32(mul32(nx, 6), ny)]) == 0xff) steps.set(0x10);
      }
    }
    if (steps.get() == 0) return;
  }
  st.set(0x15a);
  const int8_t h = static_cast<int8_t>(heading.get());
  subx.set(add32(subx.get(), shl32(state_arr(ctx, &GameState::f_tab_dirDx_4c2878)[h], 10) / 32));
  suby.set(add32(suby.get(), shl32(state_arr(ctx, &GameState::f_tab_dirDy_4c2898)[h], 10) / 32));
  const int8_t left = static_cast<int8_t>(steps.get() - 1);
  steps.set(static_cast<uint8_t>(left));
  if (!(left > 0)) st.set(0x140);
}

void tick_animals(Ctx &ctx) {
  const Ptr<Animal> a = state_arr(ctx, &GameState::g_animalRecords);
  const int32_t n = SG_NEGCTL(g_negctl_tick_animals) == 1 ? kAnimals - 1 : kAnimals;
  for (int32_t i = 0; i < n; ++i)
    if ((a + i).field(&Animal::type).as<uint16_t>().get() != 0xffff) place_animal(ctx, i);
}

uint32_t place_habitat(Ctx &ctx, int32_t x, int32_t y, int32_t type) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_place_habitat);
  const int32_t cell = add32(mul32(x, 50), y);
  const Ptr<uint8_t> hi = state_arr(ctx, &GameState::g_tileFlags).as<uint8_t>().byte(1) + mul32(cell, 2);
  hi.set(static_cast<uint8_t>(hi.get() & (mode == 1 ? 0xdf : 0xde)));
  (state_arr(ctx, &GameState::g_tileType) + cell).set(static_cast<int8_t>(type));
  const int32_t x2 = x + i32(random_range(rng(ctx), 3) & 0xffffu) - 1;
  const int32_t y2 = y + i32(random_range(rng(ctx), 3) & 0xffffu) - 1;
  uint32_t r = random_range(rng(ctx), 8);
  if ((r & 0xffffu) != 0) return r;
  r = random_range(rng(ctx), 0x80) & 0xffffu;
  const Ptr<HabitatSlot> slots = state_arr(ctx, &GameState::g_habitatSlots);
  if ((slots + i32(r)).field(&HabitatSlot::kind).get() != 0xff) return r;
  if ((gs.g_preferences & 0x20u) == 0) return r;
  uint32_t k = random_range(rng(ctx), 9) & 0xffffu;
  const int8_t tile = state_arr(ctx, &GameState::g_tileType)[add32(mul32(x2, 50), y2)];
  const Ptr<F_Animal> recs = state_arr(ctx, &GameState::f_tab_animals_4c1998);
  int32_t tries = 1;
  for (;;) {
    const Ptr<F_Animal> rec = recs + i32(k);
    if (static_cast<uint8_t>(tile) == static_cast<uint8_t>(rec.field(&F_Animal::b10).get())) {
      const int32_t mask = rec.field(&F_Animal::b11).get();
      const int32_t bit = shl32(1, static_cast<uint8_t>(gs.g_courseType));
      if ((bit & mask) != 0 && tile_blocked(ctx, x2, y2) == 0) break;
    }
    r = random_range(rng(ctx), 9);
    k = r & 0xffffu;
    if (!(++tries <= 9)) return r;
  }
  const int32_t slot = add_habitat_slot(ctx, x2, y2, i32(k));
  const int32_t py = add32(shl32(y2, 10), 0x200), px = add32(shl32(x2, 10), 0x200);
  play_sound_at(ctx, 0x8f, px, py, 0);
  const uint32_t c = random_range(rng(ctx), 2) & 0xffffu;
  if (slot == -1) return c;
  // the loop's slot never changes and c never changes: it runs until slot + 1 is taken (the first free slot is taken by
  // each addHabitatSlot); a full table with slot + 1 outside it (slot 127) and that byte 0xff never ends
  while ((slots + slot + 1).field(&HabitatSlot::kind).get() == 0xff && i32(c) < 3) {
    const int32_t a = i32(random_range(rng(ctx), 3) & 0xffffu);
    const int32_t b = i32(random_range(rng(ctx), 3) & 0xffffu);
    const int32_t t = add_habitat_slot(ctx, a + x2 - 1, b + y2 - 1, i32(k));
    (slots + t).field(&HabitatSlot::link).set(static_cast<int16_t>(slot));
    if (t == -1 && (slots + slot + 1).field(&HabitatSlot::kind).get() == 0xff) throw NoReturn{"placeHabitat"};
  }
  const uint32_t text = text_va(ctx);
  vstrcpy(ctx.mem, text, (recs + i32(k)).va());
  vstrcat(ctx.mem, text, state_va(ctx, &GameState::f_strpool_4c3f74) + 0x28);
  queue_message(ctx, px, py, i32(0x800003ffu));
  return u32(gl::play_habitat_sound(ctx, i32(k), px, py));
}
