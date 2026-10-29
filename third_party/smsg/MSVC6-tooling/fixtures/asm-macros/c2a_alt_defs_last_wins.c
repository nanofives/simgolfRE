#ifndef USE_C
#define BEGIN_ASM __asm {
#else
#define BEGIN_ASM {
#endif
int g;
void h(void)
{
    BEGIN_ASM
        mov eax, 1 ; see /* here
        mov g, eax
        ; */
    }
}
