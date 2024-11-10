/*
* Copyright (c) 2023-2024 David Xanatos, xanasoft.com
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

#include "Header.h"
#include "Framework.h"
#include "Memory.h"

#ifndef NO_CRT
#include <stdlib.h>
#endif

FW_NAMESPACE_BEGIN

void* DefaultMemPool::Alloc(size_t size)
{
	return MemAlloc(size);
}

void DefaultMemPool::Free(void* ptr)
{
	if(ptr) MemFree(ptr);
}

FW_NAMESPACE_END

#ifdef NO_CRT

int __cdecl _purecall() { return 0; }

void* __cdecl operator new(size_t size) 
{
	DBG_MSG("operator new(size_t size)\n");
	return MemAlloc(size);
}

void* __cdecl operator new[](size_t size)
{
	DBG_MSG("operator new[](size_t size)\n");
	return MemAlloc(size);
}

void* __cdecl operator new(size_t size, void* ptr) 
{
	UNREFERENCED_PARAMETER(size);
	//DBG_MSG("operator new(size_t size, void* ptr)\n");
	return ptr;
}

void __cdecl operator delete(void* ptr, size_t size) 
{
	UNREFERENCED_PARAMETER(size);
	DBG_MSG("operator delete(void* ptr, size_t size)\n");
	MemFree(ptr);
}

void __cdecl operator delete[](void* ptr) 
{
	DBG_MSG("operator delete(void* ptr, size_t size)\n");
	MemFree(ptr);
}

#endif

C_BEGIN

void* MemAlloc(size_t size)
{
#ifndef KERNEL_MODE
#ifdef NO_CRT
	return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	return malloc(size);
#endif
#elif (NTDDI_VERSION >= NTDDI_WIN10_VB)
	return ExAllocatePool2(POOL_FLAG_NON_PAGED, size, KPP_TAG);
#else
#pragma warning(suppress: 4996) // suppress deprecation warning
	return ExAllocatePoolWithTag(PagedPool, size, KPP_TAG);
#endif
}

void MemFree(void* ptr)
{
	if (!ptr) return;
#ifndef KERNEL_MODE
#ifdef NO_CRT
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	free(ptr);
#endif
#else
	ExFreePoolWithTag(ptr, KPP_TAG);
#endif
}

void MemCopy(void* dst, const void* src, size_t size)
{
#ifndef KERNEL_MODE
	memcpy(dst, src, size);
#else
	RtlCopyMemory(dst, src, size);
#endif
}

void MemMove(void* dst, const void* src, size_t size)
{
#ifndef KERNEL_MODE
	memmove(dst, src, size);
#else
	RtlMoveMemory(dst, src, size);
#endif
}

void MemSet(void* dst, int value, size_t size)
{
#ifndef KERNEL_MODE
	memset(dst, value, size);
#else
	RtlFillMemory(dst, size, value);
#endif
}

int MemCmp(const void* l, const void* r, size_t size)
{
#ifndef KERNEL_MODE
	return memcmp(l, r, size);
#else
	size_t matching_bytes = RtlCompareMemory(l, r, size);
	if (matching_bytes < size) // If not all bytes match, return -1 or 1 based on the lexicographical comparison of the first non-matching characters
		return ((char*)l)[matching_bytes] < ((char*)r)[matching_bytes] ? -1 : 1;
	return 0; // If all bytes match, return 0
#endif
}

C_END