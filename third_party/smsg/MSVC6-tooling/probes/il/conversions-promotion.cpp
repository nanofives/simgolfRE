










char gc; unsigned char guc; short gs; unsigned short gus; int gi; bool gb;
enum E { E0, E1 } ge;
struct BF { int a : 3; unsigned b : 5; short c; char d; } gbf;
int garr[8]; char *gp;
int p00() { return gc; }
int p01() { return gc - gs; }
int p02() { return gc * gc; }
int p03() { return gus * gus; }
int p04() { return guc / guc; }
int p05() { return gc % 3; }
int p06() { return gc & gs; }
int p07() { return gi << gc; }
int p08() { return gc >> 1; }
int p09() { return +gc; }
int p10() { return -gc; }
int p11() { return ~gc; }
int p12() { return !gc; }
int p13() { return gc == gs; }
int p14() { return gc && gs; }
int p15() { return gb + gb; }
int p16() { return ge + 1; }
int p17() { return gbf.a + gbf.b; }
int p18() { return gbf.c + gbf.d; }
int p19() { return garr[gc]; }
int p20() { return *(gp + gs); }
int p21() { switch (gc) { case 1: return 1; } return 0; }
int p22() { if (gc) return 1; return 0; }
int p23(int a) { return a ? gc : gs; }



































