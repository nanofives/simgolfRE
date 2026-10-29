#define ASM_BEGIN __asm {
#undef ASM_BEGIN
int g;
int ASM_BEGIN; /* c */ int k;
void h(void)
{
    ASM_BEGIN = 1; /* d */ g = 2;
}
