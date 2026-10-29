#define ASM_BEGIN __asm {
int g;
int k;
void h(void)
{
    const char *s = "x"; ASM_BEGIN
        mov eax, 1 ; "ASM_BEGIN /* here
        mov g, eax
        ; */
    }
    s = "ASM_BEGIN"; k = 1; /* e */ k = 2;
}
