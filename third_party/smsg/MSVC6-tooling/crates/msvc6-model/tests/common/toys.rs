#![allow(dead_code)]

pub const TOYS: &[(&str, i64)] = &[
    ("enum E1 {};", 1),
    ("enum E1 { a1, a2 };", 3),
    ("enum { a1, a2 };", 3),
    ("enum E { a = 5, b = 7 };", 3),
    ("extern int v1;", 1),
    ("static int A[4] = { 1, 2, 3, 4 };", 1),
    ("typedef int T1; typedef int T2;", 2),
    ("typedef struct S1 { int m; } T1;", 10),
    ("typedef void (*FP)(int);", 2),
    ("struct S1;", 1),
    ("struct S1 { };", 8),
    ("struct S1 { int m1; int m2; int m3; int m4; };", 12),
    ("union U1 { int a; float b; };", 10),
    ("void g1(int, int);", 3),
    ("void g1(int a = 3);", 2),
    ("void RenderEmployees(void);", 2),
    ("void RenderEmployees();", 1),
    ("int g1(const char *f, ...);", 2),
    ("void g1(int (*cb)(int x));", 3),
    ("void g1(); void g1();", 2),
    ("int *g1(char *a, const int &b);", 3),
    ("extern \"C\" { int g1(int a); int g2(int b); }", 4),
    (
        "struct D { ~D(); }; void g(); extern \"C\" void use() { D d; g(); }",
        16,
    ),
    (
        "struct D { ~D(); }; void g(); extern \"C\" { void use() { D d; g(); } }",
        16,
    ),
    (
        "struct D { ~D(); }; void g(); extern \"C\" void use(); void use() { D d; g(); }",
        17,
    ),
    ("__declspec(dllimport) int __stdcall g1(int a);", 2),
    (
        "inline int h1(int a, int b) { int c = a + b; return c; }",
        5,
    ),
    ("void p() { }", 2),
    ("void p() { int a, b; }", 4),
    ("void q() { { int a; { int b; } } }", 4),
    ("void p(int a) { if (a) h2(); else o1 = 0; }", 5),
    (
        "int q() { if (o1) return 1; if (o1 == 2) return 3; return 2; }",
        4,
    ),
    ("void q() { for (int i = 0; i < o1; i++) h2(); }", 6),
    ("void q() { do h2(); while (o1); }", 5),
    ("void q() { while (o1) { if (o1 == 2) break; h2(); } }", 6),
    (
        "void q() { switch (o1) { case 1: h2(); break; case 2: o1 = 3; break; default: break; } }",
        9,
    ),
    (
        "void q() { switch (o1) { case 1: case 2: h2(); break; } }",
        7,
    ),
    ("void q() { goto x; x: h2(); }", 4),
    ("void q() { static int z; z = 1; }", 4),
    ("void q() { o1 = sizeof(int); }", 3),
    ("void q() { o1 = (int)(char)o1; }", 4),
    ("void q(void *p) { o1 = *(int *)p; }", 4),
    ("int q() { return o1 ? 1 : 2; }", 2),
    (
        "void p(const char *); void q() { p(\"x\"); } void r() { p(\"x\"); }",
        7,
    ),
    ("struct S1 { int m; S1(); };", 9),
    ("struct S1 { int m; S1(int a, int b); };", 11),
    ("struct S1 { int m; S1(); S1(int a); };", 12),
    ("struct S1 { int m; S1(const S1 &o); };", 7),
    ("struct S1 { int m; S1 &operator=(const S1 &o); };", 9),
    ("struct S1 { int m; S1() : m(0) { } };", 11),
    ("struct S1 { int m; ~S1(); };", 13),
    ("struct S1 { int m; ~S1() { h2(); } };", 15),
    ("struct S1 { int m; virtual ~S1(); };", 17),
    ("struct S1 { int m; virtual ~S1(); virtual void g(); };", 18),
    ("struct S1 { int m; virtual void g() = 0; };", 13),
    (
        "struct S1 { int m; virtual void g(); virtual void h(); };",
        14,
    ),
    ("struct S1 { static int s; };", 10),
    (
        "struct S1 { int m; static int g(int a) { return a; } };",
        12,
    ),
    ("struct S1 { int m; int g() const { return m; } };", 12),
    ("struct S1 { int m; void g(); }; void S1::g() { }", 13),
    ("struct S1 { struct In { int a; } in; int m; };", 19),
    (
        "struct B { int b; virtual void g(); }; struct S1 : B { int m; };",
        24,
    ),
    (
        "struct B { int b; virtual void g(); }; struct S1 : B { int m; void g(); };",
        25,
    ),
    ("struct B { int b; ~B(); }; struct S1 : B { int m; };", 26),
    ("struct B { int b; ~B(); }; struct S1 { B x; int m; };", 27),
    (
        "struct S1 { int m; }; void q() { S1 *p = new S1; delete p; }",
        15,
    ),
    ("class C { public: int m; private: int n; };", 10),
    (
        "class R { public: R() : m(0) {} unsigned int m; double F(); unsigned short N(int n); }; extern R g;",
        15,
    ),
    (
        "void q() { struct L { int a; }; L l; l.a = 1; o1 = l.a; }",
        12,
    ),
    ("static union { int a; float b; };", 13),
    (
        "static union { int a; float b; }; static union { int c; float d; };",
        26,
    ),
    ("union { int a; float b; } g_u;", 11),
    ("void q() { union { int a; float b; }; a = 1; }", 15),
    ("void q() { union { int a; float b; } u; u.a = 1; }", 13),
    ("struct S1 { int k; union { int a; float b; }; };", 19),
    ("struct S1 { int k; union { int a; float b; } u; };", 20),
    (
        "struct S1 { S1(); int m; }; S1 *q(int n) { return new S1[n]; }",
        29,
    ),
    (
        "struct S1 { S1(); int m; }; S1 *q(int n) { return new S1[4]; }",
        28,
    ),
];

pub const EH_TOYS: &[(&str, i64)] = &[
    ("void g(); void use() { try { g(); } catch (...) { } }", 11),
    (
        "void g(); void use() { try { g(); } catch (...) { } } void use2() { try { g(); } catch (...) { } }",
        15,
    ),
    (
        "void g(); void use() { try { g(); } catch (...) { } try { g(); } catch (...) { } }",
        12,
    ),
    ("void g(); void use() { try { g(); } catch (int) { } }", 15),
    (
        "void g(); void use() { try { g(); } catch (int) { } } void use2() { try { g(); } catch (int) { } }",
        20,
    ),
    (
        "void g(); void use() { try { g(); } catch (int) { } } void use2() { try { g(); } catch (char) { } }",
        22,
    ),
    (
        "void g(); void use() { try { g(); } catch (int) { } catch (char) { } }",
        18,
    ),
    (
        "void g(); void use() { try { g(); } catch (int) { } catch (...) { } }",
        15,
    ),
    (
        "void g(); void use() { try { g(); } catch (int *) { } }",
        15,
    ),
    (
        "struct E { int a; }; void g(); void use() { try { g(); } catch (E &) { } }",
        24,
    ),
    ("void use() { throw 1; }", 21),
    ("void use() { throw 1; } void use2() { throw 2; }", 25),
    ("void use() { throw 1; } void use2() { throw 'c'; }", 33),
    ("void use() { throw 1; throw 2; }", 23),
    ("void use() { throw (char)1; }", 22),
    ("struct E { int a; }; void use() { E e; throw e; }", 31),
    (
        "void g(); void use() { try { g(); } catch (...) { } } void use2() { throw 1; }",
        25,
    ),
    (
        "void use2() { throw 1; } void g(); void use() { try { g(); } catch (...) { } }",
        26,
    ),
    (
        "void g(); void use() { try { g(); } catch (char) { } } void use2() { throw 1; }",
        28,
    ),
    (
        "void g(); void use() { try { g(); } catch (int) { } } void use2() { throw 1; }",
        26,
    ),
    (
        "void use2() { throw 1; } void g(); void use() { try { g(); } catch (int) { } }",
        27,
    ),
    (
        "void g(); void use() { try { g(); } catch (int e) { } }",
        15,
    ),
    (
        "void g(); void use() { try { g(); } catch (int e) { e = 1; } }",
        15,
    ),
    (
        "void g(); void use() { try { g(); } catch (...) { throw; } }",
        12,
    ),
    (
        "void g(); void use() { try { g(); } catch (...) { throw; } try { g(); } catch (...) { throw; } }",
        13,
    ),
    (
        "void g(); void use() { try { try { g(); } catch (...) { } } catch (...) { } }",
        13,
    ),
    (
        "void g(); void use() { try { g(); try { g(); } catch (...) { } } catch (...) { } }",
        13,
    ),
    (
        "void g(); void use() { try { g(); } catch (...) { try { g(); } catch (...) { } } }",
        14,
    ),
    (
        "void g(); int use() { try { g(); } catch (int) { return 1; } return 0; }",
        17,
    ),
    (
        "void g(); int use() { try { g(); return 1; } catch (int) { } return 0; }",
        15,
    ),
    (
        "void g(); int use() { try { g(); } catch (int e) { return e; } return 0; }",
        17,
    ),
    (
        "void g(); void use() { try { g(); } catch (...) { return; } }",
        11,
    ),
    (
        "void g(); int use() { try { g(); } catch (...) { return 1; } try { g(); } catch (...) { return 2; } return 0; }",
        16,
    ),
    (
        "void g(); void use() { for (;;) { try { g(); } catch (...) { break; } } }",
        14,
    ),
    (
        "void g(); void use() { try { g(); } catch (...) { goto out; } out: ; }",
        13,
    ),
    ("struct D { ~D(); }; void g(); void use() { D d; g(); }", 22),
    ("struct D { ~D(); }; void g(); void use() { D d; }", 16),
    (
        "struct D { ~D(); }; void g(); extern \"C++\" void use() { D d; g(); }",
        22,
    ),
    (
        "struct D { ~D(); }; void g(); void use() { D d; D e; g(); }",
        23,
    ),
    (
        "struct D { ~D(); }; void g(); void use() { D d; g(); D e; g(); D f; g(); }",
        24,
    ),
    (
        "struct D { ~D(); }; void g(); void use() { D d; g(); { D e; g(); } }",
        23,
    ),
    (
        "struct D { ~D(); }; void g(); void use() { g(); { D d; g(); } }",
        22,
    ),
    (
        "struct D { ~D(); }; void g(); void use() { D d; g(); } void use2() { D d; g(); }",
        26,
    ),
    (
        "struct D { ~D(); }; void g(); void use() { D d; g(); } void use2() { D d; g(); } void use3() { D d; g(); }",
        30,
    ),
    (
        "struct D { ~D(); }; void g(); void use() { D d; g(); try { g(); } catch (...) { } }",
        25,
    ),
    (
        "struct D { ~D(); }; void g(); void use() { try { g(); } catch (...) { } } void use2() { D d; g(); }",
        27,
    ),
    (
        "struct D { ~D(); }; void g(); void use() { D d; g(); } void use2() { try { g(); } catch (...) { } }",
        26,
    ),
    (
        "struct D { ~D(); }; void g(); void use() { D d; g(); } void use2() { throw 1; }",
        36,
    ),
    (
        "struct D { int a; }; void g(); void use() { D d; g(); }",
        13,
    ),
];

pub const TEMPLATE_TOYS: &[(&str, i64)] = &[
    ("template<class T> struct S { };", 2),
    ("template<class T> struct S { T a; };", 3),
    ("template<class T> struct S { T a; T b; };", 4),
    ("template<class T, class U> struct S { T a; U b; };", 4),
    (
        "template<class T> struct S { T a; T get() { return a; } };",
        4,
    ),
    (
        "template<class T> struct S { T a; T get() { return a; } T put(T v) { a = v; return v; } };",
        6,
    ),
    ("template<int N> struct S { int a[N]; };", 4),
    ("template<class T> struct S { T a; S(); ~S(); };", 5),
    ("template<class T> struct S { T a; virtual int f(); };", 4),
    ("template<class T> struct S { static int n; T a; };", 4),
    ("template<class T> struct S { T a; S<T> *next; };", 4),
    (
        "struct B { int b; }; template<class T> struct S : B { T a; };",
        12,
    ),
    ("template<class T> struct S { }; S<int> si;", 12),
    ("template<class T> struct S { T a; }; S<int> si;", 14),
    ("template<class T> struct S { T a; T b; }; S<int> si;", 16),
    (
        "template<class T> struct S { T a; }; S<int> si; S<int> sj;",
        15,
    ),
    (
        "template<class T> struct S { T a; }; S<int> si; S<char> sc;",
        25,
    ),
    (
        "template<class T> struct S { T a; }; S<int> si; S<int> *sp;",
        15,
    ),
    (
        "template<class T> struct S { T a; }; S<int> si; S<int> sj; S<char> *sp;",
        17,
    ),
    (
        "template<class T> struct S { T a; T get() { return a; } }; S<int> si;",
        16,
    ),
    (
        "template<class T> struct S { T a; T get() { return a; } }; S<int> si; int use() { return si.get(); }",
        18,
    ),
    (
        "template<class T> struct S { T a; T get() { return a; } T put(T v) { a = v; return v; } }; S<int> si;",
        20,
    ),
    (
        "template<class T> struct S { T a; int f() { return a; } }; S<int> si; S<char> sc;",
        28,
    ),
    ("template<int N> struct S { int a[N]; }; S<4> si;", 15),
    (
        "template<class T> struct S { T a; S<T> *next; }; S<int> si;",
        16,
    ),
    (
        "template<class T> struct S { T a; static int n; }; S<int> si;",
        17,
    ),
    (
        "struct B { int b; }; template<class T> struct S : B { T a; }; S<int> si;",
        23,
    ),
    (
        "template<class T> struct S { T a; S(); ~S(); }; S<int> *sp;",
        7,
    ),
    (
        "template<class T> struct S { T a; virtual int f(); }; S<int> *sp;",
        6,
    ),
    ("template<class T> struct S { T a; }; typedef S<int> SI;", 5),
    ("template<class T> struct S { T a; }; void u(S<int> *p);", 6),
    (
        "template<class T> struct S { T a; }; template class S<int>;",
        13,
    ),
    (
        "template<class T> struct S { T a; }; void u() { S<int> x; x.a = 1; }",
        16,
    ),
    (
        "template<class T> struct S { T a; int f(int k); }; template<class T> int S<T>::f(int k) { return k; }",
        7,
    ),
    (
        "template<class T> struct S { T a; int f(int k); }; template<class T> int S<T>::f(int k) { return k; } S<int> si;",
        20,
    ),
    (
        "template<class T> T mx(T a, T b) { return a < b ? b : a; }",
        3,
    ),
    (
        "struct B { int b; }; template<class T> struct S : virtual B { T a; };",
        12,
    ),
    (
        "template<class T> struct S { virtual int F(T *e) { return 0; } virtual void R(T *e) {} };",
        6,
    ),
    (
        "template<class T> struct S { int G() const { return 0; } };",
        3,
    ),
    ("template<class T> struct S { S() { } ~S() { } };", 4),
    (
        "template<class T> struct S { int F(S<T> *n) { return 0; } int F(int k); };",
        7,
    ),
    (
        "template<class T> struct S { int F(int *n); int F(int k); int F(char c); };",
        10,
    ),
    ("struct S { int F(int *n); int F(int k); };", 13),
    (
        "struct S { int F(int *n); int F(int k); int F(char c); };",
        16,
    ),
    ("int F(int *n); int F(int k);", 5),
];

pub const VBASE_TOYS: &[(&str, i64)] = &[
    ("struct B { int b; }; struct D : B { int d; };", 18),
    ("struct B { int b; }; struct D : virtual B { int d; };", 21),
    (
        "struct B1 { int b; }; struct B2 { int c; }; struct D : B1, B2 { int d; };",
        27,
    ),
    (
        "struct B1 { int b; }; struct B2 { int c; }; struct D : virtual B1, virtual B2 { int d; };",
        30,
    ),
    (
        "struct B { virtual void f(); int b; }; struct D : B { int d; };",
        24,
    ),
    (
        "struct B { virtual void f(); int b; }; struct D : virtual B { int d; };",
        27,
    ),
    ("struct B { int b; }; struct D : B { D(); int d; };", 18),
    (
        "struct B { int b; }; struct D : virtual B { D(); int d; };",
        21,
    ),
    ("struct B { int b; }; struct D : B { ~D(); int d; };", 22),
    (
        "struct B { int b; }; struct D : virtual B { ~D(); int d; };",
        25,
    ),
    (
        "struct B { virtual void f(); int b; }; struct D : virtual B { virtual void g(); int d; };",
        31,
    ),
    (
        "struct B { int b; }; struct D : virtual B { int d; }; struct E : D { int e; };",
        32,
    ),
    (
        "struct B { int b; }; struct D : B { int d; }; struct E : D { int e; };",
        27,
    ),
    (
        "struct B { int b; }; struct D1 : virtual B { int d; }; struct D2 : virtual B { int d2; }; struct E : D1, D2 { int e; };",
        46,
    ),
    (
        "struct B { int b; }; struct D1 : B { int d; }; struct D2 : B { int d2; }; struct E : D1, D2 { int e; };",
        36,
    ),
    (
        "struct B { ~B(); int b; }; struct D : virtual B { int d; };",
        30,
    ),
    ("struct B { ~B(); int b; }; struct D : B { int d; };", 26),
    (
        "struct B { virtual ~B(); int b; }; struct D : virtual B { int d; };",
        37,
    ),
    (
        "struct B { virtual ~B(); int b; }; struct D : B { int d; };",
        33,
    ),
    (
        "struct B1 { int b; }; struct B2 { int c; }; struct D : virtual B1, B2 { int d; };",
        30,
    ),
    (
        "struct B1 { int b; }; struct B2 { int c; }; struct B3 { int e; }; struct D : virtual B1, virtual B2, virtual B3 { int d; };",
        39,
    ),
    (
        "struct B1 { int b; }; struct B2 { int c; }; struct B3 { int e; }; struct D : B1, B2, B3 { int d; };",
        36,
    ),
    (
        "struct B { virtual void f(); int b; }; struct D : virtual B { D(); virtual ~D(); int d; };",
        35,
    ),
    (
        "struct B { ~B(); int b; }; struct D : virtual B { ~D(); int d; };",
        30,
    ),
    (
        "struct B { int b; }; struct D1 : virtual B { int d; }; struct D2 { int d2; }; struct E : D1, D2 { int e; };",
        41,
    ),
    (
        "struct B { int b; }; struct D1 : virtual B { int d; }; struct D2 : virtual B { int d2; }; struct D3 : virtual B { int d3; }; struct E : D1, D2, D3 { int e; };",
        60,
    ),
    (
        "struct P1 { virtual ~P1(); virtual void f(); int a; }; struct P2 { virtual ~P2(); virtual void g(); int b; }; struct D : virtual P1, virtual P2 { D(); virtual ~D(); int d; };",
        58,
    ),
    (
        "struct P1 { virtual ~P1(); virtual void f(); int a; }; struct P2 { virtual ~P2(); virtual void g(); int b; }; struct D : virtual P1, virtual P2 { int d; };",
        58,
    ),
    (
        "struct P1 { virtual void f(); int a; }; struct P2 { virtual void g(); int b; }; struct D : virtual P1, virtual P2 { void f(); int d; };",
        43,
    ),
    (
        "struct P1 { virtual void f(); int a; }; struct P2 { virtual void g(); int b; }; struct D : virtual P1, virtual P2 { D(); void f(); int d; };",
        43,
    ),
    (
        "struct N { virtual void h(); int n; }; struct P { virtual ~P(); int a; }; struct D : N, virtual P { int d; };",
        52,
    ),
    (
        "struct P1 { virtual ~P1(); virtual void f(); int a; }; struct P2 { virtual ~P2(); virtual void g(); int b; }; struct D : virtual P1, virtual P2 { D(); virtual ~D(); void f(); void g(); int d; };",
        60,
    ),
    (
        "struct P1 { virtual void f(); int a; }; struct D : virtual P1 { void f(); int d; };",
        28,
    ),
    (
        "struct P1 { virtual void f(); int a; }; struct D : virtual P1 { D(); void f(); int d; };",
        28,
    ),
    (
        "struct P1 { virtual void f(); int a; }; struct P2 { virtual void g(); int b; }; struct D : virtual P1, virtual P2 { int d; };",
        42,
    ),
    (
        "struct P1 { virtual void f(); int a; }; struct D : virtual P1 { D(); int d; };",
        27,
    ),
    (
        "struct B { int b; }; struct D : virtual B { D(); ~D(); int d; };",
        25,
    ),
    (
        "struct B { int b; }; struct D1 : virtual B { int d; }; struct E : D1 { E(); ~E(); int e; };",
        36,
    ),
    (
        "struct P1 { virtual ~P1(); int a; }; struct D : virtual P1 { int d; };",
        37,
    ),
    (
        "struct P1 { virtual ~P1(); int a; }; struct D : virtual P1 { virtual ~D(); int d; };",
        37,
    ),
    (
        "struct P1 { virtual void f(); int a; }; struct D : virtual P1 { int d; }; struct E : D { int e; };",
        40,
    ),
    (
        "struct B { int b; }; struct D : virtual B { int d; }; struct E : D { virtual void h(); int e; };",
        36,
    ),
    (
        "struct P1 { virtual void f(); int a; }; struct D : virtual P1 { int d; }; struct E : D { virtual void h(); int e; };",
        44,
    ),
];

pub const NEW_DELETE_TOYS: &[(&str, i64, i64)] = &[
    (
        "struct S { S(); int m; }; void q() { S *p = new S; }",
        16,
        14,
    ),
    (
        "struct S { S(); int m; }; void q() { S *p = new S; } void q2() { S *p = new S; }",
        23,
        19,
    ),
    (
        "struct S { S(); int m; }; void q() { S *p = new S[3]; }",
        28,
        26,
    ),
    (
        "struct S { S() {} int m; }; void q() { S *p = new S[3]; }",
        30,
        28,
    ),
    ("struct S { int m; }; void q() { S *p = new S[3]; }", 14, 14),
    (
        "struct S { S(); ~S(); int m; }; void q() { S *p = new S[3]; }",
        29,
        49,
    ),
    (
        "struct S { S(); virtual ~S(); int m; }; void q() { S *p = new S[3]; }",
        33,
        53,
    ),
    (
        "struct S { S(); virtual ~S(); int m; static S *N(int c) { return new S[c]; } };",
        34,
        54,
    ),
    (
        "struct S { S(); int m; }; void q() { S *p = new S[3]; S *r = new S[4]; }",
        34,
        30,
    ),
    (
        "struct S { S(); int m; }; struct T { T(); int m; }; void q() { S *p = new S[3]; T *r = new T[4]; }",
        43,
        39,
    ),
    (
        "struct S { S(); ~S(); int m; }; void q() { S *p = new S[3]; S *r = new S[4]; }",
        36,
        53,
    ),
    (
        "struct S { S(); ~S(); int m; }; void q() { S *p = new S[3]; delete [] p; }",
        31,
        51,
    ),
    (
        "struct S { S(); ~S(); int m; }; void q(S *p) { delete [] p; }",
        25,
        37,
    ),
    (
        "struct S { S(); int m; }; void q(S *p) { delete [] p; }",
        13,
        13,
    ),
    (
        "struct S { S(); ~S(); int m; }; struct T { T(); ~T(); int m; }; void q() { S *p = new S[3]; T *r = new T[4]; }",
        56,
        73,
    ),
    (
        "struct M { M() : o(0) {} int o; int a; }; struct F : M { F(); virtual ~F() { C(); } void C(); int b; static F *N(int c) { return new F[c]; } };",
        53,
        73,
    ),
    (
        "struct D { ~D(); int m; }; void q(D *p) { delete p; } void q2(D *p) { delete [] p; } void q3() { D *p = new D[3]; }",
        39,
        51,
    ),
    (
        "struct B { B(); ~B(); int m; }; void q(B *p) { delete p; } void q2(B *p) { delete p; } void q3(B *p) { delete [] p; }",
        39,
        51,
    ),
    (
        "struct B { B(); ~B(); int m; }; struct B2 { B2(); ~B2(); int m; }; void q1(B *p) { delete p; } void q2(B2 *p) { delete p; }",
        44,
        44,
    ),
    (
        "struct C { C(); virtual ~C(); int m; }; void q4(C *p) { delete p; } void q5(C *p) { delete p; }",
        27,
        27,
    ),
    (
        "struct A { A(); int m; }; void q1(int c) { A *p = new A[c]; } void q2(int c) { A *p = new A[c]; }",
        40,
        36,
    ),
    (
        "struct D { ~D(); int m; }; struct T { int m; }; void q3(int c) { D *p = new D[c]; } void q4(int c) { T *p = new T[c]; } void q5(int c) { char *p = new char[c]; }",
        47,
        59,
    ),
    (
        "struct A { A(); int m; }; inline void *operator new(unsigned int n, void *a) { return a; } void q1(void *a) { A *p = new (a) A; } void q3() { A *p = new A(); }",
        29,
        25,
    ),
    (
        "struct A { A(); int m; }; struct E { virtual void v(); int m; }; struct G : A { int g; }; struct H { A a; int h; }; void q1() { E *p = new E; } void q2() { E *p = new E; } void q3() { G *p = new G; } void q4() { H *p = new H; }",
        75,
        67,
    ),
    (
        "struct E { virtual void v(); int m; }; void q1() { E e; } void q2() { E *p = new E; }",
        25,
        23,
    ),
    (
        "struct B { B(); ~B(); int m; }; void q1(int c) { B *p = new B[c+1]; } void q2(int c) { B *p = new B[3]; } void q3(int c) { B *p = new B[c]; }",
        52,
        66,
    ),
    (
        "struct B { B(); ~B(); int m; }; struct A { A(); int m; }; void q1() { B *p = new B[3]; } void q4() { A *p = new A[3]; }",
        57,
        64,
    ),
];
