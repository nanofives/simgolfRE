int f0(int x) { return x * 3 + 1; }
int d0(int x) { return x * 5 + 2; }
int f1(int x) { return x * 3 + 1; }
int d1(int x) { return x * 6 + 3; }
int f2(int x) { return x * 3 + 1; }
int d2(int x) { return x * 7 + 4; }
int f3(int x) { return x * 3 + 1; }
int d3(int x) { return x * 8 + 5; }
int f4(int x) { return x * 3 + 1; }
int d4(int x) { return x * 9 + 6; }
int main() { return f0(0) + d0(0) + f1(1) + d1(1) + f2(2) + d2(2) + f3(3) + d3(3) + f4(4) + d4(4); }
