extern "C" __declspec(dllimport) unsigned long __stdcall GetTickCount(void);
int main() { return (int)GetTickCount(); }
