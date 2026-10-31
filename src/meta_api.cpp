#include <extdll.h>
#include <meta_api.h>
#include "bot.h"

meta_globals_t *gpMetaGlobals;
gamedll_funcs_t *gpGamedllFuncs;
mutil_funcs_t *gpMetaUtilFuncs;

plugin_info_t Plugin_info =
{
	META_INTERFACE_VERSION,
	"Crowbot",
	"0.1",
	__DATE__,
	"erikgraa",
	"https://github.com/erikgraa/crowbot-hldm",
	"CROWBOT",
	PT_ANYTIME,
	PT_ANYTIME,
};

C_DLLEXPORT int Meta_Query(char *, plugin_info_t **plinfo, mutil_funcs_t *pMetaUtilFuncs)
{
	*plinfo = &Plugin_info;
	gpMetaUtilFuncs = pMetaUtilFuncs;
	return TRUE;
}

static META_FUNCTIONS gMetaFunctionTable =
{
	NULL,                // GetEntityAPI
	NULL,                // GetEntityAPI_Post
	GetEntityAPI2,
	GetEntityAPI2_Post,
	NULL,                // GetNewDLLFunctions
	NULL,                // GetNewDLLFunctions_Post
	GetEngineFunctions,
	NULL,                // GetEngineFunctions_Post
};

C_DLLEXPORT int Meta_Attach(PLUG_LOADTIME, META_FUNCTIONS *pFunctionTable, meta_globals_t *pMGlobals, gamedll_funcs_t *pGamedllFuncs)
{
	gpMetaGlobals = pMGlobals;
	gpGamedllFuncs = pGamedllFuncs;
	memcpy(pFunctionTable, &gMetaFunctionTable, sizeof(META_FUNCTIONS));
	Bot_Init();
	return TRUE;
}

C_DLLEXPORT int Meta_Detach(PLUG_LOADTIME, PL_UNLOAD_REASON)
{
	return TRUE;
}
