#define RUN(a, b) a b
int g;
int k;
void h(void)
{
    RUN(__asm, {)
        mov eax, 1 ; see /* here
        mov g, eax
        ; */
    }
    k = 2;
}
