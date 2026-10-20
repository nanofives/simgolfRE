// The render-side callees of mainLoop's frame whose effects reach simulation state, or whose results the frame stores into
// it (phase 3.7a; included by tick.cpp inside namespace portsim::tk). Written from the C3 hook sources (cited per function)
// and the census of log/p3/p37a_simwrites.py; drawing calls inside them are dropped, every state write is kept.
//
//   resetSpriteQueue / enqueueSprite / addSprite / snapshotTables / renderSpriteQueue: the sprite queue (render-class
//     arrays g_sprite*), kept whole because drawQueuedSprite reads it;
//   drawQueuedSprite: its simulation part is composeGolferSprite(g) for a golfer sprite (look indices written back into
//     the golfer type record, phase 3.3) and the name text it builds in g_textBuffer;
//   cellAt: a sprite-sheet cell's address (mainLoop stores it into unk_005a9cc8, a sim-class word);
//   tileToScreen / screenToTile: the per-tile screen cache g_tileScreenX / g_tileScreenY (sim class);
//   setDirtyFlagA / setDirtyFlagB: H1's take-and-clear readers of the key code / key words.

struct Rend : TkBase {
  using TkBase::TkBase;

  // resetSpriteQueue (c3fu.cpp): count 0, aux 0, snapshot aux 0, head -1; the original's 0x400-pass loop stores -1 into
  // the same dword g_spriteSnapHead every pass
  int resetSpriteQueue() {
    gs().g_spriteQueueCount = 0;
    G32(GS(g_spriteQueueAux)) = 0;
    G32(GS(g_spriteSnapAux)) = 0;
    gs().g_spriteQueueHead = -1;
    G32(GS(g_spriteSnapHead)) = -1;
    return 0;
  }
  GameState &gs() { return ctx.gs(); }

  // the sprite scale divisor of addSprite / enqueueSprite / drawQueuedSprite: 0x10, 0xd above a 1000-pixel screen, 0xa from
  // 0x4b0, 0x10 again for golfer sprites (flag 0x100)
  int scaleDiv(int flags) {
    int d = 0x10;
    const int w = gs().g_screenWidth;
    if (w > 1000) d = 0xd;
    if (w >= 0x4b0) d = 0xa;
    if (flags & 0x100) d = 0x10;
    return d;
  }
  void spritePos(int a1, int a2, int a4, int a5, int a7, int flags, int &x, int &y) {
    const int d = scaleDiv(flags);
    const int s = gs().g_zoomScale;
    if (a7 == 0) {
      const int px = mul32(s, a4), py = mul32(s, a5);
      x = sub32(a1, sar32(add32(px, sar32(px, 31) & 3), 2));
      y = sub32(a2, sar32(add32(py, sar32(py, 31) & 3), 2));
    } else if (a7 > 0) {
      x = sub32(a1, mul32(mul32(s, a4), a7) / d);
      y = sub32(a2, mul32(mul32(s, a5), a7) / d);
    } else {
      x = add32(mul32(mul32(s, a4), a7) / (d + d), a1);
      y = add32(mul32(mul32(s, a5), a7) / (d + d), a2);
    }
  }
  Ref<int32_t> qa(uint32_t arr, int i) { return G<int32_t>(arr + 4u * static_cast<uint32_t>(i)); }

  // enqueueSprite (c3df.cpp): appends entry n (when n < 0x3ff) and links it into the list sorted by key a3
  void enqueueSprite(int p1, int p2, int p3, int p4, int p5, int p6, int p7, int p8) {
    const int n = gs().g_spriteQueueCount;
    if (n >= 0x3ff) return;
    int x, y;
    spritePos(p1, p2, p4, p5, p7, p8, x, y);
    qa(GS(g_spriteX), n) = x;
    qa(GS(g_spriteY), n) = y;
    qa(GS(g_spriteShade), n) = p7;
    qa(GS(g_spriteAnchorY), n) = p2;
    qa(GS(g_spriteFlags), n) = p8;
    qa(GS(g_spriteObj), n) = p6;
    qa(GS(g_spriteAnchorX), n) = p1;
    qa(GS(g_spriteSortKey), n) = p3;
    const int head = gs().g_spriteQueueHead;
    int node = head, prev = p4;
    bool atHead = true;
    if (head != -1) {
      for (;;) {
        if (qa(GS(g_spriteSortKey), node) >= p3) break;
        prev = node;
        node = qa(GS(g_spriteQueueNext), node);
        if (node == -1) break;
      }
      atHead = node == head;
      if (!atHead) {
        qa(GS(g_spriteQueueNext), n) = node;
        qa(GS(g_spriteQueueNext), prev) = n;
      }
    }
    if (SG_NEGCTL(g_negctl_sprite_queue) == 2) atHead = false;  // the entry is not linked
    if (atHead) {
      qa(GS(g_spriteQueueNext), n) = head;
      gs().g_spriteQueueHead = n;
    }
    gs().g_spriteQueueCount = n + 1;
    if (n + 1 < 0x400) qa(GS(g_spriteQueueAux), n + 1) = 0;
  }

  // addSprite (c3ei.cpp): fills entry [count] (not advanced) and draws it at once through drawQueuedSprite
  int addSprite(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8) {
    const int i = gs().g_spriteQueueCount;
    int x, y;
    spritePos(a1, a2, a4, a5, a7, a8, x, y);
    qa(GS(g_spriteX), i) = x;
    qa(GS(g_spriteY), i) = y;
    qa(GS(g_spriteAnchorY), i) = a2;
    qa(GS(g_spriteShade), i) = a7;
    qa(GS(g_spriteAnchorX), i) = a1;
    qa(GS(g_spriteSortKey), i) = a3;
    qa(GS(g_spriteObj), i) = a6;
    qa(GS(g_spriteFlags), i) = a8;
    return drawQueuedSprite(i);
  }

  // drawQueuedSprite (c3ei.cpp), simulation part: with a non-zero shade and flags != -1 carrying 0x100 (a golfer sprite),
  // composeGolferSprite(flags & 0xff) and, for a golfer below 0x98, the name label text in g_textBuffer (buildGolferName,
  // f_strpool_4c4e60 for record flag 0x4000, the golfer number for g_flags bit 5, cleared in panel mode 3 without record
  // flag 0x200). The blits, the display scale and the label bars are drawing. Returns 0 (the original returns its local
  // cache's destructor result, which no frame code reads).
  int drawQueuedSprite(int i) {
    const int p7 = qa(GS(g_spriteShade), i);
    if (p7 == 0) return 0;
    const int a8 = qa(GS(g_spriteFlags), i);
    if (a8 == -1 || !(a8 & 0x100)) return 0;
    const int g = a8 & 0xff;
    if (SG_NEGCTL(g_negctl_sprite_queue) != 1) cc("composeGolferSprite", g);
    if (g != 0xff && g < 0x98) {
      GU8(kBuf) = 0;
      cc("buildGolferName", g, 0);
      const uint32_t rec = static_cast<uint32_t>(g) << 8;
      const uint32_t fl = GU32(GS(g_golfers) + 0x10 + rec);
      if (fl & 0x4000) StrCat(kBuf, GS(f_strpool_4c4e60));
      if (GU8(GS(g_flags)) & 0x20) StrCat(kBuf, static_cast<uint32_t>(Itoa(g, GS(g_golferIdText), 10)));
      if (gs().g_panelListMode == 3 && !(fl & 0x200)) GU8(kBuf) = 0;
    }
    return 0;
  }

  // snapshotTables (c3dx.cpp): copies the queue arrays to the snapshot arrays; returns the count
  int snapshotTables() {
    const int keep = gs().g_spriteQueueCount;
    struct P {
      uint32_t dst, src;
    };
    const P cp[] = {{GS(g_spriteSnapX), GS(g_spriteX)},           {GS(g_spriteSnapY), GS(g_spriteY)},
                    {GS(g_spriteSnapAnchorX), GS(g_spriteAnchorX)}, {GS(g_spriteSnapAnchorY), GS(g_spriteAnchorY)},
                    {GS(g_spriteSnapSortKey), GS(g_spriteSortKey)}, {GS(g_spriteSnapShade), GS(g_spriteShade)},
                    {GS(g_spriteSnapNext), GS(g_spriteQueueNext)},  {GS(g_spriteSnapObj), GS(g_spriteObj)},
                    {GS(g_spriteSnapAux), GS(g_spriteQueueAux)},    {GS(g_spriteSnapFlags), GS(g_spriteFlags)}};
    G32(GS(g_spriteSnapCount)) = keep;
    for (const P &p : cp) rep_movs(ctx.mem, p.dst, p.src, 0x1000);
    if (SG_NEGCTL(g_negctl_snapshot_tables) != 1) G32(GS(g_spriteSnapHead)) = gs().g_spriteQueueHead;
    return keep;
  }

  // renderSpriteQueue (c3ev.cpp): drawQueuedSprite for every entry in list order
  int renderSpriteQueue() {
    for (int i = gs().g_spriteQueueHead; i != -1; i = qa(GS(g_spriteQueueNext), i)) drawQueuedSprite(i);
    return 0;
  }

  // cellAt(table, col, row) (golf_tables.cpp): the address of cell (col, row) of sprite sheet `table` in g_cellPool
  // (0x2c bytes per cell), or g_surfaces53ba48 when the sheet is unloaded or col is past its width
  uint32_t cellAt(int table, int col, int row) {
    const int base = G32(GS(g_sheetFirstCell) + 4u * static_cast<uint32_t>(table));
    if (base == -1) return GS(g_surfaces53ba48);
    const int w = G32(GS(g_sheetCols) + 4u * static_cast<uint32_t>(table));
    if (col >= w) return GS(g_surfaces53ba48);
    const int cell = add32(add32(add32(mul32(w, row), base), col), SG_NEGCTL(g_negctl_cell_at) == 1 ? 1 : 0);
    return GS(g_cellPool) + static_cast<uint32_t>(mul32(cell, 0x2c));
  }

  // tileToScreen(tx, ty, &sx, &sy) (c3h.cpp): the tile centre on screen through the cache g_tileScreenX / Y (0 = not
  // computed, -99 = off screen); returns 1 when on screen
  int tileToScreen(int tx, int ty, uint32_t sx, uint32_t sy) {
    if (tx < 0 || tx >= 50 || ty < 0 || ty >= 50) return 0;
    const uint32_t cell = static_cast<uint32_t>(tx * 50 + ty);
    const int16_t c = G16(GS(g_tileScreenX) + cell * 2);
    if (c != 0) {
      if (c == -99) return 0;
      G32(sx) = c;
      G32(sy) = G16(GS(g_tileScreenY) + cell * 2).get();
      return 1;
    }
    cc("worldToScreen", (tx << 10) + 0x200, (ty << 10) + 0x200, sx, sy, 0);
    const int x = G32(sx), y = G32(sy);
    if (x >= -0x40 && x < gs().g_screenWidth + 0x40 && y >= -0x2a && y < gs().g_screenHeight + 0x2a) {
      if (SG_NEGCTL(g_negctl_tile_to_screen) == 1) return 1;  // the on-screen point is not cached
      G16(GS(g_tileScreenX) + cell * 2) = static_cast<int16_t>(x);
      G16(GS(g_tileScreenY) + cell * 2) = static_cast<int16_t>(y);
      return 1;
    }
    G16(GS(g_tileScreenX) + cell * 2) = static_cast<int16_t>(-99);
    return 0;
  }

  // screenToTile(px, py, &tx, &ty, useWorld) (c3p.cpp): the camera-rotation inverse, then a descent over the 8
  // neighbours by screen distance until no neighbour is nearer
  void screenToTile(int px, int py, uint32_t txp, uint32_t typ, int useWorld) {
    if (G32(GS(unk_005a9cc0)) != 0) {
      px = (px - 400) / 2 + 400;
      py = (py - 300) / 2 + 300;
    }
    int cx = px - gs().g_screenWidth / 2 - 0x40;
    int cy = py - gs().g_screenHeight / 2;
    const int rot = G32(GS(g_cameraRot));
    if (rot == 2) {
      cx += 0x20;
      cy += 0x14;
    } else if (rot == 4) {
      cx += 0x40;
    } else if (rot == 6) {
      cx += 0x20;
      cy -= 0x14;
    }
    const int s = shl32(cy, 5) / 40;
    const int camX = G32(GS(gCameraTileX)), camY = G32(GS(gCameraTileY));
    const int q = G32(GS(g_tileStep4c2840));
    if (G32(GS(g_cameraRot)) == 0) {
      G32(txp) = div32(div32(mul32(camX * 2 + 1, q) - s * 2 + cx, q), 2);
      G32(typ) = div32(div32(q + (mul32(camY, q) + s) * 2 + cx, q), 2);
    }
    if (G32(GS(g_cameraRot)) == 2) {
      G32(txp) = div32(div32(mul32(camX * 2 + 1, q) - s * 2 - cx, q), 2);
      G32(typ) = div32(div32(mul32(camY * 2 + 1, q) - s * 2 + cx, q), 2);
    }
    if (G32(GS(g_cameraRot)) == 4) {
      G32(txp) = div32(div32((mul32(q, camX) + s) * 2 - cx + q, q), 2);
      G32(typ) = div32(div32(mul32(camY * 2 + 1, q) - s * 2 - cx, q), 2);
    }
    if (G32(GS(g_cameraRot)) == 6) {
      G32(txp) = div32(div32(q + (mul32(q, camX) + s) * 2 + cx, q), 2);
      G32(typ) = div32(div32((mul32(camY, q) + s) * 2 - cx + q, q), 2);
    }
    // the screen point of the last projection: two locals the original keeps across projections
    LV a(*this), b(*this);
    if (SG_NEGCTL(g_negctl_screen_to_tile) == 1) return;  // no descent over the neighbours
    for (;;) {
      const int t = G32(txp), u = G32(typ);
      if (useWorld == 0) tileToScreen(t, u, &a, &b);
      else cc("worldToScreen", t << 10, (u + 1) << 10, &a, &b, 0);
      int best = cc("distance", px - a, (py - b) * 2);
      for (int k = 0; k < 8; k++) {
        const int nx = t + G32(GS(f_tab_dirDx_4c2878) + 4u * k);
        const int ny = u + G32(GS(f_tab_dirDy_4c2898) + 4u * k);
        if (useWorld == 0) tileToScreen(nx, ny, &a, &b);
        else cc("worldToScreen", nx << 10, (ny + 1) << 10, &a, &b, 0);
        const int d = cc("distance", px - a, (py - b) * 2);
        if (d < best) {
          best = d;
          G32(txp) = nx;
          G32(typ) = ny;
        }
      }
      if (G32(txp) == t && G32(typ) == u) return;
    }
  }

  // the static CRT's strrchr over the address space (0 when absent)
  uint32_t vstrrchr(uint32_t s, uint8_t ch) {
    uint32_t last = 0;
    for (;; ++s) {
      const uint8_t b = GU8(s);
      if (b == ch) last = s;
      if (b == 0) return ch == 0 ? s : last;
    }
  }

  // flushCourseInfo (c3dw.cpp) with its two callees' state effects: when g_pendingCourseInfo holds a path, writeCourseInfo
  // (c3eu.cpp) cuts it at its last '.' and appends the info-file extension (f_strpool_4c7818+0x110), fills g_textBuffer
  // with the course title (appendCourseTitle(1)) and then name string 0x14 (appendString), and writes the info file;
  // saveThumbAndFull (c3fa.cpp) cuts the path again and appends the thumbnail and full-size image names
  // (f_strpool_4c7818+0x20, +0x14) around its two screenshots. The file writes and screenshots are dropped (output files,
  // not state); the path edits, the text and the cleared first byte are kept.
  void flushCourseInfo() {
    const uint32_t path = GS(g_pendingCourseInfo);
    if (GU8(path) == 0) return;
    uint32_t dot = vstrrchr(path, '.');
    if (dot != 0) GU8(dot) = 0;
    StrCat(path, GS(f_strpool_4c7818) + 0x110);
    GU8(kBuf) = 0;
    cc("appendCourseTitle", 1);
    GU8(kBuf) = 0;
    cc("appendString", 0x14);
    dot = vstrrchr(path, '.');
    if (dot != 0) GU8(dot) = 0;
    StrCat(path, GS(f_strpool_4c7818) + 0x20);
    if (dot != 0) GU8(dot) = 0;
    StrCat(path, GS(f_strpool_4c7818) + 0x14);
    if (SG_NEGCTL(g_negctl_flush_course_info) != 1) GU8(path) = 0;
  }

  // View::fromParent(self, &x, &y) (c3d.cpp): subtracts the view's origin (+0x1bc + +0x1ac, +0x1c0 + +0x1b0) and, for a
  // child view (flag 0x20 at +0x9c with a parent at +0x130), the parent's chain, adding back the parent's +0x1ac / +0x1b0
  // with flag 0x8000
  void fromParent(uint32_t self, uint32_t x, uint32_t y) {
    if (SG_NEGCTL(g_negctl_view_from_parent) == 1) G32(x) = G32(x) + 1;
    G32(x) = G32(x) - (G32(self + 0x1bc) + G32(self + 0x1ac));
    G32(y) = G32(y) - (G32(self + 0x1c0) + G32(self + 0x1b0));
    if ((GU8(self + 0x9c) & 0x20) && GU32(self + 0x130) != 0) {
      const uint32_t parent = GU32(self + 0x130);
      fromParent(parent, x, y);
      if (GU32(self + 0x9c) & 0x8000) {
        G32(x) = G32(x) + G32(parent + 0x1ac);
        G32(y) = G32(y) + G32(parent + 0x1b0);
      }
    }
  }
  // View_toLocal(self, &x, &y) (c3dq.cpp): the cursor (the display's getCursor: the run's cursor input) made local to the
  // view; for the view-tree root g_viewTreeRoot the game window's screen origin (Display::getWindowRect, GetWindowRect)
  // is subtracted too, which no input of the port carries (MissingResource; g_viewTreeRoot is 0 in every segment image)
  void viewToLocal(uint32_t self, uint32_t x, uint32_t y) {
    if (x == 0 || y == 0) return;
    if (G32(GS(g_graphicsDevice)) != 0) {
      int32_t cx, cy;
      cursor_read(cx, cy);
      if (SG_NEGCTL(g_negctl_view_to_local) == 1) std::swap(cx, cy);
      G32(x) = cx;
      G32(y) = cy;
    }
    fromParent(self, x, y);
    if (self == GU32(GS(g_viewTreeRoot))) throw MissingResource{"View_toLocal: the game window's screen rectangle"};
  }

  // setDirtyFlagA / setDirtyFlagB (c3dr.cpp): take-and-clear of the key code gDirtyFlagA / the key gDirtyFlagB
  int setDirtyFlagA() {
    const int v = G32(GS(gDirtyFlagA));
    if (v != 0 && SG_NEGCTL(g_negctl_set_dirty_flag_a) != 1) G32(GS(gDirtyFlagA)) = 0;
    return v;
  }
  int setDirtyFlagB() {
    const int v = G32(GS(gDirtyFlagB));
    if (v != 0 && SG_NEGCTL(g_negctl_set_dirty_flag_b) != 1) G32(GS(gDirtyFlagB)) = 0;
    return v;
  }
};
