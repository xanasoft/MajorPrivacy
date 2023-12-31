#pragma once
#include "../lib_global.h"
#include "../Status.h"
#include "../Common/Buffer.h"

LIBRARY_EXPORT STATUS QueryTokenVariable(HANDLE TokenHandle, TOKEN_INFORMATION_CLASS TokenInformationClass, CBuffer& Buffer);