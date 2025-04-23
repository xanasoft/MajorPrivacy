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

#include "Header.h"
#include "Memory.h"

#ifdef NO_CRT_TEST
#undef KERNEL_MODE
#endif

#ifndef NO_CRT
#include <stdlib.h>
#endif

FW_NAMESPACE_BEGIN

//////////////////////////////////////////////////////////////////////////////////////////////
// AbstractContainer

void* AbstractContainer::MemAlloc(size_t size, uint32 flags) const
{
	if (m_pMem && m_pMem != NO_MEM_POOL)
		return m_pMem->Alloc(size, flags);
#ifdef KERNEL_MODE
	DBG_MSG("AbstractContainer::MemAlloc NO POOL !!!\n");
#endif
	return ::MemAlloc(size, flags);
}

void AbstractContainer::MemFree(void* ptr, uint32 flags) const
{
	if (m_pMem && m_pMem != NO_MEM_POOL)
		m_pMem->Free(ptr, flags);
	else {
#ifdef KERNEL_MODE
		DBG_MSG("AbstractContainer::MemAlloc NO POOL !!!\n");
#endif
		::MemFree(ptr, flags);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////
// DefaultMemPool

void* DefaultMemPool::Alloc(size_t size, uint32 flags)
{
	return MemAlloc(size, flags);
}

void DefaultMemPool::Free(void* ptr, uint32 flags)
{
	if(ptr) MemFree(ptr, flags);
}

//////////////////////////////////////////////////////////////////////////////////////////////
// StackedMem

void* StackedMem::Alloc(size_t size, uint32 flags)
{
	size_t uTotalSize = sizeof(SMemHeader) + size;
	if ((m_pPtr - m_pMem) + uTotalSize <= m_uSize)
	{
		SMemHeader* pHeader = ((SMemHeader*)m_pPtr);
		pHeader->uSize = size;
		pHeader->bFreed = false;
		void* pPtr = m_pPtr + sizeof(SMemHeader);
		m_pPtr += uTotalSize;
		return pPtr;
	}
#ifndef KERNEL_MODE
	DebugBreak();
#endif
	return nullptr;
}

void StackedMem::Free(void* ptr, uint32 flags)
{
	SMemHeader* pHeader = ((SMemHeader*)((uint8*)ptr)) - sizeof(SMemHeader);
	pHeader->bFreed = true;

	uint8* blockEnd = ((uint8*)ptr) + pHeader->uSize;
	while (m_pPtr == blockEnd && m_pPtr != m_pMem)
	{
		m_pPtr -= (sizeof(SMemHeader) + pHeader->uSize);
		if (m_pPtr == m_pMem)
			break;
		pHeader = ((SMemHeader*)m_pPtr);
		blockEnd = ((uint8*)m_pPtr) + pHeader->uSize;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////

FW_NAMESPACE_END

//////////////////////////////////////////////////////////////////////////////////////////////

#ifdef NO_CRT

int __cdecl _purecall() { return 0; }

void* __cdecl operator new(size_t size) 
{
	DBG_MSG("operator new(size_t size)\n");
	return MemAlloc(size, 0);
}

void* __cdecl operator new[](size_t size)
{
	DBG_MSG("operator new[](size_t size)\n");
	return MemAlloc(size, 0);
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
	MemFree(ptr, 0);
}

void __cdecl operator delete[](void* ptr) 
{
	DBG_MSG("operator delete(void* ptr, size_t size)\n");
	MemFree(ptr, 0);
}

#endif

C_BEGIN

/*
void* MemAlloc(size_t size, uint32 flags)
{
	return MemAllocTagged(size, flags, 0);
}

void* MemAllocTagged(size_t size, uint32 flags, uint32 tag)
{
	void* ptr = nullptr;
#ifndef KERNEL_MODE
#ifdef NO_CRT
	ptr = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	ptr = malloc(size);
#endif
#elif (NTDDI_VERSION >= NTDDI_WIN10_VB)
	ptr = ExAllocatePool2(POOL_FLAG_NON_PAGED, size, tag);
#else
#pragma warning(suppress: 4996) // suppress deprecation warning
	ptr = ExAllocatePoolWithTag(PagedPool, size, tag);
#endif
	return ptr;
}

void MemFree(void* ptr, uint32 flags)
{
	MemFreeTagged(ptr, flags, 0);
}

void MemFreeTagged(void* ptr, uint32 flags, uint32 tag)
{
	if (!ptr) return;
#ifndef KERNEL_MODE
#ifdef NO_CRT
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	free(ptr);
#endif
#else
	ExFreePoolWithTag(ptr, tag);
#endif
}
*/

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