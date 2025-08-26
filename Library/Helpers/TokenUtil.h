#pragma once
#include "../lib_global.h"
#include "../Status.h"
#include "../Framework/Common/Buffer.h"

LIBRARY_EXPORT STATUS QueryTokenVariable(HANDLE TokenHandle, TOKEN_INFORMATION_CLASS TokenInformationClass, CBuffer& Buffer);

LIBRARY_EXPORT bool TokenIsAdmin(HANDLE hToken, bool OnlyFull);