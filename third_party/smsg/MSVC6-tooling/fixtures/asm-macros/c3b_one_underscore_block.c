#define AB _asm {
#define AE }
int g;
int k;
void h(void)
{
    AB
        mov eax, 1 ; see /* here
        mov g, eax
        ; */
    AE
    k = 1; /* e */ k = 2;
}
