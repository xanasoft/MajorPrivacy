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

#define FW_NAMESPACE_BEGIN namespace FW {
#define FW_NAMESPACE_END }

#ifdef KERNEL_MODE
#define FRAMEWORK_EXPORT
#else
#ifndef BUILD_STATIC
# ifdef BUILD_DLL
#  define FRAMEWORK_EXPORT __declspec(dllexport)
# else
#  define FRAMEWORK_EXPORT __declspec(dllimport)
# endif
#else
# define FRAMEWORK_EXPORT
#endif
#endif

#include "Types.h"

FW_NAMESPACE_BEGIN

enum class EInsertMode
{
	eNormal = 0, // Insert or Repalce if exists
	eMulti,
	eNoReplace,
	eNoInsert
};

enum class EInsertResult
{
	eOK = 0,
	eNoMemory,
	eNoEntry,
	eKeyExists
};

FW_NAMESPACE_END

#define FWSTATUS sint32

//#define NO_CRT_TEST

#ifdef NO_CRT_TEST
#define KERNEL_MODE

#ifdef KERNEL_MODE
#define NO_CRT
#endif

#undef DBG_MSG
#define DBG_MSG(x)
#endif

#define C_BEGIN extern "C" {
#define C_END }

