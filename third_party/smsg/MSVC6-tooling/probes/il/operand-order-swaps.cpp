






int *gp; int gi; int *gq;
struct V { int x; };
V operator+(V a, V b);
V va, vb, vc;
int o_ip() { gq = gi + gp; return 0; }
int o_pi() { gq = gp + gi; return 0; }
int o_isub() { return gi[gp]; }
int o_psub() { return gp[gi]; }
void o_user() { vc = va + vb; }


























