

int g_init = 5;
int g_zero;
static int s_file = 7;
extern int g_ext;
int g_arr[3] = { 1, 2, 3 };
const char *g_str = "hello";

int globals(void)
{
    return g_init + g_zero + s_file + g_ext + g_arr[2] + g_str[1];
}




















