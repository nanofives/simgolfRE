
#pragma data_seg(".mydata")
int g_seg = 1;
#pragma data_seg()
__declspec(selectany) int g_any = 2;
struct K { static int s_member; };
int K::s_member = 3;
const int g_konst = 4;
extern const int g_konst_ref = 5;

int storage(int a)
{
    register int r = a;
    return r + g_seg + g_any + K::s_member + g_konst + g_konst_ref;
}








