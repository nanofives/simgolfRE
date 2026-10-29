
__declspec(thread) int t_count = 1;
__declspec(dllimport) int i_data;
__declspec(dllimport) int i_func(int);
__declspec(dllexport) int e_func(int a) { return a + i_data + t_count; }
__declspec(naked) void naked_fn(void) { __asm ret }
#pragma code_seg(".mytext")
int in_seg(int a) { return i_func(a) * 2; }
#pragma code_seg()










