#include "jgldefs.h"
#include "jglwin.h"
#include "jglpng.h"
extern "C" void __cdecl user_error_fn(png_struct *png_ptr, const char *message)
{
    OutputDebugStringA(message);
}
