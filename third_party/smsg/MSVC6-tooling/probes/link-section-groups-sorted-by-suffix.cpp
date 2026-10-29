#pragma code_seg(".text$b")
int fb(int a) { return a + 1; }
#pragma code_seg(".text$a")
int fa(int a) { return a + 2; }
#pragma code_seg()
int main() { return fa(1) + fb(2); }
