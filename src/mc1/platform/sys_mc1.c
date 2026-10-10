// System layer for MC1 and for the native test build: newlib/POSIX files and time, stdout console.

#include <ctype.h>
#include <dirent.h>
#include <sys/time.h>
#include <unistd.h>
#include <windows.h>

#if defined(MC1) && !defined(MR32SIM)
#include <mc1/newlib_integ.h>
#endif

#ifdef MC1
#include <mc1/mmio.h>
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

// With fixedtime set, the clock advances by exactly that much per frame so runs repeat exactly.
static qboolean virtual_clock;
static long long virtual_us = 1001;

long long Sys_Microseconds(void)
{
	static struct timeval start;
	struct timeval now;

	if (virtual_clock)
		return virtual_us;

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

// Matches * and ? wildcards, ignoring case.
static qboolean WildcardMatch(const char* pattern, const char* name)
{
	for (; *pattern != 0; pattern++, name++)
	{
		if (*pattern == '*')
		{
			for (const char* rest = name;; rest++)
			{
				if (WildcardMatch(pattern + 1, rest))
					return true;
				if (*rest == 0)
					return false;
			}
		}

		if (*name == 0)
			return false;

		if (*pattern != '?' && tolower((unsigned char)*pattern) != tolower((unsigned char)*name))
			return false;
	}

	return (*name == 0);
}

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

		if (!WildcardMatch(findpattern, d->d_name))
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
#ifndef MC1
	if (getcwd(buffer, len) != NULL)
		return true;
#endif

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

/*
 * Feeds console commands from the file named by the in_script cvar at given frames, one
 * "frame command" pair per line, for repeatable walkthroughs and benchmarks.
 */
static void RunInputScript(const int frame)
{
	static FILE* script;
	static qboolean opened;
	static int next_frame = -1;
	static char next_command[256];

	if (!opened)
	{
		const char* name = Cvar_VariableString("in_script");

		if (name[0] == 0)
			return;

		opened = true;
		script = fopen(name, "r");
		if (script == NULL)
			Com_Printf("Couldn't open input script %s\n", name);
	}

	while (script != NULL)
	{
		if (next_frame < 0)
		{
			char line[300];

			if (fgets(line, sizeof(line), script) == NULL)
			{
				fclose(script);
				script = NULL;
				return;
			}

			if (line[0] == '#' || sscanf(line, "%d %255[^\n]", &next_frame, next_command) != 2)
			{
				next_frame = -1;
				continue;
			}
		}

		if (next_frame > frame)
			return;

		Cbuf_AddText(va("%s\n", next_command));
		next_frame = -1;
	}
}

static unsigned long long CycleCount(void)
{
#ifdef MC1
	uint32_t hi, lo;

	do
	{
		hi = MMIO(CLKCNTHI);
		lo = MMIO(CLKCNTLO);
	} while (hi != MMIO(CLKCNTHI));

	return ((unsigned long long)hi << 32) | lo;
#else
	return (unsigned long long)Sys_Microseconds();
#endif
}

// Prints the cycle counter at frame prof_start and prof_frames frames later, then quits.
static void ProfileMarks(const int frame)
{
	const int start = (int)Cvar_VariableValue("prof_start");
	const int count = (int)Cvar_VariableValue("prof_frames");

	if (count <= 0)
		return;

	if (frame == start)
		Com_Printf("PROF_START cycles=%.0f frame=%d\n", (double)CycleCount(), frame);

	if (frame == start + count)
	{
		Com_Printf("PROF_END cycles=%.0f frame=%d\n", (double)CycleCount(), frame);
		Com_Quit();
	}
}

int main(int argc, char** argv)
{
#if defined(MC1) && !defined(MR32SIM)
	mc1newlib_init(MC1NEWLIB_ALL);
#endif
	H2Common_Init();
	Qcommon_Init(argc, argv);

	long long oldtime = Sys_Microseconds();

	for (int frame = 0;; frame++)
	{
		const int fixedtime = (int)Cvar_VariableValue("fixedtime");
		long long newtime;

		RunInputScript(frame);
		ProfileMarks(frame);

		virtual_clock = (fixedtime > 0);
		if (virtual_clock)
		{
			virtual_us = oldtime + fixedtime;
			newtime = virtual_us;
		}
		else
		{
			do
				newtime = Sys_Microseconds();
			while (newtime == oldtime);
		}

		curtime = (int)(newtime / 1000);

		Qcommon_Frame((int)(newtime - oldtime));
		oldtime = newtime;
	}
}
