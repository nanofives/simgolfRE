

inline int twice(int v) { return v * 2; }
int second(int a);
int first(int a) { return twice(a) + second(a); }
int second(int a) { return twice(a + 1); }







