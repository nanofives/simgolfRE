


struct VB { VB(); virtual ~VB(); virtual void f(); int v; };
struct P : virtual VB { P(); virtual ~P() { p = 1; } virtual void f(); int p; };
struct Q : virtual VB { Q(); virtual ~Q(); virtual void f(); int q; };
