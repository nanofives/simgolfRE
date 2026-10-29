








char gc; unsigned char guc; short gs; unsigned short gus; int gi; unsigned gu;
float gf; double gd; __int64 gq; bool gb;
struct BF { int a : 3; unsigned b : 5; } gbf;
void s01() { gc = gi; }
void s02() { gc = gs; }
void s03() { gi = gc; }
void s04() { gu = guc; }
void s05() { gi = gus; }
void s06() { gd = gf; }
void s07() { gf = gd; }
void s08() { gc = gf; }
void s09() { gf = gi; }
void s10() { gb = gc; }
void s11() { gb = gf; }
void s12() { gq = gi; }
void s13() { gq = gu; }
void s14() { gi = gq; }
void s15() { gc += gc; }
void s16() { gc *= gs; }
void s17() { gc += 3; }
void s18() { gs += gi; }
void s19() { gs += gf; }
void s20() { gi += gf; }
void s21() { gi *= 1.5; }
void s22() { gd += gf; }
void s23() { gf += gd; }
void s24() { gf += gi; }
void s25() { gf *= 2.0; }
void s26() { gs <<= gc; }
void s27() { gq += gi; }
void s28() { gu += gi; }
void s29() { gc++; }
void s30() { ++gc; }
void s31() { gc--; }
void s32() { ++gs; }
void s33() { gf++; }
void s34() { gbf.a += 1; }
void s35() { gbf.b = gi; }















































