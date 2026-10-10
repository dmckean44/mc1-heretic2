// Force-included in every file: maps the MSVC runtime and keywords used by Heretic2R onto C99/POSIX.

#pragma once

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <alloca.h>
#include <sys/stat.h>

#define H2_DECLSPEC_dllexport __attribute__((visibility("default")))
#define H2_DECLSPEC_dllimport
#define H2_DECLSPEC_noreturn __attribute__((noreturn))
#define H2_DECLSPEC_noinline __attribute__((noinline))
#define __declspec(x) H2_DECLSPEC_##x
#define __forceinline inline __attribute__((always_inline))
#define __cdecl
#define _inline static inline
#define WINAPI

#ifndef __cplusplus
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#endif

typedef int errno_t;

// MSVC gives struct tags first seen in a prototype file scope; C gives them prototype scope.
struct CE_DLight_s;
struct FormMove_s;
struct G_Message_s;
struct SinglyLinkedList_s;
struct Surface_s;
struct client_entity_s;

#ifdef __cplusplus
extern "C" {
#endif

static inline errno_t h2_strncpy(char* dst, size_t size, const char* src, size_t count)
{
	if (dst == NULL || size == 0)
		return EINVAL;

	size_t n = strlen(src);
	if (n > count)
		n = count;
	if (n > size - 1)
		n = size - 1;

	memmove(dst, src, n);
	dst[n] = 0;

	return 0;
}

static inline errno_t strcpy_s(char* dst, size_t size, const char* src)
{
	return h2_strncpy(dst, size, src, (size_t)-1);
}

static inline errno_t strncpy_s(char* dst, size_t size, const char* src, size_t count)
{
	return h2_strncpy(dst, size, src, count);
}

static inline errno_t strncat_s(char* dst, size_t size, const char* src, size_t count)
{
	const size_t len = strnlen(dst, size);
	return (len < size ? h2_strncpy(dst + len, size - len, src, count) : EINVAL);
}

static inline errno_t strcat_s(char* dst, size_t size, const char* src)
{
	return strncat_s(dst, size, src, (size_t)-1);
}

static inline errno_t memmove_s(void* dst, size_t size, const void* src, size_t count)
{
	memmove(dst, src, count < size ? count : size);
	return 0;
}

static inline errno_t memcpy_s(void* dst, size_t size, const void* src, size_t count)
{
	memcpy(dst, src, count < size ? count : size);
	return 0;
}

static inline errno_t fopen_s(FILE** f, const char* name, const char* mode)
{
	*f = fopen(name, mode);
	if (*f != NULL)
		return 0;

	return (errno != 0 ? errno : ENOENT);
}

static inline size_t fread_s(void* buf, size_t size, size_t elsize, size_t count, FILE* f)
{
	(void)size;
	return fread(buf, elsize, count, f);
}

static inline errno_t localtime_s(struct tm* out, const time_t* t)
{
	return (localtime_r(t, out) != NULL ? 0 : EINVAL);
}

#ifdef __cplusplus
}

static inline int sprintf_s(char* buf, size_t size, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	const int len = vsnprintf(buf, size, fmt, args);
	va_end(args);
	return len;
}

template <size_t N> static inline int sprintf_s(char (&buf)[N], const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	const int len = vsnprintf(buf, N, fmt, args);
	va_end(args);
	return len;
}

static inline int vsprintf_s(char* buf, size_t size, const char* fmt, va_list args)
{
	return vsnprintf(buf, size, fmt, args);
}

template <size_t N> static inline int vsprintf_s(char (&buf)[N], const char* fmt, va_list args)
{
	return vsnprintf(buf, N, fmt, args);
}

template <size_t N> static inline errno_t strcpy_s(char (&dst)[N], const char* src)
{
	return strcpy_s(dst, N, src);
}

template <size_t N> static inline errno_t strcat_s(char (&dst)[N], const char* src)
{
	return strcat_s(dst, N, src);
}

extern "C" {
#else
#define sprintf_s snprintf
#define vsprintf_s vsnprintf
#endif
#define sscanf_s sscanf
#define strtok_s strtok_r
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define stricmp strcasecmp
#define strnicmp strncasecmp
#define _strdup strdup
#define _mkdir(path) mkdir((path), 0755)

#ifdef __cplusplus
}
#endif
