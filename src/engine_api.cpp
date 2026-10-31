#include <extdll.h>
#include <meta_api.h>
#include "bot.h"

// Only the Cmd_* functions are hooked, so fake client commands (e.g. "use weapon_x")
// can be fed to the game DLL.
static int Hook_Cmd_Argc()
{
	if (Bot_FakeCmdActive())
		RETURN_META_VALUE(MRES_SUPERCEDE, Bot_FakeArgc());
	RETURN_META_VALUE(MRES_IGNORED, 0);
}

static const char *Hook_Cmd_Argv(int i)
{
	if (Bot_FakeCmdActive())
		RETURN_META_VALUE(MRES_SUPERCEDE, Bot_FakeArgv(i));
	RETURN_META_VALUE(MRES_IGNORED, NULL);
}

static const char *Hook_Cmd_Args()
{
	if (Bot_FakeCmdActive())
		RETURN_META_VALUE(MRES_SUPERCEDE, Bot_FakeArgs());
	RETURN_META_VALUE(MRES_IGNORED, NULL);
}

C_DLLEXPORT int GetEngineFunctions(enginefuncs_t *table, int *interfaceVersion)
{
	if (!table)
		return FALSE;
	if (*interfaceVersion != ENGINE_INTERFACE_VERSION)
	{
		*interfaceVersion = ENGINE_INTERFACE_VERSION;
		return FALSE;
	}
	memset(table, 0, sizeof(enginefuncs_t));
	table->pfnCmd_Argc = Hook_Cmd_Argc;
	table->pfnCmd_Argv = Hook_Cmd_Argv;
	table->pfnCmd_Args = Hook_Cmd_Args;
	return TRUE;
}
