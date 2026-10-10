/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * Model memory: Hunk_Begin reserves the most a model can need, Hunk_End trims it to what was used.
 * Models hold pointers into their hunk, so the block must not move. Newlib's realloc() shrinks in
 * place; other C libraries may move the block, so the test build doesn't trim.
 */

#include "header/local.h"

#define CACHE_LINE_SIZE 32

static byte *membase;
static size_t maxhunksize;
static size_t curhunksize;

void *
Hunk_Begin(int maxsize)
{
	maxhunksize = maxsize + sizeof(size_t) + CACHE_LINE_SIZE;
	curhunksize = 0;

	membase = calloc(maxhunksize, 1);
	if (membase == NULL)
		ri.Sys_Error(ERR_FATAL, "Hunk_Begin: unable to allocate %d bytes", maxsize);

	*((size_t *)membase) = curhunksize;

	return membase + sizeof(size_t);
}

void *
Hunk_Alloc(int size)
{
	byte *buf;

	size = (size + (CACHE_LINE_SIZE - 1)) & ~(CACHE_LINE_SIZE - 1);

	if (curhunksize + size > maxhunksize)
		ri.Sys_Error(ERR_FATAL, "Hunk_Alloc overflow");

	buf = membase + sizeof(size_t) + curhunksize;
	curhunksize += size;

	return buf;
}

int
Hunk_End(void)
{
#ifdef MC1
	byte *n = realloc(membase, curhunksize + sizeof(size_t));

	if (n != membase)
		ri.Sys_Error(ERR_FATAL, "Hunk_End: realloc moved the hunk");
#endif

	*((size_t *)membase) = curhunksize + sizeof(size_t);

	return (int)curhunksize;
}

void
Hunk_Free(void *base)
{
	if (base)
		free((byte *)base - sizeof(size_t));
}
