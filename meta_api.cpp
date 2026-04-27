/*
 * Copyright (c) 2001-2006 Will Day <willday@hpgx.net>
 *  Metamod - GNU General Public License v2
 */

#include "asspark.hpp"

mBOOL dlclose_handle_invalid;
IMPORT_ASEXT_API_DEFINE();

SPARK_PLUGIN_INFO();

meta_globals_t* gpMetaGlobals;
gamedll_funcs_t* gpGamedllFuncs;
mutil_funcs_t* gpMetaUtilFuncs;

static META_FUNCTIONS gMetaFunctionTable = {
	nullptr,          // pfnGetEntityAPI
	nullptr,          // pfnGetEntityAPI_Post
	GetEntityAPI2,    // pfnGetEntityAPI2
	GetEntityAPI2_Post,
	nullptr,          // pfnGetNewDLLFunctions
	nullptr,          // pfnGetNewDLLFunctions_Post
	nullptr,          // pfnGetEngineFunctions
	nullptr,          // pfnGetEngineFunctions_Post
	nullptr,          // pfnGetStudioBlendingInterface
	nullptr,          // pfnGetStudioBlendingInterface_Post
};

C_DLLEXPORT int Meta_Query(const char* interfaceVersion,
	plugin_info_t** pPlugInfo, mutil_funcs_t* pMetaUtilFuncs) {
	if (std::strcmp(interfaceVersion, META_INTERFACE_VERSION) != 0) {
		pMetaUtilFuncs->pfnLogError(PLID,
			"Meta_Query version mismatch! expect %s but got %s",
			META_INTERFACE_VERSION, interfaceVersion);
		return FALSE;
	}
	*pPlugInfo = &Plugin_info;
	gpMetaUtilFuncs = pMetaUtilFuncs;
	return TRUE;
}

C_DLLEXPORT int Meta_Attach(PLUG_LOADTIME, META_FUNCTIONS* pFunctionTable,
	meta_globals_t* pMGlobals, gamedll_funcs_t* pGamedllFuncs) {
	if (!pMGlobals) {
		LOG_ERROR(PLID, "Meta_Attach called with null pMGlobals");
		return FALSE;
	}
	if (!pFunctionTable) {
		LOG_ERROR(PLID, "Meta_Attach called with null pFunctionTable");
		return FALSE;
	}
	gpMetaGlobals = pMGlobals;
	memcpy(pFunctionTable, &gMetaFunctionTable, sizeof(META_FUNCTIONS));
	gpGamedllFuncs = pGamedllFuncs;

	if (!gpMetaUtilFuncs->pfnGetEngineHandle()) {
		LOG_ERROR(PLID, "engine handle not found!");
		return FALSE;
	}
	if (!gpMetaUtilFuncs->pfnGetEngineBase()) {
		LOG_ERROR(PLID, "engine base not found!");
		return FALSE;
	}

	void* asextHandle = nullptr;
#ifdef _WIN32
	LOAD_PLUGIN(PLID, "addons/metamod/dlls/asext.dll",
		PLUG_LOADTIME::PT_ANYTIME, &asextHandle);
#else
	LOAD_PLUGIN(PLID, "addons/metamod/dlls/asext.so",
		PLUG_LOADTIME::PT_ANYTIME, &asextHandle);
#endif
	if (!asextHandle) {
		LOG_ERROR(PLID, "asext dll handle not found!");
		return FALSE;
	}
	IMPORT_ASEXT_API(asext);
	return TRUE;
}

C_DLLEXPORT int Meta_Detach(PLUG_LOADTIME, PL_UNLOAD_REASON) {
	return TRUE;
}
