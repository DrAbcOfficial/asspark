#pragma once
#include "signatures_template.h"
#define FILL_AND_HOOK(dll, name) FILL_FROM_SIGNATURE(dll, name);INSTALL_INLINEHOOK(name)
