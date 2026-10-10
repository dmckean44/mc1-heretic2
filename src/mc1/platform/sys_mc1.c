// System layer for MC1 and for the native test build: newlib/POSIX files and time, stdout console.

#include <dirent.h>
#include <fnmatch.h>
#include <sys/time.h>
#include <unistd.h>
#include <windows.h>

#if defined(MC1) && !defined(MR32SIM)
#include <mc1/newlib_integ.h>
#endif

#include "platform.h"
#include "qcommon.h"
#include "client/input.h"
#include "win32/dll_io/dll_io.h"

int curtime;

H2R_NORETURN void Sys_Error(const char* error, ...)
{
	va_list argptr;
	char text[1024];

	va_start(argptr, error);
	vsnprintf(text, sizeof(text), error, argptr);
	va_end(argptr);

	CL_Shutdown();
	fprintf(stderr, "Error: %s\n", text);

	exit(1);
}

H2R_NORETURN void Sys_Quit(void)
{
	CL_Shutdown();
	exit(0);
}

void Sys_Init(void)
{
	Set_Com_Printf(Com_Printf);
}

char* Sys_ConsoleInput(void)
{
	return NULL;
}

void Sys_ConsoleOutput(const char* string)
{
	fputs(string, stdout);
	fflush(stdout);
}

void OutputDebugString(const char* text)
{
	fputs(text, stderr);
}

int MessageBox(HWND window, const char* text, const char* caption, unsigned type)
{
	fprintf(stderr, "%s: %s\n", caption, text);
	return 0;
}

void Sys_LoadGameDll(const char* dll_name, HINSTANCE* hinst, DWORD* checksum)
{
	*hinst = LoadLibrary(dll_name);
	if (*hinst == NULL)
		Sys_Error("Failed to load %s", dll_name);

	*checksum = 0;
}

void Sys_UnloadGameDll(const char* name, HINSTANCE* hinst)
{
	*hinst = NULL;
}

long long Sys_Microseconds(void)
{
	static struct timeval start;
	struct timeval now;

	gettimeofday(&now, NULL);
	if (start.tv_sec == 0 && start.tv_usec == 0)
	{
		start = now;
		start.tv_usec -= 1001;
	}

	return (long long)(now.tv_sec - start.tv_sec) * 1000000 + (now.tv_usec - start.tv_usec);
}

void Sys_Nanosleep(const int nanosec)
{
	const long long until = Sys_Microseconds() + nanosec / 1000;
	while (Sys_Microseconds() < until)
		;
}

void Sys_CpuPause(void)
{
}

void Sys_Mkdir(const char* path)
{
	mkdir(path, 0755);
}

qboolean Sys_IsDir(const char* path)
{
	struct stat st;
	return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

qboolean Sys_IsFile(const char* path)
{
	struct stat st;
	return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

static char findbase[MAX_OSPATH];
static char findpattern[MAX_OSPATH];
static char findpath[MAX_OSPATH];
static DIR* finddir;

static qboolean MatchesAttributes(const char* path, const uint musthave, const uint canthave)
{
	const qboolean is_dir = Sys_IsDir(path);

	if (is_dir && (canthave & SFF_SUBDIR))
		return false;

	return !((musthave & SFF_SUBDIR) && !is_dir);
}

char* Sys_FindNext(const uint musthave, const uint canthave)
{
	if (finddir == NULL)
		return NULL;

	const struct dirent* d;
	while ((d = readdir(finddir)) != NULL)
	{
		if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
			continue;

		if (fnmatch(findpattern, d->d_name, FNM_CASEFOLD) != 0)
			continue;

		Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase, d->d_name);
		if (MatchesAttributes(findpath, musthave, canthave))
			return findpath;
	}

	return NULL;
}

char* Sys_FindFirst(const char* path, const uint musthave, const uint canthave)
{
	if (finddir != NULL)
		Sys_Error("Sys_FindFirst called without close");

	COM_FilePath(path, findbase);
	if (findbase[0] == 0)
		strcpy_s(findbase, sizeof(findbase), ".");

	strcpy_s(findpattern, sizeof(findpattern), COM_SkipPath((char*)path));
	if (strcmp(findpattern, "*.*") == 0)
		strcpy_s(findpattern, sizeof(findpattern), "*");

	finddir = opendir(findbase);

	return Sys_FindNext(musthave, canthave);
}

void Sys_FindClose(void)
{
	if (finddir != NULL)
		closedir(finddir);

	finddir = NULL;
}

qboolean Sys_GetWorkingDir(char* buffer, const size_t len)
{
	if (getcwd(buffer, len) != NULL)
		return true;

	strcpy_s(buffer, len, ".");
	return true;
}

qboolean Sys_GetOSUserDir(char* buffer, const size_t len)
{
	buffer[0] = 0;
	return false;
}

#ifdef MR32SIM
size_t _getstacksize(void)
{
	return 8 * 1024 * 1024;
}
#endif

int main(int argc, char** argv)
{
#if defined(MC1) && !defined(MR32SIM)
	mc1newlib_init(MC1NEWLIB_ALL);
#endif
	H2Common_Init();
	Qcommon_Init(argc, argv);

	long long oldtime = Sys_Microseconds();

	while (true)
	{
		long long newtime;
		do
			newtime = Sys_Microseconds();
		while (newtime == oldtime);

		curtime = (int)(newtime / 1000);

		Qcommon_Frame((int)(newtime - oldtime));
		oldtime = newtime;
	}
}
