
int f(void);
struct T { T(); ~T(); int v; };
int g_dyn = f();
T g_obj;
int dyninit(void) { return g_dyn + g_obj.v; }









