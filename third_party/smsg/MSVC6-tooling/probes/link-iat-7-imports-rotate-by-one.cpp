extern "C" {
__declspec(dllimport) unsigned long __stdcall GetTickCount(void);
__declspec(dllimport) unsigned long __stdcall GetCurrentProcessId(void);
__declspec(dllimport) unsigned long __stdcall GetCurrentThreadId(void);
__declspec(dllimport) unsigned long __stdcall GetLastError(void);
__declspec(dllimport) unsigned long __stdcall GetVersion(void);
__declspec(dllimport) char *__stdcall GetCommandLineA(void);
__declspec(dllimport) void *__stdcall GetProcessHeap(void);
}
int main()
{
    return (int)(GetTickCount() + GetCurrentProcessId() + GetCurrentThreadId() + GetLastError() + GetVersion() + (unsigned long)GetCommandLineA() + (unsigned long)GetProcessHeap());
}
