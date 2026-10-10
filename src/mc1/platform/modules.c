// LoadLibrary and GetProcAddress over the modules linked into the executable.

#include <windows.h>

#include "platform.h"
#include "q_Shared.h"

extern struct refexport_s GetRefAPI();

static const h2_symbol_t ref_soft_symbols[] =
{
	{ "GetRefAPI", (void*)GetRefAPI },
	{ NULL, NULL }
};

static const h2_module_t builtin_modules[] =
{
	{ "ref_soft", ref_soft_symbols },
	{ NULL, NULL }
};

static const h2_module_t* FindModule(const h2_module_t* modules, const char* name)
{
	for (const h2_module_t* m = modules; m->name != NULL; m++)
		if (Q_stricmp(m->name, name) == 0)
			return m;

	return NULL;
}

HINSTANCE LoadLibrary(const char* path)
{
	char name[MAX_QPATH];

	const char* slash = strrchr(path, '/');
	strcpy_s(name, sizeof(name), (slash != NULL ? slash + 1 : path));

	char* ext = strrchr(name, '.');
	if (ext != NULL)
		*ext = 0;

	const h2_module_t* module = FindModule(builtin_modules, name);
	if (module == NULL)
		module = FindModule(h2_modules, name);

	return (HINSTANCE)module;
}

void* GetProcAddress(HINSTANCE module, const char* name)
{
	if (module == NULL)
		return NULL;

	for (const h2_symbol_t* s = module->symbols; s->name != NULL; s++)
		if (strcmp(s->name, name) == 0)
			return s->address;

	return NULL;
}

BOOL FreeLibrary(HINSTANCE module)
{
	return TRUE;
}
