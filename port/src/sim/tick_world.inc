// drawWorldObjects (phase 3.7a; included by tick.cpp inside namespace portsim::tk), ported from its C3 hook source
// shim/src/re/c3fv.cpp (drawWorldObjects, Decide) with the purpose file log/c3/c3fv_purpose.md. P3 calls it once per frame.
// Its simulation effects: the 128 ground animals (g_habitatSlots, 0x14-byte records) walk, idle, follow leaders and react
// to golfers (emitGolferThought, RNG draws), the four flyers (g_skyActors, 0x10-byte records) spawn, move and leave, the
// last sprite cell of every draw lands in unk_005a9cc8, and g_flags bit 0 is set for the duration. Dropped: the spawn
// zoom-in blit (Surface_drawSprite and its .rdata scale), the debug index labels' drawing (the text itself is kept).
struct World : TkBase {
  using TkBase::TkBase;

  int Range(int n) { return Rnd(n); }
  int DirX(int d) { return G32(GS(f_tab_dirDx_4c2878) + 4u * static_cast<uint32_t>(d)); }
  int DirY(int d) { return G32(GS(f_tab_dirDy_4c2898) + 4u * static_cast<uint32_t>(d)); }
  uint8_t FlagsByte() { return GU8(GS(g_flags)); }
  int Date() { return G32(GS(g_date)); }
  int Kind(int x, int y) { return cc("typeAtPos", x, y); }
  int Sheet(int id) { return G32(GS(g_sheetCols) + 4u * static_cast<uint32_t>(id)); }

  // the spawn rise offsets the original builds in its frame (indexed by flags + 0x14, 20 used)
  static constexpr signed char kSpawnRise[20] = {-55, -54, -52, -49, -45, -40, -34, -27, -19, -10,
                                                 0,   -4,  -7,  -8,  -7,  -4,  0,   -2,  0,   0};

  struct Animal {  // a ground animal: +0 x, +4 y, +8 flags, +0xc leader, +0xe state, +0x10 frame, +0x11 dir, +0x12 kind
    World *w;
    uint32_t r;
    Ref<int32_t> x() const { return w->G<int32_t>(r); }
    Ref<int32_t> y() const { return w->G<int32_t>(r + 4); }
    Ref<int32_t> flags() const { return w->G<int32_t>(r + 8); }
    Ref<int16_t> leader() const { return w->G<int16_t>(r + 0xC); }
    Ref<int16_t> state() const { return w->G<int16_t>(r + 0xE); }
    Ref<int8_t> frame() const { return w->G<int8_t>(r + 0x10); }
    Ref<int8_t> dir() const { return w->G<int8_t>(r + 0x11); }
    Ref<int8_t> kind() const { return w->G<int8_t>(r + 0x12); }
  };
  Animal Rec(int i) { return Animal{this, GS(g_habitatSlots) + static_cast<uint32_t>(mul32(0x14, i))}; }

  // the decisions taken when an animal's animation frame wraps to 0
  void Decide(Animal a, int index) {
    int target;
    const int16_t lead = a.leader();
    if (lead == -1 && Rec(-1).kind() != -1) {  // record -1's kind byte (the word before g_habitatSlots)
      if ((Range(3) & 0xFFFF) == 0) a.flags() &= ~2;
      // the 152 golfer records (p = record + 0xe4)
      int g = 0;
      const uint32_t first = GS(g_golfers) + 0xE4, end = GS(g_golfers) + 0x98 * 0x100 + 0xE4;
      for (uint32_t p = first; p < end; p += 0x100, g++) {
        if (GU8(p - 0xC3) == 0xFF) continue;
        if (cc("distance", G32(p - 0xE4) - a.x(), G32(p - 0xE0) - a.y()) >= 0x800) {
          if (G32(p) <= 0 || G32(p - 8) >= 0x96) continue;
          if (cc("distance", G32(p - 0x10) - a.x(), G32(p - 0xC) - a.y()) >= 0x800) continue;
        }
        if (!(a.flags() & 2) && G8(p - 0x60) == 0 && GU8(p - 0x5F) != 0x27) cc("emitGolferThought", g, 0x27, a.kind().get());
        a.flags() |= 2;
        a.dir() = static_cast<int8_t>(cc("direction8", a.x() - G32(p - 0xE4), a.y() - G32(p - 0xE0)));
        if ((Range(3) & 0xFFFF) == 0) cc("playHabitatSound", a.kind().get(), a.x().get(), a.y().get());
      }
      target = Range(8) & 0xFFFF;
      if (a.state() == 0x103 && (Range(3) & 0xFFFF) != 0) target = a.dir();
      const int home = G8((GS(f_tab_animals_4c1998) + 0x10) + static_cast<uint32_t>(mul32(0x12, a.kind())));
      if (Kind(shl32(DirX(target), 10) + a.x(), shl32(DirY(target), 10) + a.y()) != home &&
          Kind(shl32(DirX(target), 10) + a.x(), shl32(DirY(target), 10) + a.y()) != 4 &&
          Kind(a.x(), a.y()) == G8((GS(f_tab_animals_4c1998) + 0x10) + static_cast<uint32_t>(mul32(0x12, a.kind()))))
        target = -1;
      if (a.flags() & 2) {
        const int r = Range(3) & 0xFFFF;
        target = (static_cast<uint8_t>(a.dir().get()) + r - 1) & 7;
        a.dir() = static_cast<int8_t>(target);
        a.state() = 0x103;
      }
    } else {
      // follow the leader record (record -1 when the index is -1)
      Animal l = Rec(lead);
      const int dx = l.x() - a.x(), dy = l.y() - a.y();
      int d = cc("distance", dx, dy);
      if (GU8(l.r + 8) & 2) {
        d = 0x2800;
        a.state() = 0x103;
      }
      const int thr = ((((a.state() != 0x103) ? 1 : 0) + 2) << 10) / ((a.kind() != 0 ? 1 : 0) + 2);
      if (d > thr) {
        target = cc("direction8", dx, dy);
        if (d < 0x800) target = (index % 3 + target - 1) & 7;
        a.dir() = static_cast<int8_t>(target);
      } else {
        target = -1;
      }
    }
    // the idle / walk state machine
    if (a.state() == 0x11E && (Range(2) & 0xFFFF) != 0) {
      a.state() = 0x115;
      a.flags() |= 1;
    } else if (a.state() == 0x115) {
      a.state() = (a.flags() & 1) ? 0x10C : 0x11E;
      a.flags() &= ~1;
    } else {
      if (a.state() == 0x10C && (Range(10) & 0xFFFF) == 0) a.state() = 0x115;
      if (a.state() == 0x10C && (Range(2) & 0xFFFF) == 0 && target != -1) {
        a.state() = 0x103;
        a.dir() = static_cast<int8_t>(target);
      }
      if (a.state() == 0x103) {
        const int r = Range(5) & 0xFFFF;
        if (r == 0 || a.dir() != target) a.state() = 0x10C;
      }
    }
  }

  void drawWorldObjects() {
    LV sx(*this), sy(*this);  // worldToScreen's outputs, kept across the whole body like the original's frame slots
    for (int i = 0; i < 0x80; i++) {
      Animal a = Rec(i);
      if (a.kind() == -1 || !(GU8(GS(g_preferences)) & 0x20)) continue;
      if (a.kind() >= 9) a.kind() = 1;
      int lf;
      int view;
      if (a.flags() < 0) {
        a.state() = 0x103;
        lf = 0;
        view = a.dir();
      } else {
        const int idx = a.kind() + a.state();
        lf = a.frame();
        if (!(FlagsByte() & 4)) {
          const int n = Sheet(idx);
          if (n == 0) {
            lf = 0;
          } else {
            a.frame() = static_cast<int8_t>(a.frame() + 1);
            lf = a.frame() % n;
          }
          if (a.state() == 0x103) {
            const int d = a.dir();
            const int tx = sar32(shl32(DirX(d), 10) / 3 + a.x(), 10);
            const int ty = sar32(shl32(DirY(d), 10) / 3 + a.y(), 10);
            const uint32_t cell = static_cast<uint32_t>(add32(mul32(0x32, tx), ty));
            if (cc("tileBlocked", tx, ty) != 0 || G8(GS(g_tileType) + cell) == 0x11 || G8(GS(g_tileType) + cell) == 0x16) {
              a.dir() = static_cast<int8_t>((a.dir() + 1) & 7);
              if ((Range(100) & 0xFFFF) == 0) a.kind() = -1;
            }
            const int16_t lead = a.leader();
            if (!(a.flags() & 2) &&
                (lead == -1 || !(GU8(GS(g_habitatSlots) + 8 + static_cast<uint32_t>(mul32(0x14, lead))) & 2))) {
              const int div = a.kind() != 0 ? 4 : 2;
              a.x() += shl32(DirX(a.dir()), 6) / div;
              a.y() += shl32(DirY(a.dir()), 6) / div;
            } else {
              a.x() += shl32(DirX(a.dir()), 6);
              a.y() += shl32(DirY(a.dir()), 6);
            }
          }
          if (lf == 0) Decide(a, i);
        }
        a.frame() = static_cast<int8_t>(lf);
        if (a.state() == 0x115 && (a.flags() & 1)) lf = Sheet(a.kind() + a.state()) - lf - 1;
        view = (G32(GS(g_cameraRot)) - static_cast<uint8_t>(a.dir().get()) - 2) & 7;
        if (a.state() != 0x103 && a.kind() != 4) view /= 2;
      }
      // draw
      const int cellId = a.kind() + a.state();
      if (a.kind() != -1 && cc("worldToScreen", a.x().get(), a.y().get(), &sx, &sy, 0) != 0) {
        int mode = 4;
        if (a.leader() != -1) mode = a.kind() == 4 ? 2 : 3;
        const int cell = cc("cellAt", cellId, lf, view);
        G32(GS(unk_005a9cc8)) = cell;
        if (a.flags() < 0) {
          // the spawn zoom-in: Surface_drawSprite with scale k * [.rdata] + [.rdata] (drawing, dropped)
          a.flags() += 1;
          if (a.flags() == 0) cc("playHabitatSound", a.kind().get(), a.x().get(), a.y().get());
        } else {
          cc("enqueueSprite", sx.get(), sy.get(), sy.get(), 0x1E, 0x28, cell, mode, a.kind() + 0x78);
        }
        if (FlagsByte() & 0x20) {  // the debug index label: the text is built, its drawing is dropped
          GU8(kBuf) = 0;
          StrCat(kBuf, static_cast<uint32_t>(Itoa(i, GS(g_textBuf), 10)));
        }
      }
      if (Kind(a.x(), a.y()) == 0x14) a.kind() = -1;
    }
    GU32(GS(g_flags)) |= 1u;

    // the four flyers: +0 x (-1 inactive), +4 y, +8 counter, +0xc direction, +0xd kind
    for (int j = 0; j < 4; j++) {
      const uint32_t f = GS(g_skyActors) + 0x10u * static_cast<uint32_t>(j);
      Ref<int32_t> X = G<int32_t>(f), Y = G<int32_t>(f + 4), V = G<int32_t>(f + 8);
      Ref<int8_t> D = G<int8_t>(f + 0xC);
      const uint32_t kind = static_cast<uint32_t>(static_cast<int>(G8(f + 0xD).get()));
      if (kind == 0) {
        if (X == -1) {
          if (SG_NEGCTL(g_negctl_draw_world_objects) == 1) continue;  // no spawn roll for kind 0
          if ((Range(100) & 0xFFFF) == 0 && Date() > 0x2000) {
            X = 0;
            Y = shl32(Range(50) & 0xFFFF, 10);
            V = 0x28;
          }
          continue;
        }
        if (cc("worldToScreen", X.get(), Y.get(), &sx, &sy, 0) != 0 && Kind(X, Y) != 0x14) {
          int c = cc("cellAt", 0xF1, 0, (Date() / 1000) & 3);
          G32(GS(unk_005a9cc8)) = c;
          cc("enqueueSprite", sx + V / 2 - 0x16, sy - V / 2, sy - 1, 0x8C, 0x8C, c, 4, 0x54);
          c = cc("cellAt", 0xF0, 0, sar32(Y, 10) & 3);
          G32(GS(unk_005a9cc8)) = c;
          cc("enqueueSprite", sx.get(), sy - V + 0x16, sy.get(), 0x8C, 0x8C, c, 4, 0x54);
        }
        if (FlagsByte() & 4) continue;
        X += shl32(cc("clamp", V.get(), 0, 0x28), 6) / 200;
        if (X > 0xCC00) X = -1;
        const int tx = sar32(X, 10), ty = sar32(Y, 10);
        const int date = Date();
        if ((date & 3) == 0) {
          switch (mod32(div32(date, ty * 4 + 0x100), 5)) {
            case 0:
              if (V == 0) cc("playSoundAt", 0x39, X.get(), Y.get(), 0);
              {
                const int v = V;
                if (v < mul32(cc("heightBlend", tx, ty, 0), G32(GS(f_w_4c2e00))) + 0x3C) V = v + 1;
              }
              break;
            case 1:
            case 4:
              if (V == 0) cc("playSoundAt", 0x39, X.get(), Y.get(), 0);
              {
                const int v = V;
                if (v < mul32(cc("heightBlend", tx, ty, 0), G32(GS(f_w_4c2e00))) + 0x28) V = v + 1;
              }
              break;
            case 2:
            case 3: {
              const uint32_t cell = static_cast<uint32_t>(ty + 0x32 * tx);
              const int8_t c1 = G8(GS(g_tileType) + cell);
              const uint8_t t1 = GU8(GS(g_tileTypeAttrs) + 0x26 + static_cast<uint32_t>(0x30 * c1));
              bool rise = t1 == 0x0D;
              int8_t c2 = 0;
              if (!rise) {
                c2 = G8(GS(g_tileType) + 0x32 + cell);
                rise = GU8(GS(g_tileTypeAttrs) + 0x26 + static_cast<uint32_t>(0x30 * c2)) == 0x0D || c1 == 0x16 || c2 == 0x16 ||
                       c1 == 0x15 || c2 == 0x15 || t1 == 0x11 || G8(GS(g_tileTypeAttrs) + 0x22 + static_cast<uint32_t>(0x30 * c1)) <= 0;
              }
              if (rise) {
                if (V < 0x28) V = V + 1;
              } else if (V > 0) {
                V = V - 1;
              }
              break;
            }
          }
        }
        if (Y > 0xCC00) X = -1;
      } else if (kind == 1) {
        if (X == -1) {
          if ((Range(100) & 0xFFFF) == 0) {
            const int r = Range(50) & 0xFFFF;
            if (GU8(GS(g_tileTypeAttrs) + 0x26 + static_cast<uint32_t>(0x30 * G8(GS(g_tileType) + static_cast<uint32_t>(0x32 * r)))) == 0x11) {
              X = shl32(r, 10) + 0x200;
              Y = 0x200;
              D = 4;
            }
          }
          continue;
        }
        if (!(FlagsByte() & 4)) {
          const int date = Date();
          const int q = 0x80 / (((date >> 8) & 3) + 3);
          X = mul32(DirX(D), q) + X;
          Y = Y + mul32(DirY(D), q);
          if ((date & 7) == 0 && (D != 0 || Y > 0x800) && Kind(mul32(DirX(D), 0x600) + X, mul32(DirY(D), 0x600) + Y) != 0x11) {
            int best = D, bestScore = 0x63;
            for (int k = 0; k < 8; k += 2) {
              if (k == D) continue;
              if (Kind(shl32(DirX(k), 10) + X, shl32(DirY(k), 10) + Y) != 0x11) continue;
              if (Kind(shl32(DirX(k), 11) + X, shl32(DirY(k), 11) + Y) != 0x11) continue;
              int diff = (static_cast<uint8_t>(D.get()) - k) & 7;
              if (diff > 4) diff = 8 - diff;
              const int s = (Range(15) & 0xFFFF) + (diff << 4);
              if (s < bestScore) {
                bestScore = s;
                best = k;
              }
            }
            D = static_cast<int8_t>(best);
          }
          if (Y < 0x200 && DirY(D) == -1) X = -1;
        }
        if (cc("worldToScreen", X.get(), Y.get(), &sx, &sy, 0) != 0) {
          const int cam = G32(GS(g_cameraRot));
          const int view = G32((GS(g_sheetRows) + 0x684)) == 8 ? ((cam - static_cast<uint8_t>(D.get()) - 2) & 7)
                                                     : (sar32(cam - static_cast<uint8_t>(D.get()) - 2, 1) & 3);
          const int c = cc("cellAt", 0x1A1, mod32(Date() / 2, Sheet(0x1A1)), view);
          G32(GS(unk_005a9cc8)) = c;
          cc("enqueueSprite", sx.get(), sy.get(), sy.get(), 0x50, 0x50, c, 4, 0x53);
          if ((GU8(GS(g_date)) & 0x7F) == 0) cc("playSoundAt", 0xD1, X.get(), Y.get(), 0);
        } else if ((Range(200) & 0xFFFF) == 0) {
          X = -1;
        }
      } else if (kind == 2) {
        if (X == -1) {
          if ((Range(100) & 0xFFFF) == 0 && Date() > 0x2000) {
            X = 0;
            Y = shl32(Range(50) & 0xFFFF, 10);
            V = 0x50;
          }
          continue;
        }
        if (cc("worldToScreen", X.get(), Y.get(), &sx, &sy, 0) != 0 && Kind(X, Y) != 0x14) {
          int c = cc("cellAt", 0xF3, 0, (G32(GS(g_cameraRot)) / 2 - 2) & 3);
          G32(GS(unk_005a9cc8)) = c;
          cc("enqueueSprite", sx - 0x2C, sy.get(), sy - 1, 0xF0, 0xF0, c, 4, 0x3A);
          c = cc("cellAt", 0xF2, 0, (G32(GS(g_cameraRot)) / 2 - 2) & 3);
          G32(GS(unk_005a9cc8)) = c;
          const int lift = mul32(V - 0x20, G32(GS(g_zoomScale))) / 4;
          cc("enqueueSprite", sx.get(), sy - lift, sy.get(), 0xF0, 0xF0, c, 4, 0x3A);
        }
        if (FlagsByte() & 4) continue;
        X += mul32(cc("clamp", V.get(), 0, 0x28), 0x40) / 0x80;
        if (X > 0xCC00) X = -1;
        if (Y > 0xCC00) X = -1;
      } else if (kind == 3) {
        if (X == -1) {
          if ((Range(100) & 0xFFFF) == 0) {
            switch (Range(4) & 0xFFFF) {
              case 0: X = 0;      Y = shl32(Range(50) & 0xFFFF, 10); D = 2; break;
              case 1: Y = 0;      X = shl32(Range(50) & 0xFFFF, 10); D = 4; break;
              case 2: X = 0xC800; Y = shl32(Range(50) & 0xFFFF, 10); D = 6; break;
              case 3: Y = 0xC800; X = shl32(Range(50) & 0xFFFF, 10); D = 0; break;
            }
          }
          continue;
        }
        if (cc("worldToScreen", X.get(), Y.get(), &sx, &sy, 0) != 0 && Kind(X, Y) != 0x14) {
          const int date = Date(), k = Sheet(0x207);
          int id, fr;
          if (mod32(date, 0x30) < 2 * k) {
            id = 0x207;
            fr = mod32(mod32(date, 0x30), k);
          } else {
            id = 0x206;
            fr = mod32(date, Sheet(0x206));
          }
          const int c = cc("cellAt", id, fr, (G32(GS(g_cameraRot)) / 2 - D / 2 - 1) & 3);
          G32(GS(unk_005a9cc8)) = c;
          cc("enqueueSprite", sx.get(), sy.get(), G32(GS(g_tileStep4c2840)) + sy, 0x8C, 0x8C, c, 4, 0x3C);
        }
        if (FlagsByte() & 4) continue;
        X += shl32(DirX(D), 7);
        Y += shl32(DirY(D), 7);
        if (X > 0xCC00 || X < 0) X = -1;
        if (Y > 0xCC00 || Y < 0) X = -1;
      }
    }
    GU32(GS(g_flags)) &= ~1u;
  }
};
