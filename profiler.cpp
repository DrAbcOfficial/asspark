#include "profiler.hpp"

asSparkContextItem::asSparkContextItem(asIScriptContext* context) {
	const char* s{};
	line = context->GetLineNumber(1, &column, &s);
	section = s ? s : "";
}

asSparkMap s_All{};
