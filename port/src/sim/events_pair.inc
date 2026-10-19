// Events, news and stories, part 5: pairing two golfers with a story (pairGolfersWithStory).
// Phase 3.5, written from the specification (analysis notes, the hook source as reading reference, vector corpus); the
// original's strings are (pool member, byte offset) pairs and no original address appears.

namespace portsim {

namespace {
// negative controls and helpers of pairGolfersWithStory
//   1 = the compatibility count looks at 4 of the 5 type-flag bits, 2 = the story reset threshold is roll > 7
int g_negctl_pair_golfers = 0;

// the CRT's toupper in the C locale on a sign-extended byte
int toupper_c(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }
// one letter of a story file name, upper-cased
int Up(Ctx &ctx, uint32_t name, int k) { return toupper_c(sx8(G<uint8_t>(ctx, name + static_cast<uint32_t>(k)))); }
#define RecTypeFlags(r) TypeFlags(G<short>(ctx, (r) + 0xb6))
// letters 2 and 6: the flag byte (+0xb8) of the golfer; false = the candidate is rejected
bool FlagLetter(Ctx &ctx, int c, uint32_t r) {
  const uint8_t f = G<uint8_t>(ctx, r + 0xb8);
  switch (c) {
    case 'D': return (f & 0x20) != 0;
    case 'M': return (f & 0x10) != 0;
    case 'N': return (f & 0x10) == 0;
    case 'S': return (f & 0x08) != 0;
    case 'W': return (f & 0x40) != 0;
    default: return true;
  }
}
// letters 3 and 7: the type flags of the golfer; false = rejected
bool TypeLetter(Ctx &ctx, int c, uint32_t r) {
  const uint8_t t = RecTypeFlags(r);
  switch (c) {
    case 'T': return (t & 0x01) != 0;
    case 'M': return (t & 0x01) == 0;
    case 'O': return (t & 0x02) != 0;
    case 'S': return (t & 0x02) == 0;
    case 'A': return (t & 0x04) != 0;
    case 'L': return (t & 0x04) == 0;
    case 'P': return (t & 0x08) != 0;
    case 'B': return (t & 0x08) == 0;
    case 'N': return (t & 0x10) != 0;
    case 'G': return (t & 0x10) == 0;
    default: return true;
  }
}
// letters 1 and 5: typeBit7Clear of the golfer (F / M) or of both (O / S, p1 first); false = rejected
bool BitLetter(Ctx &ctx, int c, int g, int p1, int p2) {
  switch (c) {
    case 'F': return kTypeBit7Clear(g) == 0;
    case 'M': return kTypeBit7Clear(g) != 0;
    case 'O': {
      const int a = kTypeBit7Clear(p1);
      return a != kTypeBit7Clear(p2);
    }
    case 'S': {
      const int a = kTypeBit7Clear(p1);
      return a == kTypeBit7Clear(p2);
    }
    default: return true;
  }
}
}  // namespace

int32_t pair_golfers_with_story(Ctx &ctx, int32_t p1, int32_t p2) {
    const uint32_t buf = text_va(ctx);
    const unsigned r1 = Rec(p1), r2 = Rec(p2);
    G<unsigned char>(ctx, r1 + 0x21) = 1;
    G<short>(ctx, r1 + 0xa2) = static_cast<short>(p2);
    G<unsigned char>(ctx, r2 + 0x21) = 1;
    G<short>(ctx, r2 + 0xa2) = static_cast<short>(p1);
    const int date = G<int>(ctx, GS(g_date));
    G<int>(ctx, r2 + 0xc0) = date;
    G<int>(ctx, r1 + 0xc0) = date;
    int seq = G<int>(ctx, GS(g_nextArrivalOrder));
    G<short>(ctx, r1 + 0xbe) = static_cast<short>(seq);
    seq++;
    G<short>(ctx, r2 + 0xbe) = static_cast<short>(seq);
    seq++;
    G<int>(ctx, GS(g_nextArrivalOrder)) = seq;
    const int r6 = Rnd(6);
    G<short>(ctx, r1 + 0x9e) = static_cast<short>(-6 - r6);
    G<short>(ctx, r2 + 0x9e) = 0;
    G<short>(ctx, r2 + 0xb0) = -1;
    G<short>(ctx, r1 + 0xb0) = -1;
    G<short>(ctx, r2 + 0xae) = 0;
    G<short>(ctx, r1 + 0xae) = 0;
    G<unsigned>(ctx, r1 + 0x10) &= 0xffefffffu;
    G<unsigned>(ctx, r2 + 0x10) &= 0xffefffffu;
    if (G<int>(ctx, (GS(g_activeObjectLevel) + 44)) != 0) {
        G<unsigned>(ctx, r1 + 0x10) |= 0x10000u;
        G<unsigned>(ctx, r2 + 0x10) |= 0x10000u;
    }
    const int diff = static_cast<int>(static_cast<signed char>(RecTypeFlags(r2))) ^
                     static_cast<int>(static_cast<signed char>(RecTypeFlags(r1)));
    int compat = 5;
    for (int i = 0; i < (SG_NEGCTL(g_negctl_pair_golfers) == 1 ? 4 : 5); i++) if (diff & (1 << i)) compat--;
    int recFor0x98 = 5;
    for (int attempt = 0; attempt < 3; attempt++) {
        Rnd(100);
        int draws = 0;
        unsigned idx;
        for (;;) {
            idx = static_cast<unsigned>(Rnd(100)) & 0xffffu;
            draws++;
            if (draws >= 100) break;
            if (G<unsigned char>(ctx, GS(g_storyDefs) + idx * 50u) == 0) continue;
            if (static_cast<int>(idx) == G<int>(ctx, GS(g_namedStoryIndex))) continue;
            break;
        }
        const bool found = draws < 100;
        const unsigned name = GS(g_storyDefs) + idx * 50u;
        Up(ctx, name, 0);
        if (!found) continue;
        if (!BitLetter(ctx, Up(ctx, name, 1), p1, p1, p2)) continue;
        if (!FlagLetter(ctx, Up(ctx, name, 2), r1)) continue;
        if (!TypeLetter(ctx, Up(ctx, name, 3), r1)) continue;
        const int c4 = Up(ctx, name, 4);
        if (c4 == 'O') {
            const int s2 = portsim::ev::golfer_score(ctx, p2);
            const int s1 = portsim::ev::golfer_score(ctx, p1);
            if (s1 < s2 + 10) continue;
        } else if (c4 == 'S') {
            const int s1 = portsim::ev::golfer_score(ctx, p1);
            const int s2 = portsim::ev::golfer_score(ctx, p2);
            int d = s1 - s2;
            d = (d ^ (d >> 31)) - (d >> 31);
            const int s3 = portsim::ev::golfer_score(ctx, p1);
            if (d > s3 / 3) continue;
        } else if (c4 == 'Y') {
            const int s2 = portsim::ev::golfer_score(ctx, p2);
            const int s1 = portsim::ev::golfer_score(ctx, p1);
            if (s1 > s2 - 10) continue;
        }
        if (!BitLetter(ctx, Up(ctx, name, 5), p2, p1, p2)) continue;
        if (!FlagLetter(ctx, Up(ctx, name, 6), r2)) continue;
        bool ok = TypeLetter(ctx, Up(ctx, name, 7), r2);
        for (unsigned a = (GS(g_golfers) + 176); a < (GS(g_golfers) + 39088); a += 0x100u)
            if (G<signed char>(ctx, a - 0x8f) > 0 && static_cast<int>(G<short>(ctx, a)) == static_cast<int>(idx)) ok = false;
        if (!ok) continue;
        G<short>(ctx, r2 + 0xb0) = static_cast<short>(idx);
        G<short>(ctx, r1 + 0xb0) = static_cast<short>(idx);
        G<unsigned>(ctx, r1 + 0x10) |= 0x100000u;
        break;
    }
    if (p1 < 2 && G<int>(ctx, GS(g_date)) < 0x800) {
        const short s = static_cast<short>(G<int>(ctx, GS(g_namedStoryIndex)));
        G<short>(ctx, r2 + 0xb0) = s;
        G<short>(ctx, r1 + 0xb0) = s;
        G<unsigned>(ctx, r1 + 0x10) |= 0x100000u;
    }
    if ((G<unsigned>(ctx, r2 + 0x10) & 0x20000u) == 0) {
        unsigned mask = 0;
        recFor0x98 = 0x98;
        for (unsigned a = (GS(g_golfers) + 182); a < (GS(g_golfers) + 39094); a += 0x100u) {
            if (G<signed char>(ctx, a - 0x95) > 0 && G<short>(ctx, a) >= 0x4c)
                mask |= 1u << ((G<unsigned short>(ctx, a) - 0x4cu) & 31u);
        }
        if (G<unsigned>(ctx, GS(g_flags)) & 0x1000000u) mask = 0xffffffffu;
        int rating = G<int>(ctx, GS(g_skillRatingX100));
        if (G<int>(ctx, GS(g_difficulty)) <= 1) {
            Cpy(buf, Txt((GS(f_strpool_4d3b5c) + 60)));
            Cat(buf, (portsim::ev::itoa_to(ctx, G<int>(ctx, GS(g_funRating)), GS(g_infoPanelNumBuf), 10)));
            Cat(buf, Txt((GS(f_strpool_4d3b5c) + 28)));
            rating = G<int>(ctx, GS(g_funRating));
        } else {
            Cpy(buf, Txt(GS(f_strpool_4d3b5c)));
            kAppendCents(rating);
            Cat(buf, Txt((GS(f_strpool_4d3b5c) + 28)));
        }
        const int k = (p2 / 2) % 6;
        if (k == 1 && (mask & 4) == 0 && G<int>(ctx, GS(g_visitor60Count)) < 8 &&
            G<unsigned char>(ctx, (GS(g_holes) + 1040) + static_cast<unsigned>(G<int>(ctx, GS(g_visitor60Count))) * 0x410u) != 0) {
            const unsigned char tag = static_cast<unsigned char>(G<int>(ctx, GS(g_visitor60Count)) + 0x60);
            Cpy(buf, Txt(GS(f_strpool_4c71b0)));
            G<unsigned char>(ctx, r2 + 0x18) = tag;
            kBuildGolferName(p2, 0);
            G<short>(ctx, r2 + 0xb6) = 0x4f;
            Cat(buf, Txt(GS(f_strpool_4d3af0)));
            if (kStartMessage(static_cast<int>(0x80002108u), 1, p2) != 0) {
                G<unsigned char>(ctx, r2 + 0x19) = 7;
                G<unsigned char>(ctx, r2 + 0x18) = static_cast<unsigned char>(G<unsigned char>(ctx, GS(g_visitor60Count)) + 0x60);
                G<unsigned short>(ctx, r2 + 0x16) = static_cast<unsigned short>(1u << (Rnd(9) & 31));
                G<unsigned short>(ctx, r2 + 0x16) |= static_cast<unsigned short>(1u << (Rnd(9) & 31));
                G<unsigned short>(ctx, r2 + 0x16) |= static_cast<unsigned short>(1u << (Rnd(9) & 31));
                const unsigned bit4 = 1u << (Rnd(9) & 31);
                G<int>(ctx, GS(g_visitor60Count)) = (G<int>(ctx, GS(g_visitor60Count)) + 1) | 0x80;
                G<unsigned short>(ctx, r2 + 0x16) |= static_cast<unsigned short>(bit4 | 0xe00u);
                kPlaySound(0x26, 0x64, 0, 0, 0);
                G<short>(ctx, r2 + 0xae) = 0;
                G<short>(ctx, r1 + 0xae) = 0;
            } else {
                G<unsigned char>(ctx, r2 + 0x18) = 0;
            }
        }
        if (k == 3 && (mask & 2) == 0 &&
            G<unsigned char>(ctx, (GS(g_courseTable) + 31) + static_cast<unsigned>(G<int>(ctx, GS(g_courseIndex))) * 0x2eu) != 2 &&
            rating > (G<int>(ctx, GS(g_landPurchaseCount)) + 2) * (G<int>(ctx, GS(g_difficulty)) + 2) * 50) {
            Cat(buf, Txt((GS(f_strpool_4c6ecc) + 444)));
            G<unsigned char>(ctx, r2 + 0x18) = static_cast<unsigned char>(G<unsigned char>(ctx, GS(g_landPurchaseCount)) + 0x40);
            kBuildGolferName(p2, 0);
            G<short>(ctx, r2 + 0xb6) = 0x4d;
            Cat(buf, Txt(GS(f_strpool_4d3a88)));
            if (kStartMessage(static_cast<int>(0x80002108u), 1, p2) != 0) {
                G<unsigned char>(ctx, r2 + 0x19) = 7;
                G<unsigned short>(ctx, r2 + 0x16) = static_cast<unsigned short>(1u << (Rnd(12) & 31));
                G<unsigned short>(ctx, r2 + 0x16) |= static_cast<unsigned short>(1u << (Rnd(12) & 31));
                G<unsigned short>(ctx, r2 + 0x16) |= static_cast<unsigned short>(1u << (Rnd(12) & 31));
                G<unsigned short>(ctx, r2 + 0x16) |= static_cast<unsigned short>((1u << (Rnd(12) & 31)) | 0x38u);
                kPlaySound(0x26, 0x64, 0, 0, 0);
                G<short>(ctx, r2 + 0xae) = 0;
                G<short>(ctx, r1 + 0xae) = 0;
                G<short>(ctx, r2 + 0xb0) = -1;
                G<short>(ctx, r1 + 0xb0) = -1;
            } else {
                G<unsigned char>(ctx, r2 + 0x18) = 0;
            }
        }
        if (k == 5 && (mask & 4) == 0 &&
            rating > (G<int>(ctx, GS(g_ratingMilestone)) + 1) * (G<int>(ctx, GS(g_ratingMilestone)) + 1) * 25 &&
            G<int>(ctx, GS(g_amenityUnlockMask)) < 0xffff) {
            G<unsigned char>(ctx, r2 + 0x18) = 0x80;
            Cat(buf, Txt((GS(f_strpool_4c6ecc) + 216)));
            kBuildGolferName(p2, 0);
            G<short>(ctx, r2 + 0xb6) = 0x4e;
            Cat(buf, Txt((GS(f_strpool_4d3968) + 172)));
            if (kStartMessage(static_cast<int>(0x80002108u), 1, p2) != 0) {
                G<unsigned char>(ctx, r2 + 0x19) = 6;
                G<unsigned short>(ctx, r2 + 0x16) = static_cast<unsigned short>(1u << (Rnd(12) & 31));
                G<unsigned short>(ctx, r2 + 0x16) |= static_cast<unsigned short>(1u << (Rnd(12) & 31));
                G<unsigned short>(ctx, r2 + 0x16) |= static_cast<unsigned short>(1u << (Rnd(12) & 31));
                G<unsigned short>(ctx, r2 + 0x16) |= static_cast<unsigned short>((1u << (Rnd(12) & 31)) | 0x300u);
                kPlaySound(0x26, 0x64, 0, 0, 0);
                G<short>(ctx, r2 + 0xae) = 0;
                G<short>(ctx, r1 + 0xae) = 0;
                G<short>(ctx, r2 + 0xb0) = -1;
                G<short>(ctx, r1 + 0xb0) = -1;
            } else {
                G<unsigned char>(ctx, r2 + 0x18) = 0;
            }
        }
    }
    if (G<unsigned char>(ctx, r2 + 0x18) != 0) {
        for (int i = 0; i < 12; i++)
            if (static_cast<int>(G<short>(ctx, r2 + 0x16)) & (1 << i))
                G<unsigned char>(ctx, r2 + 0xf0 + static_cast<unsigned>(i)) = static_cast<unsigned char>(Rnd(8) + 1);
    }
    G<short>(ctx, r2 + 0xa4) = static_cast<short>(compat + 2);
    G<short>(ctx, r1 + 0xa4) = static_cast<short>(compat + 2);
    if (G<unsigned char>(ctx, (GS(g_courseTable) + 28) + static_cast<unsigned>(G<int>(ctx, GS(g_courseIndex))) * 0x2eu) == 3)
        G<short>(ctx, Rec(recFor0x98) + 0xa4) = static_cast<short>(G<short>(ctx, Rec(recFor0x98) + 0xa4) + 1);
    G<short>(ctx, r2 + 0xb4) = static_cast<short>(compat * 2);
    G<short>(ctx, r1 + 0xb4) = static_cast<short>(compat * 2);
    if (G<unsigned>(ctx, r1 + 0x10) & 0x100000u)
        G<unsigned char>(ctx, r1 + 0x38) = static_cast<unsigned char>(compat << 1);
    else
        G<unsigned char>(ctx, r2 + 0x38) = static_cast<unsigned char>(compat << 1);
    const unsigned short roll = static_cast<unsigned short>(Rnd(G<int>(ctx, GS(g_awardStories))));
    if ((roll > (SG_NEGCTL(g_negctl_pair_golfers) == 2 ? 7 : 6) && (G<unsigned>(ctx, r1 + 0x10) & 0x20000u) == 0) || G<unsigned char>(ctx, r2 + 0x18) != 0) {
        G<short>(ctx, r2 + 0xb0) = -1;
        G<short>(ctx, r1 + 0xb0) = -1;
        G<short>(ctx, r2 + 0xae) = 0;
        G<short>(ctx, r1 + 0xae) = 0;
    }
    if (compat <= 1) {
        const int a = kTypeBit7Clear(p1);
        if (a == kTypeBit7Clear(p2)) {
            G<short>(ctx, r2 + 0xae) = 2;
            G<short>(ctx, r1 + 0xae) = 2;
        }
    }
    bool pairTest = false;
    if (compat == 5) {
        G<short>(ctx, r2 + 0xae) = 3;
        G<short>(ctx, r1 + 0xae) = 3;
        pairTest = true;
    } else if (compat >= 4) {
        pairTest = true;
    }
    if (pairTest) {
        const int a = kTypeBit7Clear(p1);
        const int b = kTypeBit7Clear(p2);
        if (a != b && (G<unsigned char>(ctx, r1 + 0xb8) & 0x10) && (G<unsigned char>(ctx, r2 + 0xb8) & 0x10)) {
            G<short>(ctx, r2 + 0xae) = 1;
            G<short>(ctx, r1 + 0xae) = 1;
        }
    }
    G<unsigned char>(ctx, r1 + 0x5d) = G<unsigned char>(ctx, r1 + 0xa4);
    G<unsigned char>(ctx, r2 + 0x5d) = G<unsigned char>(ctx, r2 + 0xa4);
    if (G<unsigned char>(ctx, r1 + 0x18) == 0)
        G<short>(ctx, (GS(g_golferTypeState) + 42) + static_cast<unsigned>(static_cast<int>(G<short>(ctx, r1 + 0xb6))) * 44u) += 1;
    if (G<unsigned char>(ctx, r2 + 0x18) == 0)
        G<short>(ctx, (GS(g_golferTypeState) + 42) + static_cast<unsigned>(static_cast<int>(G<short>(ctx, r2 + 0xb6))) * 44u) += 1;
    G<unsigned>(ctx, r1 + 0x10) &= 0xfffdffffu;
    G<unsigned>(ctx, r2 + 0x10) &= 0xfffdffffu;
    const unsigned char dlow = G<unsigned char>(ctx, GS(g_date));
    if ((dlow & 0x40) == 0) return static_cast<int>(0xfffdff00u | dlow);
    emit_golfer_thought(ctx, p1, 0x3d, G<short>(ctx, r1 + 0xa4));
    emit_golfer_thought(ctx, p2, 0x3d, G<short>(ctx, r2 + 0xa4));
    const unsigned a2 = (static_cast<unsigned>(p2 & 0xffe) << 8) + (GS(g_golfers) + 132);
    G<unsigned char>(ctx, a2) = static_cast<unsigned char>(G<unsigned char>(ctx, a2) + 0xa);
    G<unsigned char>(ctx, (static_cast<unsigned>(p1 & 0xffe) << 8) + (GS(g_golfers) + 388)) += 8;
    return static_cast<int>(a2);
}

}  // namespace portsim
