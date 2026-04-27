#include "profiler.hpp"

#include <format>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>

void GameInitPost() {
	static cvar_t cvar_spark = {
		const_cast<char*>(CVAR_SPARK_ON),
		const_cast<char*>("0"),
		FCVAR_SERVER | FCVAR_EXTDLL | FCVAR_ARCHIVE | FCVAR_PRINTABLEONLY,
		SPARK_ON_DEFAULT,
		nullptr
	};
	CVAR_REGISTER(&cvar_spark);

	REGISTER_SPARK_CMD(CMD_SPARK_DUMP, []() {
		g_engfuncs.pfnServerPrint(std::format(DUMP_FORMAT,
			"Section", "Line", "Column", "Called", "Time(ms)").c_str());
		for (const auto& [hash, item] : s_All) {
			auto us = std::chrono::duration_cast<std::chrono::microseconds>(item->alltime).count();
			g_engfuncs.pfnServerPrint(std::format(DUMP_FORMAT,
				item->section, item->line, item->column, item->called, us).c_str());
		}
	});

	REGISTER_SPARK_CMD(CMD_SPARK_DUMP_FILE, []() {
		std::string buffer = std::format(DUMP_FORMAT,
			"Section", "Line", "Column", "Called", "Time(ms)");
		for (const auto& [hash, item] : s_All) {
			auto us = std::chrono::duration_cast<std::chrono::microseconds>(item->alltime).count();
			buffer += std::format(DUMP_FORMAT,
				item->section, item->line, item->column, item->called, us);
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
		auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
			now.time_since_epoch()).count() % 1000000000;
		oss << "spark-" << std::setfill('0') << std::setw(9) << nanoseconds << ".txt";

		std::filesystem::path full_path = std::filesystem::current_path() / oss.str();
		{
			std::ofstream file(full_path);
			if (!file.is_open()) {
				g_engfuncs.pfnServerPrint(std::format("Saving failed: {}", oss.str()).c_str());
				return;
			}
			file << buffer;
		}
		g_engfuncs.pfnServerPrint(std::format("Saved into {}", oss.str()).c_str());
	});

	REGISTER_SPARK_CMD(CMD_SPARK_CLEAR, []() {
		s_All.clear();
	});

	SET_META_RESULT(MRES_HANDLED);
}
