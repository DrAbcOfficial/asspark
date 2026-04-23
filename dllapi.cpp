#include <chrono>
#include <unordered_map>
#include <functional>
#include <memory>
#include <format>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include <extdll.h>
#include <meta_api.h>
#include <asext_api.h>

class asSparkContextItem {
public:
	asSparkContextItem() = default;
	asSparkContextItem(asIScriptContext* context) {
		const char* s{};
		line = context->GetLineNumber(1, &column, &s);
		section = s;
	}
	int line{};
	int column{};
	std::string section{};
	
};
class asSparkItem : public asSparkContextItem {
public:
	asSparkItem() = default;
	std::chrono::steady_clock::duration alltime{};
	uint64_t called{};
};
std::unordered_map<size_t, std::unique_ptr<asSparkItem>> s_All{};

static hook_t* hook_executeContext{};
static void (SC_SERVER_DECL* pfnOldExecuteContext)(asIScriptContext* ctx SC_SERVER_DUMMYARG_NOCOMMA) = nullptr;
static void SC_SERVER_DECL ExecuteContext(asIScriptContext* ctx SC_SERVER_DUMMYARG_NOCOMMA) {
	if (CVAR_GET_FLOAT("spark_on") <= 0) {
		pfnOldExecuteContext(ctx, SC_SERVER_PASS_DUMMYARG_NOCOMMA);
		return;
	}
	constexpr auto to_hash = [](int line, int column, const char* section) {
		size_t h = std::hash<int>{}(line);
		h ^= std::hash<int>{}(column)+0x9e3779b9 + (h << 6) + (h >> 2);
		h ^= std::hash<std::string>{}(section)+0x9e3779b9 + (h << 6) + (h >> 2);
		return h;
	};
	int column{};
	const char* section{};
	int line = ctx->GetLineNumber(0, &column, &section);
	if (section == nullptr) {
		pfnOldExecuteContext(ctx, SC_SERVER_PASS_DUMMYARG_NOCOMMA);
		return;
	}

	size_t hash = to_hash(line, column, section);
	auto start = std::chrono::high_resolution_clock::now();
	pfnOldExecuteContext(ctx, SC_SERVER_PASS_DUMMYARG_NOCOMMA);
	auto end = std::chrono::high_resolution_clock::now();
	auto used_time = end - start;

	auto iter = s_All.find(hash);
	if (iter != s_All.end()) {
		iter->second->alltime += used_time;
		iter->second->called++;
	}
	else {
		auto allItem = std::make_unique<asSparkItem>();
		allItem->line = line;
		allItem->column = column;
		allItem->section = section;
		allItem->called = 1;
		allItem->alltime = used_time;
		s_All.emplace(hash, std::move(allItem));
	}
}

static bool s_hooked = false;
static void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax) {
	if (s_hooked) {
		SET_META_RESULT(MRES_IGNORED);
		return;
	}
	
	static auto serverManager = ASEXT_GetServerManager();
	if (!serverManager) {
		LOG_ERROR(PLID, "ERROR in angelscript hook: Server Manager is NULL!");
		SET_META_RESULT(MRES_IGNORED);
		return;
	}
	static auto& engine = serverManager->scriptEngine;
	if (!engine) {
		LOG_ERROR(PLID, "ERROR in angelscript hook: Engine is NULL!");
		SET_META_RESULT(MRES_IGNORED);
		return;
	}

	constexpr size_t EXCUTE_INDEX = 5;
	asIScriptContext* ctx = engine->RequestContext();
	void* pfnExecute = (*(void***)(ctx))[EXCUTE_INDEX];
	hook_executeContext = gpMetaUtilFuncs->pfnInlineHook(pfnExecute, (void*)ExecuteContext, (void**)&pfnOldExecuteContext, false);
	engine->ReturnContext(ctx);

	s_hooked = true;
	SET_META_RESULT(MRES_HANDLED);
}

static DLL_FUNCTIONS gFunctionTable = {
	NULL,					// pfnGameInit
	NULL,					// pfnSpawn
	NULL,					// pfnThink
	NULL,					// pfnUse
	NULL,				// pfnTouch
	NULL,				// pfnBlocked
	NULL,					// pfnKeyValue
	NULL,					// pfnSave
	NULL,					// pfnRestore
	NULL,					// pfnSetAbsBox

	NULL,					// pfnSaveWriteFields
	NULL,					// pfnSaveReadFields

	NULL,					// pfnSaveGlobalState
	NULL,					// pfnRestoreGlobalState
	NULL,					// pfnResetGlobalState

	NULL,					// pfnClientConnect
	NULL,					// pfnClientDisconnect
	NULL,					// pfnClientKill
	NULL,					// pfnClientPutInServer
	NULL,					// pfnClientCommand
	NULL,					// pfnClientUserInfoChanged
	ServerActivate,					// pfnServerActivate
	NULL,					// pfnServerDeactivate

	NULL,					// pfnPlayerPreThink
	NULL,					// pfnPlayerPostThink

	NULL,					// pfnStartFrame
	NULL,					// pfnParmsNewLevel
	NULL,					// pfnParmsChangeLevel

	NULL,					// pfnGetGameDescription
	NULL,					// pfnPlayerCustomization

	NULL,					// pfnSpectatorConnect
	NULL,					// pfnSpectatorDisconnect
	NULL,					// pfnSpectatorThink

	NULL,					// pfnSys_Error

	NULL,					// pfnPM_Move
	NULL,					// pfnPM_Init
	NULL,					// pfnPM_FindTextureType

	NULL,					// pfnSetupVisibility
	NULL,					// pfnUpdateClientData
	NULL,					// pfnAddToFullPack
	NULL,					// pfnCreateBaseline
	NULL,					// pfnRegisterEncoders
	NULL,					// pfnGetWeaponData
	NULL,					// pfnCmdStart
	NULL,					// pfnCmdEnd
	NULL,					// pfnConnectionlessPacket
	NULL,					// pfnGetHullBounds
	NULL,					// pfnCreateInstancedBaselines
	NULL,					// pfnInconsistentFile
	NULL,					// pfnAllowLagCompensation
};
C_DLLEXPORT int GetEntityAPI2(DLL_FUNCTIONS* pFunctionTable,
	int* interfaceVersion) {
	if (!pFunctionTable) {
		UTIL_LogPrintf("GetEntityAPI2 called with null pFunctionTable");
		return false;
	}
	else if (*interfaceVersion != INTERFACE_VERSION) {
		UTIL_LogPrintf("GetEntityAPI2 version mismatch; requested=%d ours=%d", *interfaceVersion, INTERFACE_VERSION);
		//! Tell metamod what version we had, so it can figure out who is out of date.
		*interfaceVersion = INTERFACE_VERSION;
		return false;
	}
	memcpy(pFunctionTable, &gFunctionTable, sizeof(DLL_FUNCTIONS));
	return true;
}

static void GameInitPost() {
	static cvar_t cvar_spark = { (char*)"spark_on",(char*)"0", FCVAR_SERVER | FCVAR_EXTDLL | FCVAR_ARCHIVE | FCVAR_PRINTABLEONLY, 1.0f, nullptr };
	CVAR_REGISTER(&cvar_spark);
	g_engfuncs.pfnAddServerCommand(const_cast<char*>("spark_dump"), [](){
		constexpr std::string_view print_format = "|{}|{}|{}|{}|{}|\n";
		g_engfuncs.pfnServerPrint((std::format(print_format, "Section", "Line", "Column", "Called", "Time(ms)")).c_str());
		for (const auto& [hash, item] : s_All) {
			auto us = std::chrono::duration_cast<std::chrono::microseconds>(item->alltime).count();
			g_engfuncs.pfnServerPrint((std::format(print_format, item->section, item->line, item->column, item->called, us)).c_str());
		}
	});
	g_engfuncs.pfnAddServerCommand(const_cast<char*>("spark_dump_file"), []() {
		constexpr std::string_view print_format = "|{}|{}|{}|{}|{}|\n";
		std::string buffer = std::format(print_format, "Section", "Line", "Column", "Called", "Time(ms)");
		for (const auto& [hash, item] : s_All) {
			auto us = std::chrono::duration_cast<std::chrono::microseconds>(item->alltime).count();
			buffer += std::format(print_format, item->section, item->line, item->column, item->called, us);
		}
		auto now = std::chrono::system_clock::now();
		std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
		std::tm tm_buf;
#ifdef _WIN32
		localtime_s(&tm_buf, &now_time_t);
#else
		localtime_r(&now_time_t, &tm_buf);
#endif
		std::ostringstream oss;
		oss << std::put_time(&tm_buf, "%Y%m%d_%H%M%S");
		auto duration = now.time_since_epoch();
		auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() % 1000000000;
		oss << "spark-" << std::setfill('0') << std::setw(9) << nanoseconds << ".txt";
		std::filesystem::path current_dir = std::filesystem::current_path();
		std::filesystem::path full_path = current_dir / oss.str();
		std::ofstream file(full_path);
		if (file.is_open()) {
			file << buffer;
#ifdef  close
#undef close
#endif //  close
			file.close();
			g_engfuncs.pfnServerPrint(std::format("Saved into {}", oss.str()).c_str());
		}
		else {
			g_engfuncs.pfnServerPrint(std::format("Saving failed: {}", oss.str()).c_str());
		}
		});
	g_engfuncs.pfnAddServerCommand(const_cast<char*>("spark_clear"), [](){
		s_All.clear();
	});
	SET_META_RESULT(MRES_HANDLED);
}

static DLL_FUNCTIONS gFunctionTable_Post = {
	GameInitPost,					// pfnGameInit
	NULL,					// pfnSpawn
	NULL,					// pfnThink
	NULL,					// pfnUse
	NULL,				// pfnTouch
	NULL,				// pfnBlocked
	NULL,					// pfnKeyValue
	NULL,					// pfnSave
	NULL,					// pfnRestore
	NULL,					// pfnSetAbsBox

	NULL,					// pfnSaveWriteFields
	NULL,					// pfnSaveReadFields

	NULL,					// pfnSaveGlobalState
	NULL,					// pfnRestoreGlobalState
	NULL,					// pfnResetGlobalState

	NULL,					// pfnClientConnect
	NULL,					// pfnClientDisconnect
	NULL,					// pfnClientKill
	NULL,					// pfnClientPutInServer
	NULL,					// pfnClientCommand
	NULL,					// pfnClientUserInfoChanged
	NULL,					// pfnServerActivate
	NULL,					// pfnServerDeactivate

	NULL,					// pfnPlayerPreThink
	NULL,					// pfnPlayerPostThink

	NULL,					// pfnStartFrame
	NULL,					// pfnParmsNewLevel
	NULL,					// pfnParmsChangeLevel

	NULL,					// pfnGetGameDescription
	NULL,					// pfnPlayerCustomization

	NULL,					// pfnSpectatorConnect
	NULL,					// pfnSpectatorDisconnect
	NULL,					// pfnSpectatorThink

	NULL,					// pfnSys_Error

	NULL,					// pfnPM_Move
	NULL,					// pfnPM_Init
	NULL,					// pfnPM_FindTextureType

	NULL,					// pfnSetupVisibility
	NULL,					// pfnUpdateClientData
	NULL,					// pfnAddToFullPack
	NULL,					// pfnCreateBaseline
	NULL,					// pfnRegisterEncoders
	NULL,					// pfnGetWeaponData
	NULL,					// pfnCmdStart
	NULL,					// pfnCmdEnd
	NULL,					// pfnConnectionlessPacket
	NULL,					// pfnGetHullBounds
	NULL,					// pfnCreateInstancedBaselines
	NULL,					// pfnInconsistentFile
	NULL,					// pfnAllowLagCompensation
};
C_DLLEXPORT int GetEntityAPI2_Post(DLL_FUNCTIONS* pFunctionTable,
	int* interfaceVersion) {
	if(!pFunctionTable) {
		UTIL_LogPrintf("GetEntityAPI2 called with null pFunctionTable");
		return false;
	}
	else if (*interfaceVersion != INTERFACE_VERSION) {
		UTIL_LogPrintf("GetEntityAPI2 version mismatch; requested=%d ours=%d", *interfaceVersion, INTERFACE_VERSION);
		//! Tell metamod what version we had, so it can figure out who is out of date.
		*interfaceVersion = INTERFACE_VERSION;
		return false;
	}
	memcpy(pFunctionTable, &gFunctionTable_Post, sizeof(DLL_FUNCTIONS));
	return true;
}