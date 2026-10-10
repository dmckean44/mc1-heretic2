// The part of the Windows API that Heretic2R's portable code uses: module loading (served by a table of
// statically linked modules, see modules.c), debug output and message boxes.

#pragma once

#include <stdint.h>

#define MAX_PATH 260

#define DLL_PROCESS_ATTACH 1
#define DLL_PROCESS_DETACH 0

#define MB_OK 0
#define MB_ICONWARNING 0x30

typedef int BOOL;
typedef uint32_t DWORD;
typedef char* LPSTR;
typedef void* LPVOID;
typedef void* HWND;
typedef struct h2_module_s* HINSTANCE;
typedef HINSTANCE HMODULE;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

HINSTANCE LoadLibrary(const char* name);
void* GetProcAddress(HINSTANCE module, const char* name);
BOOL FreeLibrary(HINSTANCE module);

void OutputDebugString(const char* text);
int MessageBox(HWND window, const char* text, const char* caption, unsigned type);

#ifdef __cplusplus
}
#endif
