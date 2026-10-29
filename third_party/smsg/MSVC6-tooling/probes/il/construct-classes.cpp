




int m00(int a) { return a; }
struct A0 { int x; };
int m01(int a) { return a; }
struct A1 { int x; int y; };
int m02(int a) { return a; }
struct A2 { int x; static int s; };
int m03(int a) { return a; }
struct A3 { int x; int get(); };
int m04(int a) { return a; }
struct A4 { int x; int get(int, int); };
int m05(int a) { return a; }
struct A5 { int x; A5(); };
int m06(int a) { return a; }
struct A6 { int x; A6(int); };
int m07(int a) { return a; }
struct A7 { int x; ~A7(); };
int m08(int a) { return a; }
struct A8 { int x; virtual int v(); };
int m09(int a) { return a; }
struct A9 { int x; virtual ~A9(); };
int m10(int a) { return a; }
struct A10 { int x; int get() { return x; } };
int m11(int a) { return a; }
struct A11 : A0 { int z; };
int m12(int a) { return a; }
struct A12 : A8 { int z; };
int m13(int a) { return a; }
struct A13 { A7 m; };
int m14(int a) { return a; }
struct A14;
int m15(int a) { return a; }
enum E0 { e0, e1, e2 };
int m16(int a) { return a; }
typedef int T0;
int m17(int a) { return a; }
int proto(int, int);
int m18(int a) { return a; }
extern int ext;
int m19(int a) { return a; }
int glob;
int m20(int a) { return a; }
struct A15 { A15(const A15 &); int x; };
int m21(int a) { return a; }
struct A16 { A16 &operator=(const A16 &); int x; };
int m22(int a) { return a; }
union U0 { int i; float f; };
int m23(int a) { return a; }










































