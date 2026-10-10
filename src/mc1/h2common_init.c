// Linked into the H2Common module, so it can reach DllMain after the module's symbols are hidden.

#include <windows.h>

extern BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);

__attribute__((visibility("default"))) void H2Common_Init(void)
{
	DllMain(NULL, DLL_PROCESS_ATTACH, NULL);
}
