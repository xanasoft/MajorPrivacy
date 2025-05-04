#pragma once

// Windows Header Files
//#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
//#include <windows.h>
#include <phnt_windows.h>
#include <phnt.h>

#define PAGE_SIZE 0x1000



// std includes
#include <string>
#include <sstream>
#include <deque>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <algorithm>
#include <unordered_map>
#include <functional>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <mutex>
#include <shared_mutex>

#include "Common/stl_helpers.h"


// machine types
#include "..\Framework\Core\Types.h"



// usefull defines
#define _T(x)      L ## x

#define STR2(X) #X
#define STR(X) STR2(X)

// Preprocessor macro to convert to wchar_t*
#define WIDEN2(x) L ## x
#define WIDEN(x) WIDEN2(x)


#ifndef ARRAYSIZE
#define ARRAYSIZE(x)		(sizeof(x)/sizeof(x[0]))
#endif

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif


#ifdef _DEBUG
#define ASSERT(x)	if(!(x)) __debugbreak();
#else
#define ASSERT(x)   ;
#endif
