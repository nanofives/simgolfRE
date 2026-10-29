#ifdef USE_ASM
#define ASM_BEGIN __asm {
#endif
int g;
void h(void)
{
#ifdef USE_ASM
    ASM_BEGIN mov g, 1 }
#else
    int ASM_BEGIN; /* c */ g = 2; {
    }
#endif
}
