#define ASM_BEGIN __asm {
#undef ASM_BEGIN
#define ASM_BEGIN {
int g;
void h(void)
{
    ASM_BEGIN g = 1; /* c */ g = 2; }
}
