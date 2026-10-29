
int __cdecl f_cdecl(int a);
int __stdcall f_std(int a);
int __fastcall f_fast(int a, int b);
int f_var(int a, ...);
static int f_static(int a) { return a + 1; }
extern "C" int f_c(int a);
__declspec(noreturn) void f_noreturn(void);

int conventions(int a)
{
    int r = f_cdecl(a) + f_std(a) + f_fast(a, 2) + f_var(a, 1, 2) + f_static(a) + f_c(a);
    if (r == 99)
        f_noreturn();
    return r;
}












