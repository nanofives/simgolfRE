// Events, news and stories, part 4: the builders of notices and events (landmark, patron, hole type, hole magazine, building).
// Phase 3.5, written from the specification (analysis notes, the hook sources as reading reference, vector corpus); the
// original's strings are (pool member, byte offset) pairs and no original address appears.
#include <cmath>

namespace portsim {

void announce_landmark(Ctx &ctx, int32_t g, int32_t who) {
    const uint32_t buf = text_va(ctx);
    (void)buf;
    int x = G<int>(ctx, GS(g_golfers) + g * 0x100) >> 10;
    int y = G<int>(ctx, (GS(g_golfers) + 4) + g * 0x100) >> 10;
    for (;;) {
        const int index = x * 50 + y;
        const signed char type = G<signed char>(ctx, GS(g_tileType) + index);
        if (G<unsigned char>(ctx, (GS(g_tileTypeAttrs) + 38) + type * 0x30) == 4 && type != 0x16 &&
            (G<unsigned short>(ctx, GS(g_tileFlags) + index * 2) & 0x320) == 0)
            break;
        x += (Rnd(SG_NEGCTL(g_negctl_announce_landmark) == 2 ? 2 : 3) & 0xFFFF) - 1;
        y += (Rnd(3) & 0xFFFF) - 1;
        if (kTileBlocked(x, y) != 0) return;
    }
    int id;
    switch (G<signed char>(ctx, GS(g_storyDefs) + who * 50)) {
    case 'A': id = SG_NEGCTL(g_negctl_announce_landmark) == 1 ? 0x16b : 0x16a; break;
    case 'C': id = 0x168; break;
    case 'F': id = 0x170; break;
    case 'G': id = 0x16f; break;
    case 'H': id = 0x16d; break;
    case 'L': id = 0x16c; break;
    case 'M': id = 0x16b; break;
    case 'P': id = 0x169; break;
    case 'R': id = 0x170; break;
    case 'S': id = 0x173; break;
    case 'X': id = 0x171; break;
    default:  id = (Rnd(10) & 0xFFFF) + 0x168; break;
    }
    const unsigned bit = 1u << (id - 0x168);
    G<unsigned>(ctx, GS(g_freeUnlockMask)) |= bit;
    G<unsigned>(ctx, GS(g_amenityUnlockMask)) |= bit;
    CatAt(GS(f_strpool_4e409c));
    kLandmarkName(id - 0x168, 1);
    CatAt((GS(f_strpool_4e4004) + 108));
    kStartMessage(0x80000210, 0, -1);
}


static void patron_refusal(Ctx &ctx, int g, unsigned rec, unsigned head, unsigned tail) {
    const uint32_t buf = text_va(ctx);
    (void)buf;
    CatAt((head));
    if (G<short>(ctx, (GS(g_golfers) + 164) + rec) > 2) {
        CatAt(((GS(f_strpool_4c6ecc) + 504)));
        kBuildGolferName(G<short>(ctx, (GS(g_golfers) + 162) + rec), 0);
        CatAt(((GS(f_strpool_4c6ecc) + 480)));
    }
    kAppendOpinion(G<short>(ctx, (GS(g_golfers) + 164) + rec), 0);
    CatAt((tail));
    kStartMessage(0x80006000, 1, g);
}


void patron_event(Ctx &ctx, int32_t g) {
    const uint32_t buf = text_va(ctx);
    (void)buf;
    const unsigned rec = static_cast<unsigned>(g) * 0x100;
    const unsigned char kind = G<unsigned char>(ctx, (GS(g_golfers) + 24) + rec);
    if ((kind & 0xe0) == 0x60) {
        Cpy(GS(g_textBuffer), (GS(f_strpool_4c71b0)));
        kBuildGolferName(g, 0);
        G<unsigned>(ctx, GS(g_visitor60Count)) &= 0xFFFFFF7F;
        const int hole = G<signed char>(ctx, (GS(g_golfers) + 33) + rec);
        if (G<signed char>(ctx, GS(g_holes) + hole * 0x208) != 0 || G<short>(ctx, (GS(g_golfers) + 164) + rec) <= 2) {
            patron_refusal(ctx, g, rec, (GS(f_strpool_4c6ecc) + 516), (GS(f_strpool_4c6ecc) + 468));
        } else if (hole > G<int>(ctx, GS(g_visitor60Count)) * 2) {
            kLogTick(0x100, kind & 0x1f);
            const int amount = ((G<short>(ctx, (GS(g_golfers) + 164) + rec) > 4) + 1) * (SG_NEGCTL(g_negctl_patron_event) == 1 ? 4000 : 5000) / 100;
            CatAt((GS(f_strpool_4c7124)));
            kAppendNumber(amount * 100);
            CatAt(((GS(f_strpool_4c6ecc) + 564)));
            G<unsigned char>(ctx, GS(g_ui_569498)) = 0;
            kStartMessage(0x80001204, 1, g);
            G<int>(ctx, GS(g_cash_div100)) += amount;
            const int slot = G<short>(ctx, GS(g_year)) % 100;
            G<short>(ctx, (GS(g_financeYears) + 14) + slot * 0x14) += static_cast<short>(amount);
            kPlaySound(0x19, 100, 0, 0, 0);
        } else {
            CatAt((GS(f_strpool_4c715c)));
            kStartMessage(0x80006000, 1, g);
            G<int>(ctx, GS(g_visitor60Count)) -= 1;
        }
    }
    if ((kind & 0xe0) == 0x40) {
        Cpy(GS(g_textBuffer), ((GS(f_strpool_4c6ecc) + 444)));
        kBuildGolferName(g, 0);
        const int hole = G<signed char>(ctx, (GS(g_golfers) + 33) + rec);
        G<unsigned>(ctx, GS(g_landPurchaseCount)) &= 0xFFFFFF7F;
        if (G<signed char>(ctx, GS(g_holes) + hole * 0x208) != 0 || G<short>(ctx, (GS(g_golfers) + 164) + rec) <= 2) {
            patron_refusal(ctx, g, rec, (GS(f_strpool_4c6ecc) + 252), (GS(f_strpool_4c6ecc) + 236));
        } else {
            CatAt(((GS(f_strpool_4c6ecc) + 396)));
            CatAt(((GS(f_strpool_4c6ecc) + 364)));
            if (G<int>(ctx, GS(g_difficulty)) > 1) {
                CatAt(((GS(f_strpool_4c6ecc) + 320)));
                kAppendCents((G<int>(ctx, GS(g_difficulty)) + 2) * (G<int>(ctx, GS(g_landPurchaseCount)) + 3) * 50);
            } else {
                CatAt(((GS(f_strpool_4c6ecc) + 344)));
                portsim::ev::itoa_to(ctx, (G<int>(ctx, GS(g_difficulty)) + 2) * (G<int>(ctx, GS(g_landPurchaseCount)) + 3) * 50, GS(g_textBuf), 10);
                CatAt(GS(g_textBuf));
            }
            CatAt(((GS(f_strpool_4c6ecc) + 308)));
            G<unsigned char>(ctx, GS(g_ui_569498)) = 0;
            G<int>(ctx, GS(unk_005a9ccc)) = 3;
            kStartMessage(0x80001204, 1, g);
            kPlaySound(0x2f, 100, 0, 0, 0);
            G<unsigned>(ctx, GS(g_eventFlags567a1c)) |= 1;
        }
    }
    if ((kind & 0xe0) == 0x80) {
        Cpy(GS(g_textBuffer), ((GS(f_strpool_4c6ecc) + 216)));
        kBuildGolferName(g, 0);
        const int hole = G<signed char>(ctx, (GS(g_golfers) + 33) + rec);
        if (G<signed char>(ctx, GS(g_holes) + hole * 0x208) != 0 || G<short>(ctx, (GS(g_golfers) + 164) + rec) <= 2) {
            patron_refusal(ctx, g, rec, (GS(f_strpool_4c6d6c) + 84), (GS(f_strpool_4c6d6c) + 36));
        } else {
            int counter = G<short>(ctx, (GS(g_golfers) + 164) + rec);
            int lm;
            for (;;) {
                lm = kClamp(Rnd(counter) & 0xFFFF, 0, 0xf);
                if (((1u << (lm & 0x1f)) & G<unsigned>(ctx, GS(g_amenityUnlockMask))) == 0) break;
                ++counter;
                if (counter > 0x18) break;
            }
            CatAt(((GS(f_strpool_4c6ecc) + 188)));
            kLandmarkName(lm, 1);
            CatAt(((GS(f_strpool_4c6ecc) + 136)));
            switch (lm & 3) {
            case 0: CatAt(((GS(f_strpool_4c6ecc) + 100))); break;
            case 1: CatAt(((GS(f_strpool_4c6ecc) + 72))); break;
            case 2: CatAt(((GS(g_golferTypes) + 16))); CatAt(((GS(f_strpool_4c6ecc) + 40))); break;
            case 3: CatAt((GS(f_strpool_4c6ecc))); break;
            }
            CatAt(((GS(f_strpool_4c6d6c) + 268)));
            kLandmarkName(lm, 0);
            CatAt(((GS(f_strpool_4c6d6c) + 232)));
            kAppendNumber((lm * 5 + 5) * 200);
            CatAt(((GS(f_strpool_4c6d6c) + 204)));
            const int diff = G<int>(ctx, GS(g_difficulty));
            G<int>(ctx, GS(g_ratingMilestone)) += 1;
            if (diff > 1) {
                CatAt(((GS(f_strpool_4c6d6c) + 156)));
                const int n = G<int>(ctx, GS(g_ratingMilestone)) + 1;
                kAppendCents(n * n * 25);
            } else {
                CatAt(((GS(f_strpool_4c6d6c) + 180)));
                const int n = G<int>(ctx, GS(g_ratingMilestone)) + 1;
                portsim::ev::itoa_to(ctx, n * n * 25, GS(g_textBuf), 10);
                CatAt(GS(g_textBuf));
            }
            CatAt(((GS(f_strpool_4c6d6c) + 140)));
            G<unsigned char>(ctx, GS(g_ui_569498)) = 0;
            kStartMessage(0x80001204, 1, g);
            kPlaySound(0x2f, 100, 0, 0, 0);
            kLogTick(0x160, lm);
            const unsigned bit = 1u << (lm & 0x1f);
            G<unsigned>(ctx, GS(g_amenityUnlockMask)) |= bit;
            G<unsigned>(ctx, GS(g_freeUnlockMask)) |= bit;
        }
    }
    G<int>(ctx, (GS(g_golfers) + 212) + rec) = 0;
    G<unsigned char>(ctx, (GS(g_golfers) + 33) + rec) = 0x13;
    if (G<unsigned>(ctx, (GS(g_golfers) + 16) + rec) & 0x200) G<int>(ctx, GS(g_playerGolfer)) = -1;
}


void announce_hole_type(Ctx &ctx, int32_t hole, int32_t kind) {
    const uint32_t buf = text_va(ctx);
    (void)buf;
    const unsigned kText1to7[7] = {GS(f_strpool_4d54bc), GS(f_strpool_4d5450), GS(f_strpool_4d53c0), GS(f_strpool_4d532c),
                                          GS(f_strpool_4d52b8), GS(f_strpool_4d522c), GS(f_strpool_4d51a4)};
    Cpy(GS(g_textBuffer), ((GS(f_strpool_4d5514) + 36)));
    Cat(GS(g_textBuffer), portsim::ev::itoa_to(ctx, hole, (GS(g_infoPanelNumBuf)), 10));
    Cat(GS(g_textBuffer), (GS(f_strpool_4d5514)));
    const unsigned k = static_cast<unsigned>(kind) - 1;
    if (k > 6) return;
    Cat(GS(g_textBuffer), (kText1to7[k]));
    int event;
    switch (kind) {
    case 3: event = SG_NEGCTL(g_negctl_announce_hole_type) == 1 ? 1 : 0; break;
    case 5: event = 1; break;
    case 6: event = 4; break;
    case 7: event = 9; break;
    default: return;
    }
    if (kind != 7 && G<int>(ctx, GS(g_difficulty)) < 2) return;
    const unsigned h = static_cast<unsigned>(hole) * 0x208;
    kPostEvent(event, G<int>(ctx, (GS(g_holes) + 24) + h) * 0x400 + 0x200, G<int>(ctx, (GS(g_holes) + 28) + h) * 0x400 + 0x200);
}


void hole_magazine_event(Ctx &ctx) {
    const uint32_t buf = text_va(ctx);
    (void)buf;
    int members = 0;
    for (unsigned p = (GS(g_golferTypeState) + 2); p < (GS(g_golferTypeState) + 3346); p += 0x2c)
        if (G<unsigned char>(ctx, p + 0x27) != 0xff && (G<unsigned char>(ctx, p) & 7) > 2) ++members;
    for (unsigned q = GS(g_placed_objects); q < GS(g_sprites58ccb8); q += 0x10)
        if (G<short>(ctx, q) == 5) --members;
    G<int>(ctx, (GS(g_ui_56d1a8) + 8)) = members;
    G<int>(ctx, GS(g_funRating)) = 0;
    G<int>(ctx, GS(g_coursePar)) = 0;
    G<int>(ctx, GS(g_courseYards)) = 0;
    G<int>(ctx, GS(g_imaginationX100)) = 0;
    G<int>(ctx, GS(g_accuracySkillX100)) = 0;
    G<int>(ctx, GS(g_lengthSkillX100)) = 0;
    int skillSum = 0;
    int prevNotable = -1;
    int hole = 1;
    for (;;) {
        const signed char par = fld<signed char>(ctx, GS(g_holes), hole);
        if (par != 0) {
            const int prevSkillSum = skillSum;
            const int prevFun = G<int>(ctx, GS(g_funRating));
            G<int>(ctx, GS(g_courseYards)) += fld<short>(ctx, (GS(g_holes) + 4), hole);
            G<int>(ctx, GS(g_coursePar)) += par;
            int col20[8], col40[8];
            for (int c = 0; c < 8; ++c) { col20[c] = static_cast<int>(par) << 3; col40[c] = 8; }
            if (fld<int>(ctx, (GS(g_holes) + 32), hole) != 0)
                G<int>(ctx, GS(g_funRating)) += (fld<short>(ctx, (GS(g_holes) + 344), hole) * 100) /
                                        (fld<int>(ctx, (GS(g_holes) + 36), hole) / 2 + 4 + fld<int>(ctx, (GS(g_holes) + 32), hole));
            int histTotal = 0, histWeighted = 0;
            uint32_t rowPtr = (GS(g_holes) + 42) + static_cast<uint32_t>(hole * 0x208);
            for (int row = 1; row < 10; ++row) {
                uint32_t pc = rowPtr;
                for (int c = 0; c < 8; ++c) {
                    const int v = G<short>(ctx, pc);
                    histTotal += v;
                    col40[c] += v;
                    col20[c] += v * row;
                    histWeighted += v * row;
                    pc += 0x16;
                }
                rowPtr += 2;
            }
            unsigned f0 = fld<unsigned>(ctx, (GS(g_holes) + 512), hole);
            fld<unsigned>(ctx, (GS(g_holes) + 512), hole) = f0 & 0xFFFFFFF3;
            if (histTotal >= 10) {
                const int d = -G<int>(ctx, GS(g_difficulty));
                if (((d + 6) * histTotal) / 3 + par * histTotal < histWeighted)
                    fld<unsigned>(ctx, (GS(g_holes) + 512), hole) = (f0 & 0xFFFFFFF3) | 4;
                if (histWeighted < par * histTotal - ((d + 3) * histTotal) / 6)
                    fld<unsigned>(ctx, (GS(g_holes) + 512), hole) |= 8;
            }
            unsigned f1 = fld<unsigned>(ctx, (GS(g_holes) + 512), hole);
            int notable = 0;
            int minDelta = 100;
            int minSkillBit = 0;
            fld<unsigned>(ctx, (GS(g_holes) + 512), hole) = f1 & 0xFFFFF8FF;
            const int notableThresh = (G<int>(ctx, GS(g_difficulty)) != 0) ? (SG_NEGCTL(g_negctl_hole_magazine_event) == 1 ? 0x31 : 0x32) : 0x19;
            if (col40[7] != 0) {
                if (col40[6] != 0) {
                    const int delta = (col20[6] * 100) / col40[6] - (col20[7] * 100) / col40[7];
                    G<int>(ctx, GS(g_lengthSkillX100)) += delta;
                    skillSum += delta;
                    notable = (delta >= notableThresh) ? 1 : 0;
                    if (delta > 0x31) fld<unsigned>(ctx, (GS(g_holes) + 512), hole) = (f1 & 0xFFFFF8FF) | 0x100;
                    if (delta < 100) { minSkillBit = 1; minDelta = delta; }
                }
                if (col40[5] != 0) {
                    const int delta = (col20[5] * 100) / col40[5] - (col20[7] * 100) / col40[7];
                    skillSum += delta;
                    G<int>(ctx, GS(g_accuracySkillX100)) += delta;
                    if (delta >= notableThresh) notable |= 2;
                    if (delta > 0x31) fld<unsigned>(ctx, (GS(g_holes) + 512), hole) |= 0x200;
                    if (delta < minDelta) { minSkillBit = 2; minDelta = delta; }
                }
                if (col40[3] != 0) {
                    const int delta = (col20[3] * 100) / col40[3] - (col20[7] * 100) / col40[7];
                    G<int>(ctx, GS(g_imaginationX100)) += delta;
                    skillSum += delta;
                    if (delta >= notableThresh) notable |= 4;
                    if (delta > 0x31) fld<unsigned>(ctx, (GS(g_holes) + 512), hole) |= 0x400;
                    if (delta < minDelta) { minSkillBit = 4; minDelta = delta; }
                }
            }
            if (minDelta < ((G<int>(ctx, GS(g_difficulty)) != 0) ? 0x64 : 0x32))
                notable &= ~minSkillBit;
            bool fire;
            if ((G<unsigned char>(ctx, GS(g_forcedEventBits)) & 2) == 0) {
                fire = (notable != 0);
            } else {
                G<unsigned>(ctx, GS(g_forcedEventBits)) &= 0xFFFFFFFD;
                hole = 1;
                notable = 4;
                fire = true;
            }
            if (fire) {
                const unsigned bit = 1u << notable;
                if ((bit & G<unsigned>(ctx, GS(g_holeNoticeMask))) == 0) {
                    kAnnounceType(hole, notable);
                    if (kStartMessage(0x80001284, 0, -4) != 0) {
                        G<unsigned>(ctx, GS(g_holeNoticeMask)) |= bit;
                        kPlaySound(0x2a, 100, 0, 0, 0);
                    }
                }
            }
            fld<unsigned>(ctx, (GS(g_holes) + 512), hole) &= 0xFFFFFFEF;
            fld<int>(ctx, (GS(g_holes) + 508), hole) = 0;
            if (hole > 1) {
                if (notable == prevNotable && fld<int>(ctx, (GS(g_holes) + 32), hole) > 7 && notable != 7)
                    fld<int>(ctx, (GS(g_holes) + 508), hole) = 1;
                if (((fld<unsigned>(ctx, (GS(g_ballRecords) + 9208), hole) ^ fld<unsigned>(ctx, (GS(g_holes) + 512), hole)) & 0x60) == 0)
                    fld<int>(ctx, (GS(g_holes) + 508), hole) += 1;
                if (fld<short>(ctx, (GS(g_holes) + 306), hole) == 0 && fld<short>(ctx, (GS(g_holes) + 308), hole) == 0)
                    fld<int>(ctx, (GS(g_holes) + 508), hole) += 1;
                if (fld<signed char>(ctx, GS(g_holes), hole) == fld<signed char>(ctx, (GS(g_ballRecords) + 8696), hole))
                    fld<int>(ctx, (GS(g_holes) + 508), hole) += 1;
                const int a1 = kAngleFixed(fld<int>(ctx, (GS(g_ballRecords) + 8720), hole) - fld<int>(ctx, (GS(g_ballRecords) + 8704), hole),
                                           fld<int>(ctx, (GS(g_ballRecords) + 8724), hole) - fld<int>(ctx, (GS(g_ballRecords) + 8708), hole));
                const int a2 = kAngleFixed(fld<int>(ctx, (GS(g_holes) + 24), hole) - fld<int>(ctx, (GS(g_holes) + 8), hole),
                                           fld<int>(ctx, (GS(g_holes) + 28), hole) - fld<int>(ctx, (GS(g_holes) + 12), hole));
                const int diff = a2 - a1;
                const int s = diff >> 0x1f;
                if ((((diff >> 0x18) ^ s) - s) < 0x28)
                    fld<int>(ctx, (GS(g_holes) + 508), hole) += 1;
                if (fld<int>(ctx, (GS(g_holes) + 508), hole) != 0 && G<int>(ctx, GS(g_difficulty)) < 2)
                    fld<int>(ctx, (GS(g_holes) + 508), hole) -= 1;
            }
            prevNotable = notable;
            int skillDelta = skillSum - prevSkillSum;
            const int funDelta = ((G<int>(ctx, GS(g_funRating)) - prevFun) * 6) /
                                 ((G<int>(ctx, GS(g_difficulty)) != 0) + 2);
            if (G<int>(ctx, GS(g_difficulty)) < 2 && skillDelta < funDelta)
                skillDelta = funDelta;
            if (fld<int>(ctx, (GS(g_holes) + 500), hole) > 200 && skillDelta > 200 &&
                (fld<unsigned char>(ctx, (GS(g_holes) + 512), hole) & 0xd) == 0) {
                Cpy((GS(g_textBuffer)), (GS(f_strpool_4c581c) + 256));
                portsim::ev::itoa_to(ctx, hole, (GS(g_textBuf)), 10);
                CatAt((GS(g_textBuf)));
                CatAt((GS(f_strpool_4c7728) + 96));
                fld<unsigned>(ctx, (GS(g_holes) + 512), hole) |= 0x80;
                kAppendHoleName(hole);
                CatAt((GS(f_strpool_4c7728) + 44));
                CatAt(GS(f_strpool_4c7728));
                CatAt(GS(f_strpool_4c7708));
                if (kStartMessage(0x80004010, 0, -21) == 0) {
                    fld<unsigned>(ctx, (GS(g_holes) + 512), hole) &= 0xFFFFFF7F;
                } else {
                    fld<unsigned>(ctx, (GS(g_holes) + 512), hole) |= 1;
                    G<int>(ctx, (GS(f_w_4c2848) + 8)) += 1;
                    kLogTick(0x80, static_cast<unsigned short>(hole));
                    kPlaySound(0x2e, 100, 0, 0, 0);
                    kPostEvent(7, fld<int>(ctx, (GS(g_holes) + 24), hole) * 0x400 + 0x200,
                               fld<int>(ctx, (GS(g_holes) + 28), hole) * 0x400 + 0x200);
                }
            }
            if (fld<int>(ctx, (GS(g_holes) + 500), hole) > 400 && skillDelta > 300 &&
                (fld<unsigned char>(ctx, (GS(g_holes) + 512), hole) & 0xe) == 0) {
                G<char>(ctx, GS(g_textBuffer)) = 0;
                kAppendHoleName(hole);
                CatAt((GS(f_strpool_4c76ac) + 48));
                CatAt(GS(f_strpool_4c76ac));
                CatAt(GS(f_strpool_4c7708));
                if (kStartMessage(0x80005288, 0, -21) != 0) {
                    G<int>(ctx, (GS(f_w_4c2848) + 8)) += 1;
                    fld<unsigned>(ctx, (GS(g_holes) + 512), hole) |= 2;
                    kLogTick(0xa0, static_cast<unsigned short>(hole));
                    kPlaySound(0x2f, 100, 0, 0, 0);
                    kPostEvent(10, fld<int>(ctx, (GS(g_holes) + 24), hole) * 0x400 + 0x200,
                               fld<int>(ctx, (GS(g_holes) + 28), hole) * 0x400 + 0x200);
                }
            }
        }
        hole = hole + 1;
        if (hole > 0x12) {
            G<int>(ctx, GS(g_skillRatingX100)) = skillSum;
            return;
        }
    }
}


void announce_building(Ctx &ctx) {
    const uint32_t buf = text_va(ctx);
    (void)buf;
    const int hole = G<int>(ctx, GS(g_holesPlusOne));
    const unsigned rec = hole * 0x208;
    if (G<int>(ctx, (GS(g_holes) + 8) + rec) == 0 || G<int>(ctx, (GS(g_holes) + 24) + rec) == 0 ||
        ((G<unsigned char>(ctx, GS(g_flags)) & 0x40) != 0 && G<int>(ctx, (GS(g_holes) + 16) + rec) == 0)) {
        kPlaySound(0x18, 100, 0, 0, 0);
        return;
    }
    for (int i = 0; i < 10; ++i) G<int>(ctx, GS(g_bestRounds) + i * 4) = 0;
    kLogTick(0x20, hole);
    uint32_t shape = 0;
    shape = ((GS(f_strpool_4c581c) + 280));
    if (G<int>(ctx, GS(g_date)) != 0) kPlaySound(0x2a, 100, 0, 0, 0);
    if (G<short>(ctx, (GS(g_holes) + 4) + rec) == 0) {
        int dx = G<int>(ctx, (GS(g_holes) + 8) + rec) - G<int>(ctx, (GS(g_holes) + 24) + rec);
        if (dx < 0) dx = -dx;
        int dy = G<int>(ctx, (GS(g_holes) + 12) + rec) - G<int>(ctx, (GS(g_holes) + 28) + rec);
        if (dy < 0) dy = -dy;
        const int sq = (dy * dy + dx * dx) * 625;
        const short len = static_cast<short>(static_cast<int>(static_cast<long long>(sqrt(static_cast<double>(sq)))));
        G<short>(ctx, (GS(g_holes) + 4) + rec) = len;
        if (len > 250)
            G<short>(ctx, (GS(g_holes) + 4) + rec) = static_cast<short>((static_cast<int>(len) - 250) / 4 + len);
    }
    int yards = G<short>(ctx, (GS(g_holes) + 4) + rec);
    if ((G<unsigned char>(ctx, (GS(g_holes) + 512) + rec) & 0x60) != 0 && yards > 250) yards += 25;
    if (yards > 300) yards -= 25 * G<int>(ctx, (GS(g_objectTypeLevel) + 40));
    G<unsigned char>(ctx, GS(g_holes) + rec) = 3;
    if (yards <= (SG_NEGCTL(g_negctl_announce_building) == 1 ? 0x31 : 0x32)) G<unsigned char>(ctx, GS(g_holes) + rec) = 2;
    if (yards >= 0xfa) G<unsigned char>(ctx, GS(g_holes) + rec) = 4;
    if (yards >= 0x1db) G<unsigned char>(ctx, GS(g_holes) + rec) = 5;
    if (yards > 0x271) G<unsigned char>(ctx, GS(g_holes) + rec) = 6;
    if (G<int>(ctx, GS(g_difficulty)) < 2) {
        const int px = G<int>(ctx, (GS(g_holes) + 24) + rec) * 0x400 + 0x200;
        const int py = G<int>(ctx, (GS(g_holes) + 28) + rec) * 0x400 + 0x200;
        if (G<unsigned char>(ctx, (GS(g_holes) + 512) + rec) & 0x40) kPostEvent(0, px, py);
        if (G<unsigned char>(ctx, (GS(g_holes) + 512) + rec) & 0x20) kPostEvent(1, px, py);
        if (G<signed char>(ctx, GS(g_holes) + rec) == 5) kPostEvent(4, px, py);
    }
    const signed char par = G<signed char>(ctx, GS(g_holes) + rec);
    if (par == 3) {
        if (yards < 0x7d) shape = ((GS(f_strpool_4c581c) + 272));
        if (yards > 0xc8) shape = ((GS(f_strpool_4c581c) + 264));
    } else if (par == 4) {
        if (yards < 0x15e) shape = ((GS(f_strpool_4c581c) + 272));
        if (yards > 0x1a9) shape = ((GS(f_strpool_4c581c) + 264));
    } else if (par == 5) {
        if (yards < 0x1f4) shape = ((GS(f_strpool_4c581c) + 272));
        if (yards > 0x23f) shape = ((GS(f_strpool_4c581c) + 264));
    }
    G<int>(ctx, (GS(g_holes) + 500) + rec) = 0;
    G<int>(ctx, (GS(g_holes) + 36) + rec) = 0;
    G<int>(ctx, (GS(g_holes) + 32) + rec) = 0;
    G<int>(ctx, (GS(g_holes) + 492) + rec) = 0;
    G<short>(ctx, (GS(g_holes) + 346) + rec) = 0;
    G<short>(ctx, (GS(g_holes) + 344) + rec) = 0;
    for (int r = 0; r < 10; ++r)
        for (int c = 0; c < 8; ++c) G<short>(ctx, (GS(g_holes) + 40) + rec + r * 2 + c * 0x16) = 0;
    for (int i = 0; i < 0x20; ++i) G<int>(ctx, (GS(g_holes) + 216) + rec + i * 4) = 0;
    const int ang = kAngleFixed(G<int>(ctx, (GS(g_holes) + 24) + rec) - G<int>(ctx, (GS(g_holes) + 16) + rec),
                                G<int>(ctx, (GS(g_holes) + 28) + rec) - G<int>(ctx, (GS(g_holes) + 20) + rec));
    G<unsigned char>(ctx, (GS(g_holes) + 2) + rec) = static_cast<unsigned char>(((((ang >> 0x1c) & 0xf) + (SG_NEGCTL(g_negctl_announce_building) == 2 ? 0 : 1)) >> 1) & 7);
    const int cell = G<int>(ctx, (GS(g_holes) + 12) + rec) + G<int>(ctx, (GS(g_holes) + 8) + rec) * 50;
    G<unsigned char>(ctx, (GS(g_tileFlags) + 1) + cell * 2) |= 0x40;
    G<unsigned char>(ctx, GS(tile_anim_frame) + cell) = 0;
    Cpy(GS(g_textBuffer), ((GS(f_strpool_4c581c) + 256)));
    Cat(GS(g_textBuffer), portsim::ev::itoa_to(ctx, hole, GS(g_textBuf), 10));
    Cat(GS(g_textBuffer), ((GS(f_strpool_4c581c) + 248)));
    Cat(GS(g_textBuffer), shape);
    Cat(GS(g_textBuffer), portsim::ev::itoa_to(ctx, G<short>(ctx, (GS(g_holes) + 4) + rec), GS(g_textBuf), 10));
    Cat(GS(g_textBuffer), ((GS(f_strpool_4c581c) + 240)));
    const unsigned flags = G<unsigned>(ctx, (GS(g_holes) + 512) + rec);
    if (flags & 0x1000) Cat(GS(g_textBuffer), ((GS(f_strpool_4c581c) + 232)));
    if (flags & 0x2000) Cat(GS(g_textBuffer), ((GS(f_strpool_4c581c) + 220)));
    Cat(GS(g_textBuffer), ((GS(f_strpool_4c581c) + 212)));
    Cat(GS(g_textBuffer), portsim::ev::itoa_to(ctx, G<signed char>(ctx, GS(g_holes) + rec), GS(g_textBuf), 10));
    const unsigned flags2 = G<unsigned>(ctx, (GS(g_holes) + 512) + rec);
    if (flags2 & 0x20) Cat(GS(g_textBuffer), ((GS(f_strpool_4c581c) + 184)));
    if (flags2 & 0x40) Cat(GS(g_textBuffer), ((GS(f_strpool_4c581c) + 156)));
    Cat(GS(g_textBuffer), ((GS(f_strpool_4c581c) + 132)));
    int msgArg = -1;
    if (hole == 6) {
        kAppendUpgradeText(0);
    } else if (hole != 10) {
        const int bi = G<int>(ctx, GS(g_buildingUnlockLevel));
        if (hole > bi - 5 && bi < 0xf) {
            Cat(GS(g_textBuffer), ((hole & 1) ? (GS(f_strpool_4c581c) + 84) : (GS(f_strpool_4c581c) + 32)));
            Cat(GS(g_textBuffer), ((GS(f_strpool_4c581c) + 8)));
            Cat(GS(g_textBuffer), (GS(f_tab_buildItems_4c26b0) + bi * 0x14));
            Cat(GS(g_textBuffer), (GS(f_strpool_4c581c)));
            Cat(GS(g_textBuffer), (GS(f_tab_buildItems_4c26b0) + bi * 0x14));
            const int bi2 = G<int>(ctx, GS(g_buildingUnlockLevel));
            if (static_cast<unsigned>(bi2 - 3) <= 0xb) {
                unsigned s = 0;
                switch (bi2) {
                    case 3:  s = GS(f_strpool_4c57ac); break;
                    case 5:  Cat(GS(g_textBuffer), ((GS(f_strpool_4c5720) + 48)));
                             s = GS(f_strpool_4c5720); break;
                    case 6:  s = (GS(f_strpool_4c55dc) + 184); break;
                    case 7:  s = GS(f_strpool_4c56dc); break;
                    case 8:  s = (GS(f_strpool_4c55dc) + 60); break;
                    case 9:  s = GS(f_strpool_4c55dc); break;
                    case 10: s = (GS(f_strpool_4c55dc) + 120); break;
                    case 11: s = (GS(f_strpool_4c54c4) + 48); break;
                    case 12: s = (GS(f_strpool_4c54c4) + 148); break;
                    case 13: s = (GS(f_strpool_4c54c4) + 208); break;
                    case 14: s = (GS(f_strpool_4c54c4) + 88); break;
                    default: break;
                }
                if (s != 0) Cat(GS(g_textBuffer), (s));
            }
            msgArg = -bi2;
            G<int>(ctx, GS(g_buildingUnlockLevel)) = bi2 + 1;
        }
    }
    if (hole == 10) kAppendUpgradeText(1);
    if (hole == 0x12) kAppendUpgradeText(2);
    if (G<int>(ctx, GS(g_date)) != 0)
        kStartMessage(static_cast<int>(0x80004210), 1, msgArg);
    if (hole == 1) {
        G<signed char>(ctx, (GS(g_walkers) + 16)) =
            static_cast<signed char>((G<int>(ctx, (GS(g_holes) + 9888)) + G<int>(ctx, (GS(g_holes) + 528)) * 2) / 3);
        G<signed char>(ctx, (GS(g_walkers) + 17)) =
            static_cast<signed char>((G<int>(ctx, (GS(g_holes) + 9892)) + G<int>(ctx, (GS(g_holes) + 532)) * 2) / 3);
    } else {
        if (hole == 9)
            kPostEvent(6, G<int>(ctx, (GS(g_holes) + 4704)) * 0x400 + 0x200, G<int>(ctx, (GS(g_holes) + 4708)) * 0x400 + 0x200);
        if (hole == 0x12)
            kPostEvent(0xe, G<int>(ctx, (GS(g_holes) + 9384)) * 0x400 + 0x200, G<int>(ctx, (GS(g_holes) + 9388)) * 0x400 + 0x200);
    }
    G<int>(ctx, GS(g_holesPlusOne)) = 1;
    if (G<signed char>(ctx, (GS(g_holes) + 520)) != 0) {
        int n = 1;
        while (G<signed char>(ctx, (GS(g_holes) + 520) + n * 0x208) != 0) ++n;
        G<int>(ctx, GS(g_holesPlusOne)) = n + 1;
    }
    G<int>(ctx, GS(f_w_4c2848)) = -1;
}

}  // namespace portsim
