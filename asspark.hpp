#pragma once

#include <extdll.h>
#include <meta_api.h>
#include <asext_api.h>
#include <signatures_template.h>

constexpr auto SPARK_NAME    = "SPARK";
constexpr auto SPARK_VERSION = "20260421";
constexpr auto SPARK_DATE    = "2026";
constexpr auto SPARK_AUTHOR  = "Dr.Abc";
constexpr auto SPARK_URL     = "SPARK";
constexpr auto SPARK_LOGTAG  = "SPARK";

constexpr size_t EXECUTE_VTABLE_INDEX = 5;
constexpr uint64_t HASH_SEED = 0x9e3779b9;
constexpr float SPARK_ON_DEFAULT = 1.0f;

constexpr auto CMD_SPARK_DUMP      = "spark_dump";
constexpr auto CMD_SPARK_DUMP_FILE = "spark_dump_file";
constexpr auto CMD_SPARK_CLEAR     = "spark_clear";
constexpr auto CVAR_SPARK_ON       = "spark_on";

constexpr auto DUMP_FORMAT = "|{}|{}|{}|{}|{}\n";

#define SPARK_PLUGIN_INFO() \
	plugin_info_t Plugin_info = { \
		META_INTERFACE_VERSION, \
		SPARK_NAME, SPARK_VERSION, SPARK_DATE, \
		SPARK_AUTHOR, SPARK_URL, SPARK_LOGTAG, \
		PT_ANYTIME, PT_STARTUP \
	}

#define REGISTER_SPARK_CMD(name, lambda) \
	g_engfuncs.pfnAddServerCommand(const_cast<char*>(name), lambda)

extern meta_globals_t* gpMetaGlobals;
extern gamedll_funcs_t* gpGamedllFuncs;
extern mutil_funcs_t* gpMetaUtilFuncs;
extern enginefuncs_t g_engfuncs;
extern globalvars_t* gpGlobals;

C_DLLEXPORT int GetEntityAPI2(DLL_FUNCTIONS*, int*);
C_DLLEXPORT int GetEntityAPI2_Post(DLL_FUNCTIONS*, int*);

void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax);
void GameInitPost();

void UTIL_LogPrintf(const char* fmt, ...);
