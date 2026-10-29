#define ASM_BEGIN __asm {
#ifdef NEVER_DEFINED
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
