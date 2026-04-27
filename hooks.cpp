#include "profiler.hpp"

static hook_t* hook_executeContext{};
static void (SC_SERVER_DECL* pfnOldExecuteContext)(asIScriptContext* ctx SC_SERVER_DUMMYARG_NOCOMMA) = nullptr;

static void SC_SERVER_DECL ExecuteContext(asIScriptContext* ctx SC_SERVER_DUMMYARG_NOCOMMA) {
	if (CVAR_GET_FLOAT(CVAR_SPARK_ON) <= 0) {
		pfnOldExecuteContext(ctx SC_SERVER_PASS_DUMMYARG2);
		return;
	}

	int column{};
	const char* section{};
	int line = ctx->GetLineNumber(0, &column, &section);
	if (!section) {
		pfnOldExecuteContext(ctx SC_SERVER_PASS_DUMMYARG2);
		return;
	}

	size_t hash = SparkHash(line, column, section);
	auto start = std::chrono::high_resolution_clock::now();
	pfnOldExecuteContext(ctx SC_SERVER_PASS_DUMMYARG2);
	auto end = std::chrono::high_resolution_clock::now();
	auto used_time = end - start;

	auto iter = s_All.find(hash);
	if (iter != s_All.end()) {
		iter->second->alltime += used_time;
		iter->second->called++;
	} else {
		auto item = std::make_unique<asSparkItem>();
		item->line = line;
		item->column = column;
		item->section = section;
		item->called = 1;
		item->alltime = used_time;
		s_All.emplace(hash, std::move(item));
	}
}

void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax) {
	static bool s_hooked = false;
	if (s_hooked) {
		SET_META_RESULT(MRES_IGNORED);
		return;
	}

	auto serverManager = ASEXT_GetServerManager();
	if (!serverManager) {
		LOG_ERROR(PLID, "ERROR in angelscript hook: Server Manager is NULL!");
		SET_META_RESULT(MRES_IGNORED);
		return;
	}
	auto& engine = serverManager->scriptEngine;
	if (!engine) {
		LOG_ERROR(PLID, "ERROR in angelscript hook: Engine is NULL!");
		SET_META_RESULT(MRES_IGNORED);
		return;
	}

	asIScriptContext* ctx = engine->RequestContext();
	void* pfnExecute = (*(void***)(ctx))[EXECUTE_VTABLE_INDEX];
	hook_executeContext = gpMetaUtilFuncs->pfnInlineHook(
		pfnExecute, (void*)ExecuteContext, (void**)&pfnOldExecuteContext, false);
	engine->ReturnContext(ctx);

	s_hooked = true;
	SET_META_RESULT(MRES_HANDLED);
}
