int g;
int Y;
#define X Y; __asm
#define Y X
void h(void)
{
    Y {
        mov eax, 1 ; c /* d
        mov g, eax
        ; */
    }
}
