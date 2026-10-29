#define ASM_BEGIN __asm {
#if 0
#undef ASM_BEGIN
#endif
int g;
void h(void)
{
    ASM_BEGIN
        mov eax, 1 ; see /* here
        mov g, eax
        ; */
    }
}
