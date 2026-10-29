#ifdef USE_ASM
#define ASM_BEGIN __asm {
#endif
int g;
void h(void)
{
#ifdef USE_ASM
    ASM_BEGIN
        mov eax, 1 ; see /* here
        mov g, eax
        ; */
    }
#endif
    g = 3;
}
