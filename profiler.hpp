#pragma once

#include "asspark.hpp"

#include <chrono>
#include <unordered_map>
#include <memory>
#include <string>
#include <functional>

#ifdef _WIN32
#define SC_SERVER_PASS_DUMMYARG2 ,SC_SERVER_PASS_DUMMYARG_NOCOMMA
#else
#define SC_SERVER_PASS_DUMMYARG2
#endif

class asSparkContextItem {
public:
	asSparkContextItem() = default;
	explicit asSparkContextItem(asIScriptContext* context);
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

using asSparkMap = std::unordered_map<size_t, std::unique_ptr<asSparkItem>>;

extern asSparkMap s_All;

inline size_t SparkHash(int line, int column, const char* section) {
	size_t h = std::hash<int>{}(line);
	h ^= std::hash<int>{}(column) + HASH_SEED + (h << 6) + (h >> 2);
	h ^= std::hash<std::string>{}(section) + HASH_SEED + (h << 6) + (h >> 2);
	return h;
}
