#include "asspark.hpp"

#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <h_export.h>
#include <enginecallback.h>

enginefuncs_t g_engfuncs;
globalvars_t* gpGlobals;

void WINAPI GiveFnptrsToDll(enginefuncs_t* pengfuncsFromEngine, globalvars_t* pGlobals) {
	memcpy(&g_engfuncs, pengfuncsFromEngine, sizeof(enginefuncs_t));
	gpGlobals = pGlobals;
}

void UTIL_LogPrintf(const char* fmt, ...) {
	va_list argptr;
	char string[1024];
	va_start(argptr, fmt);
#if defined(_WIN32) || defined(_MSC_VER)
	_vsnprintf(string, sizeof(string), fmt, argptr);
#else
	vsnprintf(string, sizeof(string), fmt, argptr);
#endif
	va_end(argptr);
	ALERT(at_logged, "%s", string);
}

static DLL_FUNCTIONS gFunctionTable;
static DLL_FUNCTIONS gFunctionTable_Post;

namespace {
	bool InitTables() {
		gFunctionTable.pfnServerActivate = ServerActivate;
		gFunctionTable_Post.pfnGameInit = GameInitPost;
		return true;
	}
	bool g_tablesInited = InitTables();
}

C_DLLEXPORT int GetEntityAPI2(DLL_FUNCTIONS* pFunctionTable, int* interfaceVersion) {
	if (!pFunctionTable) {
		UTIL_LogPrintf("GetEntityAPI2 called with null pFunctionTable");
		return false;
	}
	if (*interfaceVersion != INTERFACE_VERSION) {
		UTIL_LogPrintf("GetEntityAPI2 version mismatch; requested=%d ours=%d",
			*interfaceVersion, INTERFACE_VERSION);
		*interfaceVersion = INTERFACE_VERSION;
		return false;
	}
	memcpy(pFunctionTable, &gFunctionTable, sizeof(DLL_FUNCTIONS));
	return true;
}

C_DLLEXPORT int GetEntityAPI2_Post(DLL_FUNCTIONS* pFunctionTable, int* interfaceVersion) {
	if (!pFunctionTable) {
		UTIL_LogPrintf("GetEntityAPI2_Post called with null pFunctionTable");
		return false;
	}
	if (*interfaceVersion != INTERFACE_VERSION) {
		UTIL_LogPrintf("GetEntityAPI2_Post version mismatch; requested=%d ours=%d",
			*interfaceVersion, INTERFACE_VERSION);
		*interfaceVersion = INTERFACE_VERSION;
		return false;
	}
	memcpy(pFunctionTable, &gFunctionTable_Post, sizeof(DLL_FUNCTIONS));
	return true;
}
