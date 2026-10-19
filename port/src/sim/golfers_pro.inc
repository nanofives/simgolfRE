// Tournament fields and the golfer-type reset (included by golfers.cpp). Specifications:
// re/analysis/golfer/0046c970_loadGolfPro.md, 0046d0c0_resetGolferState.md, the hook sources (reading reference), vectors.
// Peers: loadThemeFile (3.1b; it reads the pro's file through GameFiles), clearObjectFootprint (3.1a), updateMembership (3.2,
// through initGolferState).

int32_t load_golf_pro(Ctx &ctx) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_load_golf_pro);
  uint8_t taken[100] = {0};
  rep_movs(ctx.mem, state_va(ctx, &GameState::g_golferTypesBackup), state_va(ctx, &GameState::g_golferTypes),
           sizeof(GameState::g_golferTypes));
  rep_movs(ctx.mem, state_va(ctx, &GameState::g_golferPhrasesBackup), state_va(ctx, &GameState::g_golferPhrases),
           sizeof(GameState::g_golferPhrases));
  for (int32_t g = 0; g < 152; ++g) {
    golfer(ctx, g).field(&Golfer::hole).set(0);
    golfer(ctx, g).field(&Golfer::partner).set(-1);
  }
  gs.g_visitor60Count = static_cast<int32_t>(u32(gs.g_visitor60Count) & 0xffffff7fu);  // the low byte's bit 7
  gs.g_nextArrivalOrder = 100;
  const uint32_t text = text_va(ctx);
  const Ptr<GolfPro> pros = state_arr(ctx, &GameState::g_golfPros);
  const Ptr<GolferType> types = state_arr(ctx, &GameState::g_golferTypes);
  const Ptr<HoleRecord> holes = state_arr(ctx, &GameState::g_holes);
  const Ptr<uint16_t> mask_word = state_obj(ctx, &GameState::g_skillMaskBlock).as<uint8_t>().byte(8).as<uint16_t>();
  int32_t hole = 1;
  for (int32_t outer = 0; outer < 0x24; outer += 2, ++hole) {
    const Ptr<HoleRecord> hp = holes + hole;
    if (static_cast<uint8_t>(hp.field(&HoleRecord::par).get()) == 0) continue;
    for (int32_t inner = 0; inner < 2; ++inner) {
      const int32_t g = outer + inner;
      const Ptr<Golfer> r = golfer(ctx, g);
      for (uint32_t k = 0; k < sizeof(Golfer); k += 4) ctx.mem.store<uint32_t>(r.va() + k, 0);
      const Ptr<PlacedObject> o0 = state_arr(ctx, &GameState::g_placed_objects);
      r.field(&Golfer::x).set(add32(shl32(o0.field(&PlacedObject::x).get(), 10), 0x600));
      r.field(&Golfer::y).set(add32(shl32(o0.field(&PlacedObject::y).get(), 10), 0x600));
      r.field(&Golfer::hole).set(static_cast<int8_t>(hole));
      r.field(&Golfer::facing).set(hp.field(&HoleRecord::direction).get());
      r.field(&Golfer::traits).set(static_cast<uint16_t>(gl::rnd(ctx, 0x7fff)));
      const int32_t base = sub32(hole, gs.g_holesPlusOne);
      r.field(&Golfer::cls).set(2);
      r.field(&Golfer::prefs).set(7);
      r.field(&Golfer::type).set(static_cast<int16_t>(g != 1 ? 0x4c : 0));
      r.field(&Golfer::wait).set(static_cast<int16_t>(shl32(add32(inner, mul32(2, base)), 2)));
      r.field(&Golfer::story_id).set(-1);
      {
        const int32_t rr = gl::rnd(ctx, 0x80) & 0xffff;
        const int16_t type = r.field(&Golfer::type).get();
        const int8_t demo = static_cast<int8_t>((types + type).field(&GolferType::demographics).get());
        r.field(&Golfer::traits).set(static_cast<uint16_t>(demo + (rr << 8)));
      }
      r.field(&Golfer::date_stamp).set(gs.g_date);
      r.field(&Golfer::state).set(0xb);
      r.field(&Golfer::mood).set(static_cast<int16_t>(gl::rnd(ctx, 4) + 3));
      const int32_t seq = gs.g_nextArrivalOrder;
      r.field(&Golfer::order).set(static_cast<int16_t>(seq));
      gs.g_nextArrivalOrder = sub32(seq, 1);
      if (gs.g_difficulty == 0) r.field(&Golfer::mood).set(5);
      const int32_t act = (state_arr(ctx, &GameState::g_activeObjectLevel) + 9).get();
      if (act != 0) r.field(&Golfer::mood).set(static_cast<int16_t>(add32(act, 4)));
      r.field(&Golfer::partner).set(static_cast<int16_t>(g ^ 1));

      // the pro draw: a rejection loop over the 100 pro records
      bool eligible = false;
      for (int32_t k = 1; k < 100 && !eligible; ++k) eligible = (pros + k).field(&GolfPro::look_hi).get() != -1;
      if (!eligible) throw NoReturn{"loadGolfPro"};  // every draw is rejected before the try count moves
      int32_t tries = 0, pick, v;
      for (;;) {
        pick = gl::rnd(ctx, 0x64) & 0xffff;
        int32_t level = gs.g_difficulty;
        const int32_t skill = (pros + pick).field(&GolfPro::skill_sum).get();
        v = div32(mul32(skill - 10, skill - 10), mul32(add32(mul32(level, 5), 0x14), 2));
        if (g == 0) {
          const int32_t hi = static_cast<int32_t>((static_cast<int64_t>(static_cast<int32_t>(0xd5555555u)) *
                                                   static_cast<int64_t>(mul32(level, v))) >> 32);
          v = add32(v, add32(hi, i32(u32(hi) >> 31)));
        }
        if (gs.g_flags & 0x4000000u) {
          v = gl::rnd(ctx, v) & 0xffff;
          level = gs.g_difficulty;
        }
        if (static_cast<uint8_t>((pros + pick).field(&GolfPro::look_hi).get()) == 0xff) continue;
        if (pick == 0) continue;
        ++tries;
        if (tries < 0x320 && taken[pick] != 0) continue;
        const int32_t cash = gs.g_cash_div100 / 200;
        const int32_t rep = gs.g_tournamentPurse / 10;
        if (v > add32(add32(div32(tries, sub32(4, level)), rep), cash)) continue;  // 4 - level == 0 faults (model_defects)
        const int32_t slack = g != 0 ? tries : tries / 4;
        if (v < add32(sub32(rep, slack), cash)) continue;
        break;
      }
      if (taken[pick] != 0)
        pick = 0;
      else
        taken[pick] = 1;

      r.field(&Golfer::kind).set(static_cast<uint8_t>(g != 1 ? 0x20 : 0x24));
      if (g != 1) r.field(&Golfer::name_rec).set(static_cast<uint8_t>(pick));
      r.field(&Golfer::flags).set(r.field(&Golfer::flags).get() & 0xffffffe3u);
      r.field(&Golfer::cls).set(static_cast<int8_t>(g != 1 ? clamp(gs.g_difficulty, 0, 2) : 2));
      if (g == 1) {  // the player
        uint16_t mask = 0;
        r.field(&Golfer::prefs).set(0x47);
        mask_word.set(0);
        for (int32_t i = 0; i < 12; ++i) {
          const uint8_t b = static_cast<uint8_t>(state_arr(ctx, &GameState::g_playerSkills)[i]);
          (r.field(&Golfer::skill) + i).set(b);
          if (b != 0) mask = static_cast<uint16_t>(mask | (1u << i));
        }
        const uint32_t f = gs.g_flags;
        mask_word.set(mask);
        r.field(&Golfer::skill_mask).set(static_cast<int16_t>(mask));
        r.field(&Golfer::flags).set(r.field(&Golfer::flags).get() | 0x200u);
        if ((f & 0x4000000u) == 0) {
          const Ptr<Walker> w0 = state_arr(ctx, &GameState::g_walkers);
          r.field(&Golfer::x).set(w0.field(&Walker::x).get());
          r.field(&Golfer::y).set(w0.field(&Walker::y).get());
          r.field(&Golfer::facing).set(w0.field(&Walker::facing).get());
        }
        r.field(&Golfer::wait).set(0);
        continue;
      }
      const int32_t t = g != 0 ? g : 1;
      const Ptr<GolfPro> pro = pros + pick;
      vstrcpy(ctx.mem, text, pro.va());
      vstrcat(ctx.mem, text, state_va(ctx, &GameState::f_strpool_4c581c) + 0x12c);
      const int32_t loaded = i32(gl::call_peer(ctx, "loadThemeFile", {text, u32(t), 0xffffffffu}).eax);
      const Ptr<GolferType> T = types + t;
      if (loaded == 0) {
        const uint32_t ph = state_va(ctx, &GameState::g_golferPhrases) + u32(t) * u32(sizeof(GolferTypePhrases));
        for (uint32_t k = 0; k < sizeof(GolferTypePhrases); ++k) ctx.mem.store<uint8_t>(ph + k, 0);
        const uint8_t b0 = static_cast<uint8_t>(pro.field(&GolfPro::look_hi).get());
        T.field(&GolferType::look_25).set(pro.field(&GolfPro::look_25).get());
        T.field(&GolferType::look_24).set(pro.field(&GolfPro::look_24).get());
        T.field(&GolferType::look_27).set(pro.field(&GolfPro::look_27).get());
        T.field(&GolferType::look_23).set(
            static_cast<uint8_t>(static_cast<uint8_t>(pro.field(&GolfPro::look_lo).get()) | ((b0 & 3) << 4)));
        T.field(&GolferType::demographics).set(9);
        if (b0 & 4) T.field(&GolferType::demographics).set(0x89);
        const uint32_t c = random_range(rng(ctx), 5) & 0xffffu;
        T.field(&GolferType::personality).set(static_cast<uint8_t>(1u << (c & 0x1f)));
        const int32_t r4 = gl::rnd(ctx, 0x80) & 0xffff;
        r.field(&Golfer::traits).set(
            static_cast<uint16_t>((r4 << 8) + static_cast<int8_t>(T.field(&GolferType::demographics).get())));
        vstrcpy(ctx.mem, T.field(&GolferType::name).va(), pro.va());
        vstrcpy(ctx.mem, T.va(), state_va(ctx, &GameState::f_strpool_4c581c) + 0x120);
        // the pro's 12 skill bytes (+0x25) as three dwords into the golfer's skills
        for (int32_t k = 0; k < 3; ++k)
          ctx.mem.store<uint32_t>(r.field(&Golfer::skill).va() + u32(k) * 4, ctx.mem.load<uint32_t>(pro.field(&GolfPro::skill).va() + u32(k) * 4));
      } else {
        r.field(&Golfer::traits).set(static_cast<uint16_t>(static_cast<int8_t>(T.field(&GolferType::demographics).get())));
      }
      r.field(&Golfer::type).set(static_cast<int16_t>(t));
      r.field(&Golfer::prefs).set(7);
      r.field(&Golfer::skill_mask).set(0xe0);
      for (int32_t i = 0; i < 12; ++i)
        if ((r.field(&Golfer::skill) + i).get() != 0)
          r.field(&Golfer::skill_mask).set(static_cast<int16_t>(static_cast<uint16_t>(r.field(&Golfer::skill_mask).get()) | (1u << i)));
    }
  }
  const uint32_t f = gs.g_flags | (mode == 1 ? 0x100000u : 0x200000u);
  gs.g_nextArrivalOrder = 100;
  gs.g_tournamentPrep = -1;
  gs.g_flags = f;
  return i32(f);
}

void reset_golfer_state(Ctx &ctx) {
  GameState &gs = ctx.gs();
  const Ptr<PlacedObject> objs = state_arr(ctx, &GameState::g_placed_objects);
  for (int32_t i = 0; i < 256; ++i) {
    const uint16_t type = (objs + i).field(&PlacedObject::type).as<uint16_t>().get();
    if (type == 0x11 || type == 0x12) gl::call_peer(ctx, "clearObjectFootprint", {u32(i)});
  }
  const int32_t rec = gs.g_holesPlusOne;
  rep_movs(ctx.mem, state_va(ctx, &GameState::g_golferTypes), state_va(ctx, &GameState::g_golferTypesBackup),
           sizeof(GameState::g_golferTypes));
  rep_movs(ctx.mem, state_va(ctx, &GameState::g_golferPhrases), state_va(ctx, &GameState::g_golferPhrasesBackup),
           sizeof(GameState::g_golferPhrases));
  const int32_t mult = (state_arr(ctx, &GameState::g_objectTypeLevel) + 10).get();
  // the 18-iteration loop works on ONE course record (g_holesPlusOne's), as in the original
  const Ptr<HoleRecord> h = state_arr(ctx, &GameState::g_holes) + rec;
  const Ptr<uint8_t> par = h.field(&HoleRecord::par).as<uint8_t>();
  const int32_t top = SG_NEGCTL(g_negctl_reset_golfer_state) == 1 ? 0x270 : 0x271;
  for (int32_t n = 0x12; n != 0; --n) {
    if (par.get() == 0) continue;
    par.set(3);
    const uint8_t f = h.field(&HoleRecord::flags).as<uint8_t>().get();
    int32_t v = h.field(&HoleRecord::length).get();
    if ((f & 0x60) != 0 && v > 0xfa) v += 0x19;
    if (v > 0x12c) v = sub32(v, mul32(25, mult));
    for (int pass = 0; pass < 2; ++pass) {  // the same four thresholds twice, as in the original
      if (v <= 0x32) par.set(2);
      if (v >= 0xfa) par.set(4);
      if (v >= 0x1db) par.set(5);
      if (v > top) par.set(6);
    }
  }
  gs.g_tournamentPrep = 0;
  init_golfer_state(ctx);
}

// composeGolferSprite: the layered sprite is drawing (the compositor objects are the renderer's); its simulation effect is
// the write-back of the chosen look indices into the golfer's type record (g < 0x9c). Returns 0: the original returns the
// last compositor call's eax, which is not simulation state (U-0861).
int32_t compose_golfer_sprite(Ctx &ctx, int32_t g0) {
  int32_t g = g0;
  if (!(g < 0x9c)) return 0;  // celebrity records (g >= 0x9c) only choose layers
  const int32_t f2 = type_bit7_clear(ctx, g);
  if (g == 0xff) g = 0;
  const Ptr<Golfer> rec = golfer(ctx, g);
  const int32_t t = rec.field(&Golfer::type).get();
  int32_t D = t % 10;
  int32_t A = mul32(t, 3) % 10;
  const Ptr<GolferType> T = state_arr(ctx, &GameState::g_golferTypes) + t;
  const int32_t app = T.field(&GolferType::body).get();
  int32_t B;
  if (app < 0x13) {
    const uint32_t tab = f2 != 0 ? state_va(ctx, &GameState::f_tab_quotes_4d55e8) : state_va(ctx, &GameState::f_tab_quotes_4d5b38);
    B = ctx.mem.load<int8_t>(tab + u32(mul32(68, app)));
  } else {
    B = t % 4;
  }
  const uint8_t fl = rec.field(&Golfer::prefs).get();
  int32_t ebx = A;
  if (!(fl & 4)) ebx = f2 != 0 ? 0 : 9;
  int32_t C = B;
  if (fl & 2) C = 4;
  if (f2 == 0) A = mul32(2, t) % 3 + 1;
  const int32_t off2 = type_bit7_clear(ctx, g) != 0 ? 0 : 0x14;
  const int32_t idx2 = add32(T.field(&GolferType::body).get(), off2);
  int32_t Ev = ctx.mem.load<int8_t>(state_va(ctx, &GameState::f_tab_quotes_4d55e8) + 3 + u32(mul32(68, idx2)));
  if (T.field(&GolferType::custom).get() != 0) {
    A = T.field(&GolferType::look_24).get();
    D = T.field(&GolferType::look_25).get();
    B = T.field(&GolferType::look_27).get();
    ebx = T.field(&GolferType::look_23).get() & 0xf;
    C = T.field(&GolferType::look_26).get();
    Ev = T.field(&GolferType::look_28).get();
  }
  if (rec.field(&Golfer::kind).get() == 0x20) {
    C = 4;
    const Ptr<GolfPro> pro = state_arr(ctx, &GameState::g_golfPros) + rec.field(&Golfer::name_rec).get();
    ebx = pro.field(&GolfPro::look_lo).get();
    B = pro.field(&GolfPro::look_27).get();
    A = pro.field(&GolfPro::look_24).get();
    D = pro.field(&GolfPro::look_25).get();
  }
  const auto l23 = T.field(&GolferType::look_23);
  const int mode = SG_NEGCTL(g_negctl_compose_golfer_sprite);
  l23.set(static_cast<uint8_t>((l23.get() & 0xf0) | static_cast<uint8_t>(ebx)));
  T.field(&GolferType::look_24).set(static_cast<int8_t>(mode == 1 ? D : A));
  T.field(&GolferType::look_25).set(static_cast<int8_t>(mode == 1 ? A : D));
  T.field(&GolferType::look_27).set(static_cast<int8_t>(B));
  T.field(&GolferType::look_26).set(static_cast<int8_t>(C));
  T.field(&GolferType::look_28).set(static_cast<int8_t>(Ev));
  return 0;
}
