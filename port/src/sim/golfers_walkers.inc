// Walkers (staff and the player character): updateWalkers (phase 3.6 rows of phase 3.3; included by golfers.cpp).
// Specification: re/analysis/sim/00402a40_updateWalkers.md, the hook source's per-instruction comments (reading reference),
// log/c3/c3eb_purpose.md and the vector corpus. Peers: pathStep / freeAtTile (3.1a), thoughtFlag / emitGolferThought (3.5).

namespace {
int32_t thought_flag_peer(Ctx &c, int32_t g) { return i32(gl::call_peer(c, "thoughtFlag", {u32(g)}).eax); }
void emit_thought_peer(Ctx &c, int32_t g, int32_t code, int32_t flag) {
  gl::call_peer(c, "emitGolferThought", {u32(g), u32(code), u32(flag)});
}
int32_t path_step_peer(Ctx &c, int32_t a, int32_t b, int32_t x, int32_t y, int32_t i) {
  return i32(gl::call_peer(c, "pathStep", {u32(a), u32(b), u32(x), u32(y), u32(i)}).eax);
}
void free_at_tile_peer(Ctx &c, int32_t x, int32_t y) { gl::call_peer(c, "freeAtTile", {u32(x), u32(y)}); }
// the octant of an angleFixed result: (((a >> 28) & 0xf) + 1) >> 1 & 7
uint8_t octant(int32_t a) { return static_cast<uint8_t>((((sar32(a, 28) & 0xf) + 1) >> 1) & 7); }
}  // namespace

int32_t update_walkers(Ctx &ctx) {
  GameState &gs = ctx.gs();
  const int mode_n = SG_NEGCTL(g_negctl_update_walkers);
  const Ptr<int32_t> dx8 = state_arr(ctx, &GameState::f_tab_dirDx_4c2878);
  const Ptr<int32_t> dy8 = state_arr(ctx, &GameState::f_tab_dirDy_4c2898);
  const Ptr<uint16_t> tflags = state_arr(ctx, &GameState::g_tileFlags);
  const Ptr<int8_t> ttype = state_arr(ctx, &GameState::g_tileType);
  const auto rnd = [&](int32_t n) { return gl::rnd(ctx, n); };
  // two stack slots keep their value from one record to the next (the flagged-walker arm reads them)
  int32_t s30 = 0, s28 = 0;
  for (int32_t i = 0; i < 0x40;) {
    const Ptr<Walker> w = state_arr(ctx, &GameState::g_walkers) + i;
    const int32_t cur = i;
    ++i;
    const auto W_x = w.field(&Walker::x), W_y = w.field(&Walker::y);
    const auto W_flags = w.field(&Walker::flags);
    const auto W_kind = w.field(&Walker::kind);
    const auto W_action = w.field(&Walker::action).as<uint8_t>();
    const auto W_timer = w.field(&Walker::action_timer);
    const auto W_facing = w.field(&Walker::facing);
    const auto W_steps = w.field(&Walker::steps_left);
    const auto W_wait = w.field(&Walker::wait);
    const auto W_frame = w.field(&Walker::anim_frame);
    const auto W_seq = w.field(&Walker::anim_seq);
    const auto W_target = w.field(&Walker::target_golfer);
    const auto W_services = w.field(&Walker::services);
    const auto W_postx = w.field(&Walker::post_x), W_posty = w.field(&Walker::post_y);
    const auto G_ = [&](int32_t g) { return golfer(ctx, g); };

    int8_t mode = W_kind.get();
    int32_t tx = 0, ty = 0;
    if (mode >= 0 && gs.g_playerGolfer == -1) W_flags.set(0);
    if (W_flags.get() == 0) continue;
    // ---- the action timer (every 4 dates)
    if (W_timer.get() != 0 && (u32(gs.g_date) & 3u) == 0) {
      const uint8_t t = static_cast<uint8_t>(W_timer.get() - 1);
      W_timer.set(t);
      if (t == 4 && W_action.get() == 0xfe) {
        const int32_t who = W_target.get();
        bool close_by = false;
        if ((W_flags.get() & 8) == 0) {
          const Ptr<Golfer> g = G_(who);
          if (!(g.field(&Golfer::meter_a8).get() > 0x10) && !(g.field(&Golfer::meter_a6).get() > 0x10) &&
              !(g.field(&Golfer::meter_aa).get() > 0xa0))
            close_by = true;
        } else {
          close_by = true;
        }
        if (close_by) {
          const int32_t f = thought_flag_peer(ctx, W_target.get());
          emit_thought_peer(ctx, W_target.get(), 0x22, f);
        } else {
          const int32_t f = thought_flag_peer(ctx, who);
          emit_thought_peer(ctx, W_target.get(), 0x3a, f);
        }
      }
      if (W_timer.get() == 4 && W_action.get() == 0xfb) {
        if ((W_flags.get() & 8) != 0) G_(W_target.get()).field(&Golfer::meter_a8).set(0x63);
        emit_thought_peer(ctx, W_target.get(), 0x19, 0x14);
        const int32_t y = W_y.get();
        gs.g_cash_div100 = add32(gs.g_cash_div100, mode_n == 1 ? 3 : 2);
        const int32_t x = W_x.get();
        G_(W_target.get()).field(&Golfer::meter_a8).set(0);
        points_popup(ctx, 2, x, y, -1);
        const int32_t rem = gs.g_year % 100;
        const Ptr<int16_t> food = (state_arr(ctx, &GameState::g_financeYears) + rem).field(&FinanceYear::food_drink);
        food.set(static_cast<int16_t>(food.get() + 2));
      }
    }
    // ---- wait
    if (W_wait.get() != 0) {
      W_wait.set(static_cast<int16_t>(W_wait.get() - 1));
      continue;
    }
    mode = W_kind.get();
    if (mode > 0) {
      const Ptr<Golfer> sel = G_(gs.g_playerGolfer);
      int32_t gx = sel.field(&Golfer::ball_x).get(), gy = sel.field(&Golfer::ball_y).get();
      if (gx == 0) {
        gx = sel.field(&Golfer::x).get();
        gy = sel.field(&Golfer::y).get();
      } else {
        gx = add32(gx, shl32(dx8[cur & 7], 10));
        gy = add32(gy, shl32(dy8[cur & 7], 10));
      }
      tx = gx;
      ty = gy;
      const int32_t ex = sub32(gx, W_x.get()), ey = sub32(gy, W_y.get());
      s30 = ex;
      s28 = ey;
      const int32_t d = approx_distance(ex, ey);
      if (d >= 0x800 && !(sel.field(&Golfer::wait).get() > 0) &&
          (static_cast<uint8_t>(sel.field(&Golfer::strokes).get()) != 0 || static_cast<uint8_t>(sel.field(&Golfer::hole).get()) != 1)) {
        // to the move
      } else {
        W_facing.set(static_cast<int8_t>(octant(angle_fixed(ex, s28))));
        W_seq.set(0xb);
        W_wait.set(static_cast<int16_t>(rnd(0x10) + 1));
        W_steps.set(0);
        continue;
      }
    } else {
      // ---- mode <= 0
      if (static_cast<uint8_t>(mode) == 0xfa) {
        if (gs.g_playerGolfer != -1) continue;
        if ((gs.g_flags & 0x200000u) != 0 && static_cast<uint8_t>(G_(1).field(&Golfer::hole).get()) != 0) continue;
      }
      const uint8_t flags = W_flags.get();
      if (W_steps.get() != 0 || (flags & 0x10) != 0) {
        // to the move
      } else {
        const int32_t x = W_x.get(), y = W_y.get();
        int32_t best = 0xfffff;
        s30 = best;
        int32_t best_idx = -1;
        tx = x;
        ty = y;
        if (static_cast<uint8_t>(W_postx.get()) != 0xff) {
          tx = add32(shl32(W_postx.get(), 10), 0x200);
          ty = add32(shl32(W_posty.get(), 10), 0x200);
        }
        int32_t kind = -1, sel_idx = -1, sel_gy = 0;
        if (static_cast<uint8_t>(mode) == 0xfc || static_cast<uint8_t>(mode) == 0xfa) {
          int32_t lim = (flags & 8) ? 0x20 : 0x18;
          const int32_t px = sar32(x, 10), py = sar32(y, 10);
          for (int32_t a = -16; a <= 16; ++a) {
            for (int32_t b = -16; b <= 16; ++b) {
              const int32_t gx = W_postx.get() + a, gy = W_posty.get() + b;
              if (tile_blocked(ctx, gx, gy) != 0) continue;
              if (((tflags + (gy + gx * 50)).get() & 0x800) == 0) continue;
              const int32_t dd = approx_distance(sub32(sub32(gx * 2, W_postx.get()), px), sub32(sub32(gy * 2, W_posty.get()), py));
              if (dd >= lim) continue;
              lim = dd;
              sel_idx = gx;
              tx = add32(shl32(gx, 10), 0x200);
              sel_gy = gy;
              ty = add32(shl32(gy, 10), 0x200);
              best_idx = 1;
              kind = -4;
            }
          }
          s28 = 17;
          if (best_idx == -1 && (flags & 8) != 0) {
            int32_t lim2 = 0x1400;
            const Ptr<BallRecord> balls = state_arr(ctx, &GameState::g_ballRecords);
            for (int32_t e = 0; e < 256; ++e) {
              const Ptr<BallRecord> p = balls + e;
              if (p.field(&BallRecord::x).get() == -1) continue;
              const int32_t qy = p.field(&BallRecord::start_y).get();
              const int32_t wx = W_x.get();
              // the original subtracts the walker's x in the y expression too (model_defects#16)
              const int32_t a2 = sub32(sub32(sub32(add32(qy, qy), 0x200), shl32(W_posty.get(), 10)), wx);
              const int32_t qx = p.field(&BallRecord::start_x).get();
              const int32_t a1 = sub32(sub32(sub32(add32(qx, qx), 0x200), shl32(W_postx.get(), 10)), wx);
              const int32_t dd = approx_distance(a1, a2);
              if (dd >= lim2) continue;
              lim2 = dd;
              ty = p.field(&BallRecord::start_y).get();
              tx = p.field(&BallRecord::start_x).get();
              sel_idx = e;
              best_idx = 2;
              kind = -4;
            }
          }
        }
        const uint8_t m2 = static_cast<uint8_t>(W_kind.get());
        bool search = true;
        if (m2 == 0xfc || m2 == 0xfa) {
          if (sel_idx != -1)
            search = false;
          else if (m2 != 0xfa)
            search = false;
        }
        if (search) {
          tx = add32(shl32(W_postx.get(), 10), 0x200);
          ty = add32(shl32(W_posty.get(), 10), 0x200);
          int32_t gi = 0;
          s28 = 0;
          s30 = best;
          best = 0xfffff;
          for (; gi < 152; ++gi) {
            s28 = gi;
            const Ptr<Golfer> g = G_(gi);
            const int32_t gx = g.field(&Golfer::x).get(), gy = g.field(&Golfer::y).get();
            int32_t dd = tile_distance(gx, gy, W_postx.get(), W_posty.get());
            int32_t kd = -1;
            if (!(g.field(&Golfer::hole).get() <= 0) && !(g.field(&Golfer::state).get() < 7) && gi != W_target.get() &&
                type_at_pos(ctx, gx, gy) != 0 && type_at_pos(ctx, gx, gy) != 0x16) {
              const uint8_t cl = static_cast<uint8_t>(W_kind.get());
              if (cl == 0xfd || cl == 0xfa) {
                const uint32_t gflags = g.field(&Golfer::flags).get();
                if ((gflags & 0x20000000u) != 0 && (W_flags.get() & 8) != 0) {
                  dd = dd / 8;
                  kd = -3;
                } else if ((gflags & 0x4000u) == 0 && !(g.field(&Golfer::strokes).get() > 1)) {
                  kd = -3;
                }
              }
              const uint8_t th = g.field(&Golfer::thought_kind).get();
              if ((cl == 0xfe || cl == 0xfa) && th != 0x22 && th != 0x3a) kd = -2;
              if (cl == 0xfb || cl == 0xfa) {
                const int32_t lowv = ((~W_flags.get()) & 8) >> 1;
                if (g.field(&Golfer::meter_a8).get() > lowv) kd = -5;
              }
              if ((kd == static_cast<int8_t>(cl) || cl == 0xfa) && dd < best) {
                best = dd;
                best_idx = gi;
                kind = kd;
                s30 = dd;
              }
            }
          }
          s28 = gi;
          if (best < 0x64) {
            tx = G_(best_idx).field(&Golfer::x).get();
            ty = G_(best_idx).field(&Golfer::y).get();
          } else {
            if (best_idx != -1) {
              const int32_t dyv = sub32(G_(best_idx).field(&Golfer::y).get(), W_y.get());
              const int32_t dxv = sub32(G_(best_idx).field(&Golfer::x).get(), W_x.get());
              W_facing.set(static_cast<int8_t>(direction8(dxv, dyv)));
            }
            best_idx = -1;
          }
        }
        if (static_cast<uint8_t>(W_kind.get()) == 0xfa && (state_arr(ctx, &GameState::g_holes) + 1).field(&HoleRecord::par).get() == 0) {
          if ((u32(gs.g_date) & 0x200u) == 0 &&
              static_cast<uint8_t>(ttype[add32(gs.g_cursorTileY, mul32(gs.g_cursorTileX, 50))]) != 0x11) {
            tx = add32(shl32(gs.g_cursorTileX, 10), 0x200);
            ty = add32(shl32(gs.g_cursorTileY, 10), 0x200);
          } else {
            tx = W_x.get();
            ty = W_y.get();
          }
        }
        const int32_t ey = sub32(ty, W_y.get()), ex = sub32(tx, W_x.get());
        s30 = ex;
        s28 = ey;
        if (approx_distance(ex, ey) > 0x400) {
          // to the move
        } else {
          W_seq.set(0xb);
          W_wait.set(static_cast<int16_t>(rnd(0x10) + 1));
          W_steps.set(static_cast<int16_t>(approx_distance(ex, ey) > 0x800 ? 0x200 : 0));
          int32_t snd = 0;
          if ((W_flags.get() & 8) != 0) snd = 4;
          if (static_cast<uint8_t>(W_kind.get()) == 0xfa && kind != -2) snd = 8;
          if (best_idx == -1) {
            W_wait.set(static_cast<int16_t>(rnd(8)));
            continue;
          }
          W_facing.set(static_cast<int8_t>(octant(angle_fixed(ex, ey))));
          W_wait.set(static_cast<int16_t>(W_wait.get() + 0x40));
          W_timer.set(7);
          W_target.set(static_cast<int16_t>(best_idx));
          const uint32_t sel = u32(kind + 5);
          bool cnt = false;
          if (sel <= 3) {
            const Ptr<Golfer> b = G_(best_idx);
            switch (sel) {
              case 3:  // kind -2
                W_action.set(0xfe);
                W_wait.set(static_cast<int16_t>(W_wait.get() + 0x40));
                play_sound_at(ctx, snd + 0x5c, W_x.get(), W_y.get(), 0);
                W_seq.set(0xc);
                W_frame.set(0);
                b.field(&Golfer::wait).set(static_cast<int16_t>(b.field(&Golfer::wait).get() - 0x10));
                b.field(&Golfer::facing).set(static_cast<int8_t>(static_cast<uint8_t>(W_facing.get()) ^ 4));
                b.field(&Golfer::state).set(0xb);
                cnt = true;
                break;
              case 2: {  // kind -3
                const uint32_t gflags = b.field(&Golfer::flags).get();
                if ((gflags & 0x20000000u) != 0 && (W_flags.get() & 8) != 0) {
                  b.field(&Golfer::mood).set(static_cast<int16_t>(0xfff5));
                  W_action.set(0xf9);
                  b.field(&Golfer::wait).set(0);
                } else {
                  W_action.set(0xfd);
                  b.field(&Golfer::flags).set(gflags | 0x4000u);
                }
                W_seq.set(0xc);
                W_frame.set(0);
                play_sound_at(ctx, snd + 0x5d, W_x.get(), W_y.get(), 500);
                cnt = true;
                break;
              }
              case 0:  // kind -5
                W_action.set(0xfb);
                W_seq.set(0xc);
                W_frame.set(0);
                play_sound_at(ctx, snd + 0x5f, W_x.get(), W_y.get(), 0);
                W_wait.set(static_cast<int16_t>(W_wait.get() - 0x20));
                b.field(&Golfer::wait).set(static_cast<int16_t>(0xffe0));
                b.field(&Golfer::state).set(0xb);
                b.field(&Golfer::facing).set(static_cast<int8_t>(static_cast<uint8_t>(W_facing.get()) ^ 4));
                cnt = true;
                break;
              case 1:  // kind -4
                if (best_idx == 2) {
                  W_action.set(0xfa);
                  W_seq.set(0xc);
                  W_wait.set(0xc);
                  (state_arr(ctx, &GameState::g_ballRecords) + sel_idx).field(&BallRecord::x).set(-1);
                } else {
                  W_action.set(0xfc);
                  W_seq.set(0xc);
                  W_wait.set(static_cast<int16_t>(((~W_flags.get()) & 8) >> 3));
                  const int32_t fl = add32(sel_gy, mul32(sel_idx, 50));
                  const Ptr<uint16_t> fwp = tflags + fl;
                  uint16_t fw = fwp.get();
                  const Ptr<uint8_t> anim = state_arr(ctx, &GameState::tile_anim_frame) + fl;
                  if ((fw & 0x4000) == 0) {
                    fw = static_cast<uint16_t>(fw | 0x4000);
                    fwp.set(fw);
                    const uint8_t base = static_cast<uint8_t>((state_arr(ctx, &GameState::g_sheetCols) + 400).get());
                    anim.set(static_cast<uint8_t>(base - 3));
                    play_sound_at(ctx, snd + 0x5e, W_x.get(), W_y.get(), 0);
                    free_at_tile_peer(ctx, sel_idx, sel_gy);
                  } else {
                    const uint8_t c = static_cast<uint8_t>(anim.get() - 1);
                    anim.set(c);
                    if (c <= 1) {
                      fw = static_cast<uint16_t>(fw & 0xb7ff);
                      W_wait.set(8);
                      fwp.set(fw);
                      cnt = true;
                    }
                  }
                }
                break;
            }
          }
          if (cnt) W_services.set(static_cast<int16_t>(W_services.get() + 1));
          if (W_wait.get() > 1 && (W_flags.get() & 8) != 0) W_wait.set(static_cast<int16_t>(W_wait.get() / 2));
          continue;
        }
      }
    }
    // ---- the move toward (tx, ty)
    if ((W_flags.get() & 0x10) != 0) {
      const Ptr<HoleRecord> exit = state_arr(ctx, &GameState::g_holes) + 19;
      tx = add32(shl32(exit.field(&HoleRecord::tee_x).get(), 10), 0x200);
      ty = add32(shl32(exit.field(&HoleRecord::tee_y).get(), 10), 0x200);
      if (mod32(add32(gs.g_date, mul32(cur, 0x23)), 100) == 0) {
        W_timer.set(7);
        W_action.set(0x23);
      }
      if (approx_distance(sub32(tx, W_x.get()), sub32(ty, W_y.get())) < 0x600) W_flags.set(0);
    }
    if (W_steps.get() == 0) {
      gs.g_flags |= 0x100u;
      const int32_t r = path_step_peer(ctx, tx, ty, W_x.get(), W_y.get(), cur);
      gs.g_flags &= 0xfffffeffu;
      const uint8_t al = static_cast<uint8_t>(r);
      W_facing.set(static_cast<int8_t>(al));
      if (al != 0xff) {
        const int32_t x6 = sar32(W_x.get(), 6) & 0xf, y6 = sar32(W_y.get(), 6) & 0xf;
        const int32_t di = static_cast<int8_t>(al);
        int32_t sv;
        if (dx8[di] == 0)
          sv = dy8[di] == 1 ? 0x18 - y6 : y6 + 8;
        else if (dx8[di] == 1)
          sv = 0x18 - x6;
        else
          sv = x6 + 8;
        W_steps.set(static_cast<int16_t>(sv));
        if ((static_cast<uint8_t>(W_facing.get()) & 1) != 0 && W_steps.get() > 0x10) W_steps.set(0x10);
      } else {
        W_facing.set(static_cast<int8_t>(octant(angle_fixed(s30, s28))));
        W_seq.set(0xb);
        W_wait.set(static_cast<int16_t>(rnd(0x10) + 1));
      }
      if (W_steps.get() == 0) continue;
    }
    // ---- keep clear of the earlier walkers
    for (int32_t j = 0; j < cur; ++j) {
      const Ptr<Walker> o = state_arr(ctx, &GameState::g_walkers) + j;
      if (o.field(&Walker::flags).get() == 0) continue;
      const int32_t dxv = sub32(o.field(&Walker::x).get(), W_x.get());
      const int32_t dyv = sub32(o.field(&Walker::y).get(), W_y.get());
      if (approx_distance(dxv, dyv) < 0x155) {
        const int32_t d8 = direction8(dxv, dyv);
        const int32_t dv = W_facing.get();
        if (dv == d8 || dv == ((d8 + 1) & 7) || dv == ((d8 - 1) & 7)) {
          W_wait.set(static_cast<int16_t>(rnd(4) + 4));
          W_seq.set(0xb);
        }
      }
      if (W_wait.get() != 0) W_facing.set(static_cast<int8_t>(octant(angle_fixed(dxv, dyv))));
    }
    // ---- take the step
    const int32_t tile = add32(sar32(W_y.get(), 10), mul32(sar32(W_x.get(), 10), 50));
    const int8_t typ = ttype[tile];
    int32_t speed = clamp(6 - (state_arr(ctx, &GameState::g_tileTypeAttrs) + typ).field(&TileTypeAttr::path_cost).get(), 2, 5);
    if (((tflags + tile).as<uint8_t>().get() & 0x20) != 0) speed = 6;
    const uint8_t wf = W_flags.get();
    if ((wf & 8) != 0) speed += 2;
    if (static_cast<uint8_t>(W_kind.get()) == 0xfa) speed += 2;
    if ((wf & 0x10) != 0) speed = mode_n == 2 ? 4 : 3;
    const int32_t sp = speed << 6;
    const int32_t dsel = W_facing.get();
    const int32_t div = ((static_cast<uint8_t>(W_facing.get()) & 1) != 0 ? 1 : 0) + 3;
    W_x.set(add32(W_x.get(), (mul32(dx8[dsel], sp) / 2) / div));
    const int32_t dsel2 = W_facing.get();
    W_y.set(add32(W_y.get(), (mul32(dy8[dsel2], sp) / 2) / (((static_cast<uint8_t>(W_facing.get()) & 1) != 0 ? 1 : 0) + 3)));
    W_steps.set(static_cast<int16_t>(W_steps.get() - 1));
    if (W_steps.get() != 0) {
      if (static_cast<int16_t>(rnd(4)) == 0) W_steps.set(static_cast<int16_t>(W_steps.get() - 1));
    }
    const int16_t an = W_seq.get();
    if (an < 7) {
      W_seq.set(7);
    } else {
      const int16_t an1 = static_cast<int16_t>(an + 1);
      W_seq.set(an1);
      if (an1 > (mode_n == 3 ? 9 : 10)) W_seq.set(7);
    }
  }
  return 0x40;
}

// ------------------------------------------------------------------------------------------------ arrivals and ball records
void init_golfer_state(Ctx &ctx) {
  GameState &gs = ctx.gs();
  for (int32_t g = 0; g < 152; ++g) golfer(ctx, g).field(&Golfer::hole).set(0);
  gs.g_nextArrivalOrder = 0;
  gs.g_nextGolferSlot = 0;
  const int reps = SG_NEGCTL(g_negctl_init_golfer_state) == 1 ? 1 : 2;
  for (int32_t h = 18; h >= 0; --h) {
    if ((state_arr(ctx, &GameState::g_holes) + h).field(&HoleRecord::par).get() == 0) continue;
    for (int k = 0; k < reps; ++k) {
      const int32_t r = i32(gl::call_peer(ctx, "updateMembership", {1u}).eax);
      const Ptr<Golfer> g = golfer(ctx, r);
      const int32_t n = gs.g_nextArrivalOrder;
      g.field(&Golfer::hole).set(static_cast<int8_t>(h));
      g.field(&Golfer::order).set(static_cast<int16_t>(n));
      gs.g_nextArrivalOrder = add32(n, 1);
      g.field(&Golfer::partner).set(static_cast<int16_t>(r ^ 1));
    }
  }
}

int32_t add_golfer_to_table(Ctx &ctx, int32_t g) {
  GameState &gs = ctx.gs();
  const Ptr<Golfer> s = golfer(ctx, g);
  const Ptr<BallRecord> balls = state_arr(ctx, &GameState::g_ballRecords);
  for (int32_t e = 0; e < 256; ++e) {
    if ((balls + e).field(&BallRecord::x).get() != -1) {
      // a range draw and a distance whose comparison feeds nothing: only the draw changes state
      const int32_t arg = add32(shl32(gs.g_difficulty, 10) / 4, 0x600);
      (void)gl::rnd(ctx, arg);
    }
    if (gs.g_forcedEventBits & 1u) {
      if (SG_NEGCTL(g_negctl_add_golfer_to_table) != 2) gs.g_forcedEventBits &= ~1u;
      gl::call_peer(ctx, "emitGolferThought", {u32(g), 0x18u, 0x14u});
    }
    gl::call_peer(ctx, "holeQuadrants", {u32(s.field(&Golfer::ball_x).get()), u32(s.field(&Golfer::ball_y).get()), 6u});
  }
  int32_t i = 0;
  while ((balls + i).field(&BallRecord::x).get() != -1)
    if (++i >= 256) return i;
  const Ptr<BallRecord> b = balls + i;
  b.field(&BallRecord::start_x).set(s.field(&Golfer::ball_x).get());
  b.field(&BallRecord::x).set(s.field(&Golfer::ball_x).get());
  b.field(&BallRecord::start_y).set(s.field(&Golfer::ball_y).get());
  b.field(&BallRecord::y).set(s.field(&Golfer::ball_y).get());
  b.field(&BallRecord::height).set(s.field(&Golfer::ball_z).get());
  b.field(&BallRecord::direction).set(i32(s.field(&Golfer::ball_dir).get()));
  b.field(&BallRecord::speed).set(s.field(&Golfer::ball_speed).get() / (SG_NEGCTL(g_negctl_add_golfer_to_table) == 1 ? 4 : 2));
  const int32_t vz = s.field(&Golfer::ball_vz).get();
  const int32_t hi = static_cast<int32_t>((static_cast<int64_t>(vz) * 0x55555556LL) >> 32);
  const int32_t sign = i32(u32(hi) >> 31);
  b.field(&BallRecord::vspeed).set(add32(hi, sign));
  b.field(&BallRecord::date).set(gs.g_date);
  return sign;
}
