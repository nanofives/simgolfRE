#define ASM_BEGIN {
int g;
void f(void)
{
    ASM_BEGIN g = 1; /* c */ g = 2; }
}
#undef ASM_BEGIN
#define ASM_BEGIN __asm {
void h(void)
{
    ASM_BEGIN
        mov eax, 1 ; see /* here
        mov g, eax
        ; */
    }
}
