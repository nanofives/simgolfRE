// Events, news and stories, part 3: a golfer's thought and the comment text it produces (see portsim/sim/events.h).
// Phase 3.5, written from the specification (analysis notes, the hook sources as reading reference, vector corpus); the
// original's strings are (pool member, byte offset) pairs and no original address appears.
#define TypeName(t) (GS(g_tileTypeAttrs) + static_cast<unsigned>(t) * 0x30u)

namespace portsim {

void emit_golfer_thought(Ctx &ctx, int32_t g, int32_t kind, int32_t arg) {
    const unsigned r = Rec(g);
    const signed char sub = G<signed char>(ctx, r + 0x22);
    if (sub > 9) return;
    if (g >= 0x98) return;
    if (G<unsigned>(ctx, GS(g_flags)) & 0x2000000u) return;
    if (kind == 0x13 && (G<unsigned char>(ctx, r + 0x18) & 0xe0) != 0x20) {
        const int h = G<signed char>(ctx, r + 0x21);
        const unsigned hs = static_cast<unsigned>(h) * 0x208u;
        if (G<int>(ctx, hs + (GS(g_holes) + 32)) >= 10) {
            const int d = static_cast<int>(G<signed char>(ctx, r + 0x23 + static_cast<unsigned>(h))) -
                          static_cast<int>(G<signed char>(ctx, hs + GS(g_holes)));
            const unsigned char fl = G<unsigned char>(ctx, hs + (GS(g_holes) + 512));
            if (((fl & 4) && d > (SG_NEGCTL(g_negctl_emit_golfer_thought) == 1 ? 2 : 1)) || ((fl & 8) && d < 0)) kind = 0x17;
        }
    }
    build_golfer_comment(ctx, kind, arg, G<signed char>(ctx, r + 0x21) * 11 + static_cast<int>(sub), g);
    kBuildStatusText(g, 0);
    const int pairFlag = G<int>(ctx, GS(g_thoughtOnPartner));
    const unsigned t = Rec(pairFlag ? static_cast<int>(G<short>(ctx, r + 0xa2)) : g);
    G<unsigned char>(ctx, t + 0x84) = 7;
    G<unsigned char>(ctx, t + 0x85) = static_cast<unsigned char>(kind);
    G<unsigned char>(ctx, t + 0x86) = static_cast<unsigned char>(G<unsigned char>(ctx, r + 0x21) * 11 + G<unsigned char>(ctx, r + 0x22));
    G<short>(ctx, t + 0x9c) = static_cast<short>(arg);
    if (pairFlag) G<unsigned char>(ctx, t + 0x85) |= 0x80;
    G<int>(ctx, GS(g_thoughtArgByKind) + static_cast<unsigned>(kind) * 4u) = arg;
    if (kind == 0x23) {
        const int x = G<int>(ctx, r + 0), y = G<int>(ctx, r + 4);
        kPlaySoundAt(kTypeBit7Clear(g) + 0x46, x, y, 0);
        if (G<unsigned char>(ctx, r + 0x70) == 0x23) return;
    } else if (kind == 0x13) {
        return;
    }
    unsigned char stacked[0x100];
    for (unsigned k = 0; k < 0x100; k++) stacked[k] = G<unsigned char>(ctx, r + k);
    for (unsigned i = 9; i > 0; i--) {
        G<unsigned char>(ctx, r + 0x70 + i) = G<unsigned char>(ctx, r + 0x6f + i);
        G<unsigned char>(ctx, r + 0x7a + i) = G<unsigned char>(ctx, r + 0x79 + i);
        G<short>(ctx, r + 0x88 + 2 * i) = G<short>(ctx, r + 0x86 + 2 * i);
    }
    G<unsigned char>(ctx, r + 0x70) = static_cast<unsigned char>(kind);
    G<unsigned char>(ctx, r + 0x7a) = static_cast<unsigned char>(G<unsigned char>(ctx, r + 0x21) * 11 + G<unsigned char>(ctx, r + 0x22));
    G<short>(ctx, r + 0x88) = static_cast<short>(arg);
    if (kind >= 4) G<unsigned char>(ctx, r + 0x7a) += 1;
    int snd = (G<unsigned char>(ctx, (GS(g_golferTypes) + 35) + static_cast<unsigned>(static_cast<int>(G<short>(ctx, r + 0xb6))) * 0x230u) >> 4) & 0xf;
    if (kTypeBit7Clear(g) == 0) snd += 5;
    int delta = 0;
    const int x = G<int>(ctx, r + 0), y = G<int>(ctx, r + 4);
    const int tile = (x >> 10) * 50 + (y >> 10);
    bool pathMark = false;
    bool typeFlagCall = false;
    unsigned char typeFlagBit = 1;
    switch (kind) {
    case 1:
        delta = 1;
        kPlaySoundAt(((G<unsigned>(ctx, r + 0x10) & 0x20000u) ? 0x14 : 0) + 0xd2 + snd, x, y, 0);
        break;
    case 2: case 3: case 8: {
        delta = (kind == 2) ? -1 : -2;
        const int f34 = (kind == 8) ? 0 : (G<unsigned char>(ctx, GS(g_tileType) + static_cast<unsigned>(tile)) != 0x11 ? 0 : 1000);
        kPlaySoundAt(((G<unsigned>(ctx, r + 0x10) & 0x20000u) ? static_cast<int>(0xffffff34u) : 0) + 0xdc + snd, x, y, f34);
        break;
    }
    case 4:     delta = -2; kPlaySoundAt(kTypeBit7Clear(g) + 0x12, x, y, 0); break;
    case 5: case 0x1f: kPlaySoundAt(kTypeBit7Clear(g) + 0x10, x, y, 0); break;
    case 6:     delta = 1; kPlaySoundAt(kTypeBit7Clear(g) + 0x14, x, y, 0); break;
    case 7:
        if (arg == 0) { delta = 1; kPlaySoundAt(kTypeBit7Clear(g) + 0x4c, x, y, 0); }
        break;
    case 9:     delta = -3; kPlaySoundAt(kTypeBit7Clear(g) + 0x46, x, y, 0); break;
    case 0xa: case 0x2b: delta = -2; kPlaySoundAt(kTypeBit7Clear(g) + 0x50, x, y, 0); break;
    case 0xb:   delta = 1; kPlaySoundAt(kTypeBit7Clear(g) + 0x4a, x, y, 0); break;
    case 0xc: case 0xd: delta = -1; kPlaySoundAt(kTypeBit7Clear(g) + 8, x, y, 0x1f4); break;
    case 0xe:   delta = -1; kPlaySoundAt(kTypeBit7Clear(g) + 0x9e, x, y, 0); break;
    case 0xf:   delta = -1; kPlaySoundAt(kTypeBit7Clear(g) + 0x9c, x, y, 0); break;
    case 0x12:
        delta = G<short>(ctx, r + 0xa6) >= 8 ? 1 : 0;
        typeFlagCall = delta != 0;
        break;
    case 0x14:  delta = -2; kPlaySoundAt(kTypeBit7Clear(g) + 0x56, x, y, 0); pathMark = true; break;
    case 0x15:  delta = -2; kPlaySoundAt(kTypeBit7Clear(g) + 0x44, x, y, 0); break;
    case 0x16:  delta = 1; kPlaySoundAt(kTypeBit7Clear(g) + 0x4e, x, y, 0); pathMark = true; break;
    case 0x17:
        delta = G<int>(ctx, GS(g_difficulty)) != 0 ? -2 : -1;
        kPlaySoundAt(kTypeBit7Clear(g) + 0x9a, x, y, 0);
        break;
    case 0x18:  delta = -2; kPlaySoundAt(kTypeBit7Clear(g) + 0x42, x, y, 0); break;
    case 0x19:
        delta = G<short>(ctx, r + 0xa8) >= 8 ? 1 : 0;
        kPlaySoundAt(kTypeBit7Clear(g) + 0x3c, x, y, 0);
        typeFlagCall = delta != 0;
        break;
    case 0x1a:  delta = -1; kPlaySoundAt(kTypeBit7Clear(g) + 0xa0, x, y, 0); break;
    case 0x1b:
        delta = G<short>(ctx, r + 0xaa) >= 0x3c ? 1 : 0;
        kPlaySoundAt(kTypeBit7Clear(g) + 0x48, x, y, 0);
        typeFlagCall = delta != 0;
        break;
    case 0x1c:  delta = 1; kPlaySoundAt(kTypeBit7Clear(g) + 0x4c, x, y, 0); pathMark = true; break;
    case 0x1d:  delta = 1; kPlaySoundAt(kTypeBit7Clear(g) + 0x96, x, y, 0); break;
    case 0x1e:  delta = -2; kPlaySoundAt(kTypeBit7Clear(g) + 0x98, x, y, 0); break;
    case 0x20: case 0x21: delta = 1; kPlaySoundAt(kTypeBit7Clear(g) + 0xa2, x, y, 0); break;
    case 0x22:
        delta = 1; kPlaySoundAt((kTypeBit7Clear(g) != 0 ? 3 : 0) + 0xa4, x, y, 0); break;
    case 0x23:  delta = -2; kPlaySoundAt(kTypeBit7Clear(g) + 0x46, x, y, 0); break;
    case 0x24:  delta = -3; kPlaySoundAt(kTypeBit7Clear(g) + 0x3e, x, y, 0); break;
    case 0x27:
        delta = G<int>(ctx, GS(g_difficulty)) < 2 ? 1 : 0; kPlaySoundAt(kTypeBit7Clear(g) + 0x14, x, y, 0); break;
    case 0x2c: case 0x2e: delta = 1; break;
    case 0x2f:  delta = -2; break;
    case 0x33: case 0x34: case 0x35:
        delta = G<int>(ctx, GS(g_difficulty)) == 0 ? 1 : 0;
        typeFlagCall = true;
        typeFlagBit = 4;
        break;
    case 0x36:
        delta = G<int>(ctx, GS(g_difficulty)) <= 1 ? 1 : 0; break;
    case 0x3a:
        kPlaySoundAt((kTypeBit7Clear(g) != 0 ? 0x4b : 0) + 0x5a, x, y, 0); break;
    case 0x3b:
        if (G<unsigned char>(ctx, static_cast<unsigned>(static_cast<int>(G<short>(ctx, r + 0xb6))) * 44u +
                             static_cast<unsigned>(static_cast<int>(G<signed char>(ctx, r + 0x21))) + (GS(g_golferTypeState) + 3)) != 0) delta = 1;
        break;
    case 0x41:
        if (G<int>(ctx, GS(g_difficulty)) > 1) delta = -2;
        kPlaySoundAt(kTypeBit7Clear(g) + 0x16, x, y, 0);
        break;
    default: break;
    }
    if (pathMark) {
        G<unsigned char>(ctx, r + 0x1d) = 0xe;
        G<unsigned short>(ctx, r + 0x9e) = 0xffe8;
    }
    if (typeFlagCall && (TypeFlags(G<short>(ctx, r + 0xb6)) & typeFlagBit)) (void)0;
    if (kind == 0x28)
        for (unsigned k = 0; k < 0x100; k++) G<unsigned char>(ctx, r + k) = stacked[k];
    if (delta == 0 && kind >= 0x30) return;
    const int switchDelta = delta;
    if (G<unsigned char>(ctx, r + 0x18) != 0) {
        const int a = kSign(G<signed char>(ctx, r + 0x36));
        if (-a == kSign(delta))
            G<unsigned char>(ctx, r + 0x36) = 0;
        else
            G<unsigned char>(ctx, r + 0x36) = static_cast<unsigned char>(G<unsigned char>(ctx, r + 0x36) + delta);
        if (G<unsigned char>(ctx, r + 0x18) != 0 && (kind == 2 || kind == 3 || kind == 0xd || kind == 0xc)) {
            const signed char e = G<signed char>(ctx, r + 0x36);
            if (e < 0 && delta < 0 && static_cast<int>(e) < delta) G<unsigned char>(ctx, r + 0x36) = 1;
        }
        if (g == G<int>(ctx, GS(g_playerGolfer)) && delta < 0 && (G<unsigned>(ctx, r + 0x10) & 0x40000u))
            kPlaySound(0x30, 0x64, 0, 0, 0);
    }
    bool partnerTest = true;
    if (delta == -1) {
        const int mode = G<int>(ctx, GS(unk_00543cf4));
        if (mode != 2 && G<unsigned char>(ctx, r + 0x18) == 0) {
            delta = 0;
            for (int i = 1;; i++) {
                const int c88 = G<int>(ctx, GS(g_difficulty));
                const int lim = c88 == 0 ? 3 : (c88 <= 2 ? 5 : 10);
                if (i >= lim) break;
                if (static_cast<int>(G<unsigned char>(ctx, r + 0x70 + static_cast<unsigned>(i))) == kind) delta = -1;
            }
            if (G<unsigned short>(ctx, r + 0x8a) & 0x8000) delta = -1;
            if (mode == 1) { delta = 0; partnerTest = false; }
            else if (delta >= 0) partnerTest = false;
        }
    } else if (delta >= 0) {
        partnerTest = false;
    }
    if (partnerTest) {
        const unsigned char cls = G<unsigned char>(ctx, r + 0x18) & 0xe0;
        if (cls != 0x40) delta = SG_NEGCTL(g_negctl_emit_golfer_thought) == 3 ? delta / 2 : (delta - 1) / 2;
        if (delta < 0 && cls != 0x20 &&
            G<int>(ctx, GS(g_difficulty)) >= (static_cast<int>((G<unsigned char>(ctx, r + 0x22) + static_cast<unsigned>(g)) & 1) + 2) &&
            kind != 0x2f && kind != 0x15 && kind != 0x18) {
            const int p = G<short>(ctx, r + 0xa2);
            const unsigned pr = Rec(p);
            if (G<signed char>(ctx, pr + 0x21) > 0 && G<unsigned char>(ctx, pr + 0x84) == 0 && G<short>(ctx, pr + 0xa4) > 0) {
                emit_golfer_thought(ctx, p, 0x2f, 0x14);
                G<unsigned char>(ctx, Rec(G<short>(ctx, r + 0xa2)) + 0x84) += 2;
            }
        }
    }
    G<short>(ctx, r + 0xa4) = static_cast<short>(G<short>(ctx, r + 0xa4) + delta);
    G<short>(ctx, r + 0xa4) = static_cast<short>(kClamp(G<short>(ctx, r + 0xa4), SG_NEGCTL(g_negctl_emit_golfer_thought) == 2 ? -9 : -10, 10));
    const int h = G<signed char>(ctx, r + 0x21);
    const unsigned hs = static_cast<unsigned>(h) * 0x208u;
    G<short>(ctx, hs + (GS(g_holes) + 344)) = static_cast<short>(G<short>(ctx, hs + (GS(g_holes) + 344)) + delta);
    const int roll = Rnd(6) & 0xffff;
    if ((roll <= G<int>(ctx, GS(g_difficulty)) || (G<unsigned>(ctx, r + 0x10) & 0x20000000u)) &&
        delta < 0 && (G<unsigned>(ctx, GS(g_flags)) & 0x4000000u) == 0 && kind != 0x2f &&
        kHoleQuadrants(G<int>(ctx, r + 0), G<int>(ctx, r + 4), 2) == 0) {
        const unsigned tl = static_cast<unsigned>((G<int>(ctx, r + 0) >> 10) * 50 + (G<int>(ctx, r + 4) >> 10));
        const unsigned short w = G<unsigned short>(ctx, GS(g_tileFlags) + tl * 2u);
        if ((w & 0xc00) == 0 && G<unsigned char>(ctx, GS(g_tileType) + tl) != 0x11) {
            G<unsigned char>(ctx, GS(tile_anim_frame) + tl) = 1;
            G<unsigned short>(ctx, GS(g_tileFlags) + tl * 2u) = static_cast<unsigned short>(w | 0x4800);
            G<int>(ctx, GS(g_cachedCameraTileX)) = -1;
        }
    }
    if (delta != 0) {
        const unsigned o = static_cast<unsigned>(kind + h * 0x104) * 2u;
        G<short>(ctx, o + (GS(g_holes) + 216)) = static_cast<short>(G<short>(ctx, o + (GS(g_holes) + 216)) + 1);
        G<short>(ctx, o + (GS(g_holes) + 364)) = static_cast<short>(arg);
        if (delta > 0) G<unsigned char>(ctx, r + 0x89) |= 0x40;
    }
    if (switchDelta < 0) G<unsigned short>(ctx, r + 0x88) |= 0x8000;
    if (delta < 0) G<unsigned char>(ctx, r + 0x89) |= 0xc0;
    const unsigned tl = static_cast<unsigned>((G<int>(ctx, r + 0) >> 10) * 50 + (G<int>(ctx, r + 4) >> 10));
    if (delta > 0) G<unsigned char>(ctx, GS(g_tileMoodUp) + tl) += 1;
    if (delta < 0) G<unsigned char>(ctx, GS(g_tileMoodDown) + tl) += 1;
    if (kind == 0x32) return;
    const uint32_t pron = kTypeBit7Clear(g) != 0 ? (GS(f_strpool_4c6d6c) + 4) : GS(f_strpool_4c6d6c);
    unsigned seen;
    if (kind < 0x20) seen = (1u << (kind & 31)) & G<unsigned>(ctx, GS(g_thoughtSeenLo));
    else seen = (1u << ((kind - 0x20) & 31)) & G<unsigned>(ctx, GS(g_thoughtSeenHi));
    if (G<int>(ctx, GS(g_date)) - G<int>(ctx, GS(g_lastTipDate)) <= 0x1f4) return;
    if (G<short>(ctx, r + 0xae) != 0) return;
    if (seen != 0) return;
    const int px = G<int>(ctx, r + 0x08);
    if (px <= 0x64 || px >= 0x2bc) return;
    const int py = G<int>(ctx, r + 0x0c);
    if (py <= 0x64 || py >= 0x190) return;
    const uint32_t buf = text_va(ctx);
    Cpy(buf, Txt(GS(f_strpool_4c4e54)));
    build_golfer_comment(ctx, kind, arg, G<signed char>(ctx, r + 0x21) * 11 + static_cast<int>(G<signed char>(ctx, r + 0x22)), g);
    CatAt(GS(f_strpool_4e28e0));
    kBuildGolferName(g, 0);
    const uint32_t typeName = (GS(g_tileTypeAttrs) + static_cast<unsigned>(arg) * 0x30u);
    switch (kind) {
    case 1: CatAt(GS(f_strpool_4e2894)); break;
    case 2: CatAt((GS(f_strpool_4e27dc) + 52)); Cat(buf, typeName); CatAt((GS(f_strpool_4e27dc) + 28)); Cat(buf, pron); CatAt(GS(f_strpool_4e27dc)); break;
    case 3: CatAt(GS(f_strpool_4e2794)); Cat(buf, typeName); CatAt(GS(f_strpool_4c4944)); break;
    case 4: CatAt((GS(f_strpool_4e248c) + 292)); break;
    case 5: case 0x25: case 0x26: CatAt((GS(f_strpool_4e248c) + 256)); Cat(buf, pron); CatAt((GS(f_strpool_4e248c) + 204)); break;
    case 6: CatAt((GS(f_strpool_4e27dc) + 112)); break;
    case 8: CatAt(GS(f_strpool_4e2718)); break;
    case 9: CatAt((GS(f_strpool_4e2620) + 116)); break;
    case 0xa: CatAt((GS(f_strpool_4e2358) + 152)); Cat(buf, typeName); CatAt((GS(f_strpool_4e2358) + 112)); break;
    case 0xb: CatAt((GS(f_strpool_4e216c) + 268)); kDecorationName(arg % 50, arg / 50, -1); CatAt((GS(f_strpool_4e216c) + 240)); break;
    case 0xc: CatAt((GS(f_strpool_4e216c) + 204)); kBuildGolferName(g, 0); CatAt((GS(f_strpool_4e216c) + 168)); break;
    case 0xd: CatAt((GS(f_strpool_4e216c) + 132)); Cat(buf, typeName); CatAt(GS(f_strpool_4d3414)); kBuildGolferName(g, 0); CatAt((GS(f_strpool_4e216c) + 168)); break;
    case 0xe: CatAt((GS(f_strpool_4e216c) + 92)); Cat(buf, pron); CatAt((GS(f_strpool_4e216c) + 72)); break;
    case 0xf: CatAt((GS(f_strpool_4e2088) + 112)); Cat(buf, pron); CatAt((GS(f_strpool_4e216c) + 72)); break;
    case 0x10: CatAt(GS(f_strpool_4e2088)); break;
    case 0x11: CatAt((GS(f_strpool_4e2088) + 56)); break;
    case 0x12: CatAt(GS(f_strpool_4e2044)); break;
    case 0x14: CatAt((GS(f_strpool_4e1fa8) + 56)); kDecorationName(arg % 50, arg / 50, -1); CatAt(GS(f_strpool_4e1fa8)); break;
    case 0x15: CatAt(GS(f_strpool_4e1e50)); CatAt(GS(f_strpool_4e1e04)); CatAt((GS(f_strpool_4e1e50) + 124)); break;
    case 0x16: CatAt((GS(f_strpool_4e1e50) + 104)); Cat(buf, Txt(GS(g_celebrities) + static_cast<unsigned>(arg) * 37u)); CatAt((GS(f_strpool_4e1e50) + 64)); break;
    case 0x18: CatAt((GS(f_strpool_4e1f18) + 48)); CatAt(GS(f_strpool_4e1f18)); CatAt((GS(f_strpool_4e1e50) + 124)); break;
    case 0x19: CatAt((GS(f_strpool_4e1fa8) + 84)); break;
    case 0x1a: CatAt((GS(f_strpool_4e216c) + 28)); Cat(buf, pron); CatAt(GS(f_strpool_4e216c)); break;
    case 0x1b: CatAt((GS(f_strpool_4e2088) + 148)); break;
    case 0x1c: CatAt((GS(f_strpool_4e2358) + 36)); kDecorationName(arg % 50, arg / 50, -1); CatAt(GS(f_strpool_4e2358)); break;
    case 0x1d: CatAt(GS(f_strpool_4e2310)); break;
    case 0x1e: CatAt((GS(f_strpool_4e216c) + 300)); break;
    case 0x1f: CatAt((GS(f_strpool_4e248c) + 160)); kBuildGolferName(g, 0); CatAt((GS(f_strpool_4e248c) + 108)); break;
    case 0x20: case 0x21: CatAt((GS(f_strpool_4e248c) + 52)); CatAt(GS(f_strpool_4e248c)); break;
    case 0x23: CatAt((GS(f_strpool_4e2620) + 56)); break;
    case 0x24: CatAt(GS(f_strpool_4e2620)); break;
    case 0x27: CatAt((GS(f_strpool_4e2358) + 64)); break;
    case 0x33: CatAt((GS(f_strpool_4e1c30) + 64)); break;
    case 0x34: CatAt((GS(f_strpool_4e1c30) + 128)); break;
    case 0x35: CatAt(GS(f_strpool_4e1c30)); break;
    case 0x36: CatAt((GS(f_strpool_4e1bc8) + 12)); break;
    case 0x37: CatAt(GS(f_strpool_4e1d7c)); break;
    case 0x38: CatAt(GS(f_strpool_4e1dc0)); break;
    case 0x39: CatAt(GS(f_strpool_4e1d2c)); break;
    case 0x3c: CatAt((GS(f_strpool_4e1c30) + 184)); break;
    case 0x41: CatAt(GS(f_strpool_4e2434)); break;
    default: G<char>(ctx, buf) = 0; break;
    }
    unsigned code = 0x80000210u;
    if (switchDelta > 0) code = 0x80000288u;
    if (switchDelta < 0) code = 0x80005084u;
    if (G<int>(ctx, GS(g_difficulty)) != 0 && switchDelta < -1) code = 0x80007084u;
    if ((G<unsigned char>(ctx, GS(g_preferences)) & 4) && G<char>(ctx, buf) != 0 &&
        kStartMessage(static_cast<int>(code), -8, g) != 0) {
        if (kind < 0x20) G<unsigned>(ctx, GS(g_thoughtSeenLo)) |= 1u << (kind & 31);
        else G<unsigned>(ctx, GS(g_thoughtSeenHi)) |= 1u << ((kind - 0x20) & 31);
        G<int>(ctx, GS(g_lastTipDate)) = G<int>(ctx, GS(g_date));
    }
}

void build_golfer_comment(Ctx &ctx, int32_t kind, int32_t arg, int32_t place, int32_t g) {
    const int hole = place / 11;
    const unsigned r = Rec(g);
    const uint32_t buf = text_va(ctx);
    G<int>(ctx, GS(g_thoughtOnPartner)) = 0;
    bool done = false;
    if (g == 0x98 && SG_NEGCTL(g_negctl_build_golfer_comment) != 3) {
        for (unsigned k = 0; k < 0x100; k += 4) G<unsigned>(ctx, (GS(g_golfers) + 38912) + k) = 0;
    } else if (g < 0x98) {
        const int type = G<short>(ctx, r + 0xb6);
        for (int n = 0;; n++) {
            const signed char e = G<signed char>(ctx, GS(f_tab_bytes_4c2d10) + static_cast<unsigned>(n));
            if (static_cast<int>(e) == kind) {
                const unsigned s = GS(g_golferPhrases) + static_cast<unsigned>((type * 25 + n) * 50);
                if (G<char>(ctx, s) != 0) { Cat(buf, Txt(s)); done = true; break; }
            }
            if (G<unsigned char>(ctx, (GS(f_tab_bytes_4c2d10) + 1) + static_cast<unsigned>(n)) == 0xff) break;
        }
    }
    if (!done) {
        G<unsigned>(ctx, GS(g_commentTone)) = 0x80006318u;
        const unsigned char cls = G<unsigned char>(ctx, r + 0x18) & 0xe0;
        const short pair = G<short>(ctx, r + 0xae);
        const int t3 = G<unsigned char>(ctx, r + 0xb6) & 3;
        const bool odd = (g & 1) != 0;
        switch (kind) {
        case 1: {
            const int bits = (SG_NEGCTL(g_negctl_build_golfer_comment) == 1 ? 3 - t3 : t3) << 5;
            if (G<unsigned char>(ctx, r + 0xae) & 1) {
                CatAt((GS(f_strpool_4d3954) + 8));
                const int p = G<short>(ctx, r + 0xa2);
                unsigned piece;
                if (G<unsigned char>(ctx, Rec(p) + 0xb6) & 1)
                    piece = kTypeBit7Clear(p) != 0 ? (GS(f_strpool_4e28e0) + 4000) : (GS(f_strpool_4e28e0) + 4024);
                else
                    piece = kTypeBit7Clear(p) != 0 ? (GS(f_strpool_4e28e0) + 3944) : (GS(f_strpool_4e28e0) + 3976);
                G<int>(ctx, GS(g_thoughtOnPartner)) = 1;
                G<unsigned>(ctx, GS(g_commentTone)) = 0x800023e8u;
                CatAt(piece);
            } else {
                unsigned piece;
                if (bits & 0x20) piece = (bits & 0x40) ? (GS(f_strpool_4e28e0) + 3924) : (GS(f_strpool_4e28e0) + 3904);
                else if ((bits & 0x40) || g == -1) piece = (GS(f_strpool_4e28e0) + 3836);
                else { CatAt((GS(f_strpool_4e28e0) + 3876)); piece = (GS(f_strpool_4e28e0) + 3864); }
                G<unsigned>(ctx, GS(g_commentTone)) = 0x800023e8u;
                CatAt(piece);
            }
            break;
        }
        case 2:
            if (pair == 2) {
                CatAt((GS(f_strpool_4e28e0) + 3788)); kBuildGolferName(g, 0); CatAt((GS(f_strpool_4e28e0) + 3772));
                G<int>(ctx, GS(g_thoughtOnPartner)) = 1; G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u;
            } else if (pair == 4) {
                CatAt((GS(f_strpool_4e28e0) + 3796)); kAppendEndearment(g); CatAt(GS(f_strpool_4c4e60));
                G<int>(ctx, GS(g_thoughtOnPartner)) = 1; G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u;
            } else {
                G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u; CatAt((GS(f_strpool_4e28e0) + 3820));
            }
            break;
        case 3:
            if (pair == 2) {
                CatAt((GS(f_strpool_4e28e0) + 3640)); CatAt(GS(f_strpool_4c3f70)); kBuildGolferName(g, 0); CatAt(GS(f_strpool_4c4944));
                G<int>(ctx, GS(g_thoughtOnPartner)) = 1; G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u;
            } else if (pair == 3) {
                CatAt((GS(f_strpool_4e28e0) + 3688)); kBuildGolferName(g, 0); CatAt((GS(f_strpool_4e28e0) + 3668));
                G<int>(ctx, GS(g_thoughtOnPartner)) = 1; G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u;
            } else if (pair == 4) {
                CatAt((GS(f_strpool_4e28e0) + 3692));
                G<int>(ctx, GS(g_thoughtOnPartner)) = 1; G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u;
            } else {
                const unsigned kByType[4] = { (GS(f_strpool_4e28e0) + 3764), (GS(f_strpool_4e28e0) + 3756), (GS(f_strpool_4e28e0) + 3748), (GS(f_strpool_4e28e0) + 3740) };
                CatAt(kByType[t3]);
                G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u; CatAt((GS(f_strpool_4e28e0) + 3724));
            }
            break;
        case 4:
            if (pair == 4) {
                CatAt((GS(f_strpool_4e28e0) + 4512)); kBuildGolferName(g, 0);
                G<int>(ctx, GS(g_thoughtOnPartner)) = 1; G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u;
            } else {
                G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u; CatAt((GS(f_strpool_4e28e0) + 4480));
            }
            break;
        case 5: case 0x25: case 0x26:
            switch (pair) {
            case 0: case 4:
                if (kind == 5) CatAt((GS(f_strpool_4e28e0) + 4196));
                else if (kind == 0x25) CatAt((GS(f_strpool_4e28e0) + 4232));
                else CatAt((GS(f_strpool_4e28e0) + 4264));
                CatAt((GS(f_strpool_4e28e0) + 4188));
                break;
            case 3: case 5:
                CatAt((GS(f_strpool_4e28e0) + 4164)); kBuildGolferName(g, 1);
                G<int>(ctx, GS(g_thoughtOnPartner)) = 1;
                break;
            case 1:
                CatAt((GS(f_strpool_4e28e0) + 4144)); kAppendEndearment(g);
                G<int>(ctx, GS(g_thoughtOnPartner)) = 1;
                break;
            case 2:
                if (odd) CatAt((GS(f_strpool_4e28e0) + 4116));
                else { CatAt((GS(f_strpool_4e28e0) + 4092)); CatAt((GS(f_strpool_4e28e0) + 4084)); }
                CatAt(GS(f_strpool_4c3f70)); kBuildGolferName(g, 0);
                G<int>(ctx, GS(g_thoughtOnPartner)) = 1;
                break;
            default: break;
            }
            CatAt(GS(f_strpool_4c4944));
            break;
        case 6: G<unsigned>(ctx, GS(g_commentTone)) = 0x800023e8u; CatAt((GS(f_strpool_4e28e0) + 3572)); break;
        case 7:
            if (arg == 0) {
                Cpy(buf, Txt((G<unsigned>(ctx, r + 0x10) & 0x10000u) ? (GS(f_strpool_4e28e0) + 1888) : (GS(f_strpool_4e28e0) + 1848)));
                G<unsigned>(ctx, GS(g_commentTone)) = 0x800023e8u;
            } else {
                Cpy(buf, Txt((GS(f_strpool_4e28e0) + 1808)));
            }
            break;
        case 8: G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u; CatAt((GS(f_strpool_4e28e0) + 3608)); break;
        case 9: G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u; CatAt((GS(f_strpool_4e28e0) + 3540)); break;
        case 0xa: {
            CatAt((GS(f_strpool_4e28e0) + 3516));
            const uint32_t name = TypeName(arg);
            const unsigned n = portsim::ev::slen(ctx, name);
            const unsigned piece = G<char>(ctx, name + static_cast<unsigned>(static_cast<int>(n) - 1)) == 's' ? (GS(f_strpool_4e28e0) + 3500) : (GS(f_strpool_4e28e0) + 3508);
            G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u;
            CatAt(piece); Cat(buf, name); CatAt(GS(f_strpool_4c5c0c));
            break;
        }
        case 0xb: {
            CatAt(pair == 1 ? (GS(f_strpool_4e28e0) + 1644) : (GS(f_strpool_4e28e0) + 1660));
            CatAt((arg & 0x100) ? (GS(f_strpool_4e28e0) + 1636) : (GS(f_strpool_4e28e0) + 1624));
            CatAt((GS(f_strpool_4e28e0) + 4188));
            if (pair == 1) kAppendEndearment(G<short>(ctx, r + 0xa2));
            else CatAt(GS(f_strpool_4c4e60));
            G<unsigned>(ctx, GS(g_commentTone)) = 0x800023e8u;
            break;
        }
        case 0xc: G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u; CatAt((GS(f_strpool_4e28e0) + 1600)); break;
        case 0xd:
            if (pair == 2 || pair == 4) {
                CatAt((GS(f_strpool_4e28e0) + 1500)); kBuildGolferName(g, 0); CatAt(GS(f_strpool_4c5c0c));
                G<int>(ctx, GS(g_thoughtOnPartner)) = 1;
            } else {
                const int m = static_cast<int>(G<short>(ctx, r + 0xb6)) % 3;
                if (m == 0) CatAt((GS(f_strpool_4e28e0) + 1524));
                else if (m == 1) CatAt((GS(f_strpool_4e28e0) + 1544));
                else if (m == 2) CatAt((GS(f_strpool_4e28e0) + 1572));
            }
            G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u;
            break;
        case 0xe: G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u; CatAt((GS(f_strpool_4e28e0) + 1468)); break;
        case 0xf: G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u; CatAt((GS(f_strpool_4e28e0) + 1440)); break;
        case 0x10: CatAt((GS(f_strpool_4e28e0) + 1356)); break;
        case 0x11: CatAt((GS(f_strpool_4e28e0) + 1320)); break;
        case 0x12: G<unsigned>(ctx, GS(g_commentTone)) = 0x800023e8u; CatAt((GS(f_strpool_4e28e0) + 1284)); break;
        case 0x13: case 0x17: {
            const unsigned hs = static_cast<unsigned>(hole) * 0x208u;
            const signed char own = G<signed char>(ctx, r + static_cast<unsigned>(hole) + 0x23);
            const int d = static_cast<int>(own) - static_cast<int>(G<signed char>(ctx, hs + GS(g_holes)));
            const unsigned kByDiff[8] = { (GS(f_strpool_4e28e0) + 788), (GS(f_strpool_4e28e0) + 772), (GS(f_strpool_4e28e0) + 764), (GS(f_strpool_4e28e0) + 752),
                                                 (GS(f_strpool_4e28e0) + 744), (GS(f_strpool_4e28e0) + 736), (GS(f_strpool_4e28e0) + 720), (GS(f_strpool_4e28e0) + 704) };
            CatAt(static_cast<unsigned>(d + 4) <= 7 ? kByDiff[d + 4] : (GS(f_strpool_4e28e0) + 696));
            const unsigned fl = G<unsigned>(ctx, hs + (GS(g_holes) + 512));
            if ((fl & 4) && (d > 1 || g == 0x98)) {
                G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u; CatAt((GS(f_strpool_4e28e0) + 672));
                break;
            }
            if ((fl & 8) && (d < 0 || g == 0x98)) {
                G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u; CatAt((GS(f_strpool_4e28e0) + 648));
                break;
            }
            if (kind == 0x17) {
                G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u; CatAt((GS(f_strpool_4e28e0) + 620));
                break;
            }
            unsigned want = 0;
            const unsigned char d1 = G<unsigned char>(ctx, r + 0x19);
            switch ((g + hole) & 3) {
            case 0: if (d1 & 1) want = 0x100; break;
            case 1: if (d1 & 2) want = 0x200; break;
            case 2: if (d1 & 4) want = 0x400; break;
            default: break;
            }
            if ((want & fl) && d <= 0) {
                CatAt(want == 0x100 ? (GS(f_strpool_4e28e0) + 512) : (want == 0x200 ? (GS(f_strpool_4e28e0) + 548) : (GS(f_strpool_4e28e0) + 580)));
                break;
            }
            if (pair == 2 || pair == 4) {
                const signed char theirs = G<signed char>(ctx, Rec(G<short>(ctx, r + 0xa2)) + static_cast<unsigned>(hole) + 0x23);
                if (theirs != 0) {
                    CatAt(own < theirs ? (GS(f_strpool_4e28e0) + 492) : (GS(f_strpool_4e28e0) + 460));
                    break;
                }
            }
            kAppendOpinion(arg, d);
            break;
        }
        case 0x14: G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u; CatAt((GS(f_strpool_4e28e0) + 3440)); CatAt(GS(f_strpool_4c4944)); break;
        case 0x15: G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u; CatAt((GS(f_strpool_4e28e0) + 3360)); break;
        case 0x16:
            G<unsigned>(ctx, GS(g_commentTone)) = 0x800023e8u;
            CatAt((GS(f_strpool_4e28e0) + 3348)); Cat(buf, Txt(GS(g_celebrities) + static_cast<unsigned>(arg) * 37u)); CatAt((GS(f_strpool_4e28e0) + 3336));
            break;
        case 0x18: G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u; CatAt((GS(f_strpool_4e28e0) + 3400)); break;
        case 0x19: G<unsigned>(ctx, GS(g_commentTone)) = 0x800023e8u; CatAt((GS(f_strpool_4e28e0) + 1252)); break;
        case 0x1a:
            if (pair & 1) {
                CatAt((GS(f_strpool_4e28e0) + 1416));
                if (pair == 1) kAppendEndearment(g);
                else kBuildGolferName(G<short>(ctx, r + 0xa2), 1);
                G<int>(ctx, GS(g_thoughtOnPartner)) = 1;
                CatAt(GS(f_strpool_4c4944));
            } else {
                CatAt((GS(f_strpool_4e28e0) + 1388));
            }
            break;
        case 0x1b: G<unsigned>(ctx, GS(g_commentTone)) = 0x800023e8u; CatAt((GS(f_strpool_4e28e0) + 1220)); break;
        case 0x1c:
            if (pair == 1 || pair == 3 || pair == 5) {
                CatAt((GS(f_strpool_4e28e0) + 1676));
                if (pair == 1) kAppendEndearment(G<short>(ctx, r + 0xa2));
                else kBuildGolferName(G<short>(ctx, r + 0xa2), 1);
                G<unsigned>(ctx, GS(g_commentTone)) = 0x800023e8u;
                CatAt(GS(f_strpool_4c4944));
            } else {
                const bool clear = kTypeBit7Clear(g) != 0;
                const bool bit0 = (G<unsigned char>(ctx, r + 0xb6) & 1) != 0;
                G<unsigned>(ctx, GS(g_commentTone)) = 0x800023e8u;
                if (clear) CatAt(bit0 ? (GS(f_strpool_4e28e0) + 1780) : (GS(f_strpool_4e28e0) + 1756));
                else CatAt(bit0 ? (GS(f_strpool_4e28e0) + 1732) : (GS(f_strpool_4e28e0) + 1704));
            }
            break;
        case 0x1d: G<unsigned>(ctx, GS(g_commentTone)) = 0x800023e8u; CatAt((GS(f_strpool_4e28e0) + 3144)); break;
        case 0x1e: {
            const unsigned hs = static_cast<unsigned>(hole) * 0x208u;
            const unsigned fl = G<unsigned>(ctx, hs + (GS(g_holes) + 512));
            unsigned piece;
            if ((fl & 0x20) && (G<unsigned char>(ctx, hs + (GS(g_ballRecords) + 9208)) & 0x20)) piece = (GS(f_strpool_4e28e0) + 3116);
            else if ((fl & 0x40) && (G<unsigned char>(ctx, hs + (GS(g_ballRecords) + 9208)) & 0x40)) piece = (GS(f_strpool_4e28e0) + 3088);
            else {
                const signed char a = G<signed char>(ctx, hs + GS(g_holes));
                if (a == G<signed char>(ctx, hs + (GS(g_ballRecords) + 8696))) {
                    CatAt((GS(f_strpool_4e28e0) + 3064));
                    Cat(buf, portsim::ev::itoa_to(ctx, a, GS(g_numberTextBuf), 10));
                    piece = GS(f_strpool_4c4944);
                } else {
                    piece = (GS(f_strpool_4e28e0) + 3028);
                }
            }
            G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u;
            CatAt(piece);
            break;
        }
        case 0x1f: {
            const unsigned kByType[4] = { (GS(f_strpool_4e28e0) + 4372), (GS(f_strpool_4e28e0) + 4456), (GS(f_strpool_4e28e0) + 4400), (GS(f_strpool_4e28e0) + 4424) };
            CatAt(kByType[t3]);
            break;
        }
        case 0x20: G<unsigned>(ctx, GS(g_commentTone)) = 0x800023e8u; CatAt((GS(f_strpool_4e28e0) + 4336)); break;
        case 0x21: G<unsigned>(ctx, GS(g_commentTone)) = 0x800023e8u; CatAt((GS(f_strpool_4e28e0) + 4300)); break;
        case 0x22:
            if (arg == 0) CatAt((GS(f_strpool_4e28e0) + 2388));
            else if (arg == 1) CatAt((GS(f_strpool_4e28e0) + 2416));
            else if (arg == 2) CatAt((GS(f_strpool_4e28e0) + 2444));
            G<unsigned>(ctx, GS(g_commentTone)) = 0x800023e8u;
            break;
        case 0x23: {
            const int v = G<int>(ctx, GS(g_date)) + g * 45;
            const unsigned kBySeason[4] = { (GS(f_strpool_4e28e0) + 2352), (GS(f_strpool_4e28e0) + 2304), (GS(f_strpool_4e28e0) + 2268), (GS(f_strpool_4e28e0) + 2232) };
            CatAt(kBySeason[(v / 80) & 3]);
            G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u;
            break;
        }
        case 0x24:
            G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u;
            CatAt(arg != 0 ? (GS(f_strpool_4e28e0) + 2208) : (GS(f_strpool_4e28e0) + 2180));
            break;
        case 0x27:
            G<unsigned>(ctx, GS(g_commentTone)) = 0x800023e8u;
            CatAt(kTypeBit7Clear(g) != 0 ? (GS(f_strpool_4e28e0) + 1968) : (GS(f_strpool_4e28e0) + 1928));
            break;
        case 0x28: {
            const int m = static_cast<int>(G<short>(ctx, r + 0xb6)) % 8;
            const unsigned kByType8[8] = { (GS(f_strpool_4e28e0) + 2168), (GS(f_strpool_4e28e0) + 2156), (GS(f_strpool_4e28e0) + 2148), (GS(f_strpool_4e28e0) + 2136),
                                                  (GS(f_strpool_4e28e0) + 2124), (GS(f_strpool_4e28e0) + 2112), (GS(f_strpool_4e28e0) + 2100), (GS(f_strpool_4e28e0) + 2088) };
            if (static_cast<unsigned>(m) <= 7) CatAt(kByType8[m]);
            if (arg > 0) {
                CatAt((GS(f_strpool_4c4970) + 4)); kDecorationName(0, 0, arg); CatAt(GS(f_strpool_4c59e0));
            }
            break;
        }
        case 0x29:
            CatAt(odd ? (GS(f_strpool_4e28e0) + 2024) : (GS(f_strpool_4e28e0) + 2004)); break;
        case 0x2a: CatAt((GS(f_strpool_4e28e0) + 2072)); kBuildGolferName(g, 0); G<int>(ctx, GS(g_thoughtOnPartner)) = 1; break;
        case 0x2b: G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u; CatAt((GS(f_strpool_4e28e0) + 3464)); break;
        case 0x2c:
            CatAt((GS(f_strpool_4e28e0) + 2052)); kDecorationName(0, 0, arg);
            G<unsigned>(ctx, GS(g_commentTone)) = 0x800023e8u; CatAt(GS(f_strpool_4c59e0));
            break;
        case 0x2d: CatAt((GS(f_strpool_4e28e0) + 4700)); break;
        case 0x2e: G<unsigned>(ctx, GS(g_commentTone)) = 0x800023e8u; CatAt((GS(f_strpool_4e28e0) + 4672)); break;
        case 0x2f: {
            const unsigned kByType[4] = { (GS(f_strpool_4e28e0) + 4640), (GS(f_strpool_4e28e0) + 4608), (GS(f_strpool_4e28e0) + 4580), (GS(f_strpool_4e28e0) + 4544) };
            CatAt(kByType[t3]);
            G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u;
            break;
        }
        case 0x30: CatAt((GS(f_strpool_4e28e0) + 812)); break;
        case 0x31:
            CatAt((GS(f_strpool_4e28e0) + 804));
            Cat(buf, Txt(GS(g_golferTypes) + static_cast<unsigned>(static_cast<int>(G<short>(ctx, r + 0xb6))) * 0x230u));
            CatAt(GS(f_strpool_4c4944));
            break;
        case 0x32:
            G<char>(ctx, buf) = 0;
            load_story_strings(ctx, G<short>(ctx, r + 0xb0), arg & 0xf, arg >> 4, static_cast<int>((G<unsigned>(ctx, r + 0x10) >> 20) & 1));
            break;
        case 0x33: case 0x34: case 0x35: {
            const int v = G<int>(ctx, GS(g_date)) + g * 5;
            const unsigned kOff[3][4] = {
                { (GS(f_strpool_4e28e0) + 1100), (GS(f_strpool_4e28e0) + 1128), (GS(f_strpool_4e28e0) + 1156), (GS(f_strpool_4e28e0) + 1188) },
                { (GS(f_strpool_4e28e0) + 840), (GS(f_strpool_4e28e0) + 872), (GS(f_strpool_4e28e0) + 896), (GS(f_strpool_4e28e0) + 924) },
                { (GS(f_strpool_4e28e0) + 980), (GS(f_strpool_4e28e0) + 1008), (GS(f_strpool_4e28e0) + 1036), (GS(f_strpool_4e28e0) + 1068) } };
            const unsigned kSel[3] = { (GS(g_objectTypeLevel) + 40), (GS(g_objectTypeLevel) + 32), (GS(g_objectTypeLevel) + 24) };
            const int k = kind - 0x33;
            if ((v & 8) == 0) CatAt(kOff[k][0]);
            else {
                const int sel = G<int>(ctx, kSel[k]);
                if (sel >= 1 && sel <= 3) CatAt(kOff[k][sel]);
            }
            break;
        }
        case 0x36:
            CatAt((GS(f_strpool_4e28e0) + 964)); kAppendClubName(arg); CatAt((GS(f_strpool_4e28e0) + 948));
            if (G<int>(ctx, GS(g_difficulty)) <= 1) G<unsigned>(ctx, GS(g_commentTone)) = 0x800023e8u;
            break;
        case 0x37: CatAt((GS(f_strpool_4e28e0) + 3256)); break;
        case 0x38: CatAt((GS(f_strpool_4e28e0) + 3296)); break;
        case 0x39: CatAt((GS(f_strpool_4e28e0) + 3216)); break;
        case 0x3a:
            if (arg == 0) CatAt((GS(f_strpool_4e28e0) + 2940));
            else if (arg == 1) CatAt((GS(f_strpool_4e28e0) + 2976));
            else if (arg == 2) CatAt((GS(f_strpool_4e28e0) + 2996));
            break;
        case 0x3b: {
            const unsigned tt = static_cast<unsigned>(static_cast<int>(G<short>(ctx, r + 0xb6))) * 44u;
            if (G<unsigned char>(ctx, tt + static_cast<unsigned>(hole) + (GS(g_golferTypeState) + 3)) != 0) {
                G<unsigned>(ctx, GS(g_commentTone)) = 0x800023e8u; CatAt((GS(f_strpool_4e28e0) + 2608));
                break;
            }
            const unsigned fl = G<unsigned>(ctx, static_cast<unsigned>(hole) * 0x208u + (GS(g_holes) + 512));
            if (fl & 1) {
                if (fl & 2) { CatAt((GS(f_strpool_4e28e0) + 2572)); break; }
                CatAt((GS(f_strpool_4e28e0) + 2560)); kBuildGolferName(g ^ 1, 0);
                CatAt((GS(f_strpool_4c528c) + 44));
                if (G<unsigned char>(ctx, r + 0xb6) & 1) {
                    kAppendHoleName(hole); CatAt((GS(f_strpool_4e28e0) + 2540));
                } else {
                    CatAt((GS(f_strpool_4e28e0) + 2516)); kAppendHoleName(hole); CatAt(GS(f_strpool_4c4944));
                }
            } else {
                CatAt((GS(f_strpool_4e28e0) + 2504));
                Cat(buf, portsim::ev::itoa_to(ctx, G<signed char>(ctx, tt + static_cast<unsigned>(hole) + (GS(g_golferTypeState) + 22)), GS(g_numberTextBuf), 10));
                CatAt((GS(f_strpool_4e28e0) + 2476));
            }
            break;
        }
        case 0x3c: {
            CatAt((GS(f_strpool_4e28e0) + 3192));
            if (G<unsigned char>(ctx, TypeName(arg) + 0x26) == 0xd) {
                CatAt((GS(f_strpool_4e28e0) + 3180)); Cat(buf, TypeName(arg));
            }
            CatAt(GS(f_strpool_4c4944));
            break;
        }
        case 0x3d:
            switch (arg) {
            case 3:
                CatAt(odd ? (GS(f_strpool_4e28e0) + 2912) : (GS(f_strpool_4e28e0) + 2904)); break;
            case 4:
                CatAt(odd ? (GS(f_strpool_4e28e0) + 2880) : (GS(f_strpool_4e28e0) + 2844)); break;
            case 5:
                CatAt(odd ? (GS(f_strpool_4e28e0) + 2808) : (GS(f_strpool_4e28e0) + 2796)); break;
            case 6:
                CatAt(odd ? (GS(f_strpool_4e28e0) + 2724) : (GS(f_strpool_4e28e0) + 2756)); break;
            case 7:
                CatAt(odd ? (GS(f_strpool_4e28e0) + 2700) : (GS(f_strpool_4e28e0) + 2680)); break;
            default:
                CatAt(odd ? (GS(f_strpool_4e28e0) + 2656) : (GS(f_strpool_4e28e0) + 2640)); break;
            }
            break;
        case 0x3e: {
            int idx = G<signed char>(ctx, (GS(g_golferTypes) + 34) + static_cast<unsigned>(static_cast<int>(G<short>(ctx, r + 0xb6))) * 0x230u);
            if (kTypeBit7Clear(g) == 0) idx += 0x14;
            CatAt((GS(f_tab_quotes_4d55e8) + 4) + static_cast<unsigned>(idx) * 68u);
            break;
        }
        case 0x3f: {
            const unsigned kA[4] = { (GS(f_strpool_4e28e0) + 5016), (GS(f_strpool_4e28e0) + 4976), (GS(f_strpool_4e28e0) + 4940), (GS(f_strpool_4e28e0) + 4904) };
            const unsigned kB[4] = { (GS(f_strpool_4e28e0) + 4868), (GS(f_strpool_4e28e0) + 4824), (GS(f_strpool_4e28e0) + 4772), (GS(f_strpool_4e28e0) + 4728) };
            CatAt(cls == 0x20 ? kA[g & 3] : kB[g & 3]);
            break;
        }
        case 0x41:
            CatAt((GS(f_strpool_4e28e0) + 4044));
            if (G<int>(ctx, GS(g_difficulty)) > 1) G<unsigned>(ctx, GS(g_commentTone)) = 0x80007d08u;
            break;
        default: break;
        }
    }
    if (arg == -1) return;
    std::vector<uint8_t> own, third, other, saved;
    saved = portsim::ev::grab(ctx, buf); G<char>(ctx, buf) = 0;
    kBuildGolferName(g, 0);
    own = portsim::ev::grab(ctx, buf); G<char>(ctx, buf) = 0;
    kBuildGolferName(g ^ 1, 0);
    other = portsim::ev::grab(ctx, buf); G<char>(ctx, buf) = 0;
    if (SG_NEGCTL(g_negctl_build_golfer_comment) == 2) own.swap(other);
    if (g > 0x98) CatAt((GS(f_strpool_4e28e0) + 452));
    switch (kind) {
    case 2: case 3:
        CatAt(G<unsigned char>(ctx, TypeName(arg) + 0x26) == 0xd ? (GS(f_strpool_4e28e0) + 432) : (GS(f_strpool_4e28e0) + 444));
        Cat(buf, TypeName(arg));
        break;
    case 0xb: case 0x14: case 0x1c: kDecorationName(arg % 50, arg / 50, -1); break;
    case 0xc: Cat(buf, TypeName(arg)); break;
    case 0x27: Cat(buf, Txt(GS(f_tab_animals_4c1998) + static_cast<unsigned>(arg) * 18u)); break;
    default:
        Cat(buf, Txt(G<unsigned char>(ctx, TypeName(arg) + 0x26) == 0xd ? TypeName(arg) + 0x10 : TypeName(arg)));
        break;
    }
    if (g > 0x98) CatAt(GS(f_strpool_4c59e0));
    third = portsim::ev::grab(ctx, buf);
    portsim::ev::put(ctx, buf, saved);
    portsim::ev::replace_with(ctx, (GS(f_strpool_4d3954) + 8), own);
    portsim::ev::replace_with(ctx, GS(f_strpool_4d3954), other);
    portsim::ev::replace_with(ctx, (GS(f_strpool_4e28e0) + 4188), third);
}

}  // namespace portsim
