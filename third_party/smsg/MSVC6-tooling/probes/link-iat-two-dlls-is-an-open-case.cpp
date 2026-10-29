extern "C" {
__declspec(dllimport) unsigned long __stdcall GetTickCount(void);
__declspec(dllimport) unsigned long __stdcall GetCurrentProcessId(void);
__declspec(dllimport) unsigned long __stdcall GetCurrentThreadId(void);
__declspec(dllimport) unsigned long __stdcall GetLastError(void);
__declspec(dllimport) unsigned long __stdcall GetVersion(void);
__declspec(dllimport) char *__stdcall GetCommandLineA(void);
__declspec(dllimport) void *__stdcall GetProcessHeap(void);
__declspec(dllimport) void *__stdcall GetCurrentProcess(void);
__declspec(dllimport) void *__stdcall GetCurrentThread(void);
__declspec(dllimport) unsigned int __stdcall GetACP(void);
__declspec(dllimport) void *__stdcall GetDesktopWindow(void);
__declspec(dllimport) void *__stdcall GetActiveWindow(void);
__declspec(dllimport) void *__stdcall GetFocus(void);
__declspec(dllimport) void *__stdcall GetCapture(void);
__declspec(dllimport) unsigned long __stdcall GetMessagePos(void);
}
int main()
{
    return (int)(GetTickCount() + GetCurrentProcessId() + GetCurrentThreadId() + GetLastError() + GetVersion() + (unsigned long)GetCommandLineA() + (unsigned long)GetProcessHeap() + (unsigned long)GetCurrentProcess() + (unsigned long)GetCurrentThread() + GetACP() + (unsigned long)GetDesktopWindow() + (unsigned long)GetActiveWindow() + (unsigned long)GetFocus() + (unsigned long)GetCapture() + GetMessagePos());
}
