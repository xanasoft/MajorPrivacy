/*
* Copyright (c) 2023-2025 David Xanatos, xanasoft.com
* All rights reserved.
*
* This file is part of MajorPrivacy.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
*/

#pragma once

#ifdef KERNEL_MODE

#ifdef __cplusplus
extern "C" {
#endif
#include "../../Driver/Driver.h"
#ifdef __cplusplus
}
#endif


#define KPP_TAG '0++K'

#ifndef ASSERT
#define ASSERT(x) NT_ASSERT(x)
#endif

#define DBG_MSG(x) DbgPrintEx(DPFLTR_DEFAULT_ID, 0xFFFFFFFF, x)

#else
//#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
//#include <windows.h>
//#include <winternl.h>
#include <phnt_windows.h>
#include <phnt.h>


#ifndef ASSERT
#ifdef _DEBUG
#define ASSERT(x)	if(!(x)) __debugbreak();
#else
#define ASSERT(x)
#endif
#endif

#ifndef DBG_MSG
#define DBG_MSG(x) DbgPrint(x)
#endif

#endif


