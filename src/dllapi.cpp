#include <extdll.h>
#include <meta_api.h>
#include "bot.h"

static void OnServerActivate_Post(edict_t *, int, int)
{
	Bot_ServerActivate();
	RETURN_META(MRES_IGNORED);
}

static void OnServerDeactivate()
{
	Bot_ServerDeactivate();
	RETURN_META(MRES_IGNORED);
}

static void OnStartFrame()
{
	Bot_StartFrame();
	RETURN_META(MRES_IGNORED);
}

static void OnClientDisconnect(edict_t *ent)
{
	Bot_ClientDisconnect(ent);
	RETURN_META(MRES_IGNORED);
}

static void OnClientCommand(edict_t *ent)
{
	if (Bot_ClientCommand(ent))
		RETURN_META(MRES_SUPERCEDE);
	RETURN_META(MRES_IGNORED);
}

C_DLLEXPORT int GetEntityAPI2(DLL_FUNCTIONS *table, int *interfaceVersion)
{
	if (!table)
		return FALSE;
	if (*interfaceVersion != INTERFACE_VERSION)
	{
		*interfaceVersion = INTERFACE_VERSION;
		return FALSE;
	}
	memset(table, 0, sizeof(DLL_FUNCTIONS));
	table->pfnServerDeactivate = OnServerDeactivate;
	table->pfnStartFrame = OnStartFrame;
	table->pfnClientDisconnect = OnClientDisconnect;
	table->pfnClientCommand = OnClientCommand;
	return TRUE;
}

C_DLLEXPORT int GetEntityAPI2_Post(DLL_FUNCTIONS *table, int *interfaceVersion)
{
	if (!table)
		return FALSE;
	if (*interfaceVersion != INTERFACE_VERSION)
	{
		*interfaceVersion = INTERFACE_VERSION;
		return FALSE;
	}
	memset(table, 0, sizeof(DLL_FUNCTIONS));
	table->pfnServerActivate = OnServerActivate_Post;
	return TRUE;
}
