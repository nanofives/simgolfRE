








int g(int);
int garr[8];
bool gb;
bool gbarr[8];
struct VB { int vb; int fv(int); };
struct VD : virtual VB { int vd; int fvd(int); };
VD *mkvdp(int);
struct B1 { int b1; virtual void v(); };
struct B2 { int b2; virtual void w(); int f2(int); };
struct DD : B1, B2 { int d; int fd(int); };
struct VV : virtual B1 { int x; };
VV *mkvv(int);
typedef int (DD::*PMF)(int);
typedef int (VD::*PMV)(int);
typedef int VD::*PMVD;
PMF gpmfarr[4];
PMV gpmv;
PMV gpmvarr[4];
PMVD gpmvd;
struct K { int k; K(int); K(const K &); ~K(); };
K mkk(int);
struct CPK { int v; CPK(int); void *operator new(unsigned, K); void operator delete(void *, K); };
struct CPI { int v; CPI(int); void *operator new(unsigned, int); void operator delete(void *, int); };
int u01(int a) { return mkvdp(a)->vb; }
int u02(int a) { gb++; return 0; }
int u03(int a) { gbarr[g(a)]++; return 0; }
int u04(int a) { (garr[g(a)] += a) += 1; return 0; }
int u05(int a) { int &r = (garr[g(a)] = a); return r; }
int u06(int a) { CPK *p = new (mkk(a)) CPK(a); return p->v; }
int u07(int a) { return dynamic_cast<DD *>(mkvv(a)) != 0; }
int u08(int a) { return gpmvarr[g(a)] == 0; }
int u09(int a) { int VD::*pd = &VD::vd; return pd != 0; }
int u10(int a) { return mkvdp(a)->*gpmvd; }
int u11(int a) { return (mkvdp(a)->*gpmv)(1); }
int u12(int a) { int DD::*pd = &B2::b2; return pd != 0; }
int u13(int a) { return gpmfarr[g(a)] == 0; }
int u14(int a) { CPI *p = new (g(a)) CPI(a); return p->v; }

int n01(int a, VD *p) { return p->vb; }
int n02(int a) { gbarr[a]++; return 0; }
int n03(int a) { (garr[a] += a) += 1; return 0; }


































































