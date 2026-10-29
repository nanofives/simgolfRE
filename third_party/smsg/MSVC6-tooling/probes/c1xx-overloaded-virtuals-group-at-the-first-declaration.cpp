struct V { virtual int a(); virtual int f(int); virtual int b(); virtual int f(char *); };
int V::a() { return 1; }
int V::f(int) { return 2; }
int V::b() { return 3; }
int V::f(char *) { return 4; }
V *make() { return new V; }
