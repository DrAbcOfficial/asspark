#include <stack>
#include <chrono>
#include <unordered_map>
#include <functional>
#include <memory>
#include <format>

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
	size_t hash() const noexcept {
		size_t h = std::hash<int>{}(line);
		h ^= std::hash<int>{}(column)+0x9e3779b9 + (h << 6) + (h >> 2);
		h ^= std::hash<std::string>{}(section)+0x9e3779b9 + (h << 6) + (h >> 2);
		return h;
	}
};
class asSparkTimePoint : public asSparkContextItem {
public:
	std::chrono::steady_clock::time_point start{};

	std::chrono::steady_clock::duration stop() const {
		auto end = std::chrono::high_resolution_clock::now();
		return end - start;
	}
};
class asSparkItem : public asSparkContextItem {
public:
	asSparkItem() = default;
	asSparkItem(std::unique_ptr<asSparkTimePoint> timepoint){
		line = timepoint->line;
		column = timepoint->column;
		section = timepoint->section;
		alltime = timepoint->stop();
		called++;
	}
	std::chrono::steady_clock::duration alltime{};
	uint64_t called{};
};

std::unordered_map<size_t, std::unique_ptr<asSparkItem>> s_All{};
std::stack<std::unique_ptr<asSparkTimePoint>> s_TimeList{};

static hook_t* hook_requestContext{};
static asIScriptContext* (SC_SERVER_DECL* pfnOldRequestContext)(asIScriptEngine* engine SC_SERVER_DUMMYARG_NOCOMMA) = nullptr;
static asIScriptContext* SC_SERVER_DECL RequestContext(asIScriptEngine* engine SC_SERVER_DUMMYARG_NOCOMMA) {
	auto context = pfnOldRequestContext(engine, SC_SERVER_PASS_DUMMYARG_NOCOMMA);
	
	auto sparkTime = std::make_unique<asSparkTimePoint>(context);
	sparkTime->start = std::chrono::high_resolution_clock::now();
	s_TimeList.push(std::move(sparkTime));

	return context;
}

static hook_t* hook_returnContext{};
static void (SC_SERVER_DECL* pfnOldReturnContext)(asIScriptEngine* engine, SC_SERVER_DUMMYARG asIScriptContext* context) = nullptr;
static void SC_SERVER_DECL ReturnContext(asIScriptEngine* engine, SC_SERVER_DUMMYARG asIScriptContext* context) {
	if (s_TimeList.empty()) {
		pfnOldReturnContext(engine, SC_SERVER_PASS_DUMMYARG context);
		LOG_ERROR(PLID, "ERROR in angelscript return context: Stack is NULL!");
		return;
	}
	std::unique_ptr<asSparkTimePoint> sparkTime = std::move(s_TimeList.top());
	size_t hash = sparkTime->hash();
	auto iter = s_All.find(hash);
	if (iter != s_All.end()) {
		iter->second->alltime += sparkTime->stop();
		iter->second->called++;
	}
	else {
		auto allItem = std::make_unique<asSparkItem>(std::move(sparkTime));
		s_All.emplace(hash, std::move(allItem));
	}
	s_TimeList.pop();
	pfnOldReturnContext(engine, SC_SERVER_PASS_DUMMYARG context);
}

static bool s_hooked = false;
static void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax) {
	if (s_hooked) {
		SET_META_RESULT(MRES_IGNORED);
		return;
	}
	SET_META_RESULT(MRES_HANDLED);
	static auto serverManager = ASEXT_GetServerManager();
	static auto& engine = serverManager->scriptEngine;

	const auto INDEX_REQUEST = 95;
	void* pfnRequest = (*(void***)(engine))[INDEX_REQUEST];
	hook_requestContext = gpMetaUtilFuncs->pfnInlineHook(pfnRequest, (void*)RequestContext, (void**)&pfnOldRequestContext, false);

	const auto INDEX_RETURN = 96;
	void* pfnReturn = (*(void***)(engine))[INDEX_RETURN];
	hook_returnContext = gpMetaUtilFuncs->pfnInlineHook(pfnReturn, (void*)ReturnContext, (void**)&pfnOldReturnContext, false);
	s_hooked = true;
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
	g_engfuncs.pfnAddServerCommand(const_cast<char*>("spark_dump_all"), [](){
		constexpr std::string_view print_format = "|{}|{}|{}|{}|{}|\n";
		g_engfuncs.pfnServerPrint((std::format(print_format, "Section", "Line", "Column", "Called", "Time(ms)")).c_str());
		for (const auto& [hash, item] : s_All) {
			auto us = std::chrono::duration_cast<std::chrono::microseconds>(item->alltime).count();
			g_engfuncs.pfnServerPrint((std::format(print_format, item->section, item->line, item->column, item->called, us)).c_str());
		}
	});
	g_engfuncs.pfnAddServerCommand(const_cast<char*>("spark_dump_timepoints"), [](){
		constexpr std::string_view print_format = "|{}|{}|{}|{}|\n";
		g_engfuncs.pfnServerPrint((std::format(print_format, "Section", "Line", "Column", "Elapsed(ms)")).c_str());
		std::vector<std::unique_ptr<asSparkTimePoint>> temp;
		temp.reserve(s_TimeList.size());
		while (!s_TimeList.empty()) {
			temp.push_back(std::move(s_TimeList.top()));
			s_TimeList.pop();
		}
		for (auto it = temp.rbegin(); it != temp.rend(); ++it) {
			auto us = std::chrono::duration_cast<std::chrono::microseconds>((*it)->stop()).count();
			g_engfuncs.pfnServerPrint((std::format(print_format, (*it)->section, (*it)->line, (*it)->column, us)).c_str());
			s_TimeList.push(std::move(*it));
		}
	});
	g_engfuncs.pfnAddServerCommand(const_cast<char*>("spark_clear_all"), [](){
		s_All.clear();
	});
	g_engfuncs.pfnAddServerCommand(const_cast<char*>("spark_clear_timepoints"), [](){
		while (!s_TimeList.empty()) {
			s_TimeList.pop();
		}
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