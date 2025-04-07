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

#ifdef __cplusplus

#include "Framework.h"
#include "WeakPtr.h"

FW_NAMESPACE_BEGIN

class FRAMEWORK_EXPORT AbstractContainer
{
	friend class AbstractMemPool;
public:
	AbstractContainer(AbstractMemPool* pMem) { m_pMem = pMem; }
	AbstractContainer(const AbstractContainer& other) { m_pMem = other.m_pMem; }
	AbstractMemPool* Allocator() const { return m_pMem; }

	void* MemAlloc(size_t size) const;
	void MemFree(void* ptr) const;

protected:
	AbstractMemPool* m_pMem = nullptr;

private:
	AbstractContainer& operator=(const AbstractContainer& other) = delete;
};

template <typename V>
struct DefaultValue {
	V operator()(AbstractMemPool* m_pMem) const {
		return V();
	}
};

template <typename V>
struct DefaultContainer{
	V operator()(AbstractMemPool* m_pMem) const {
		return V(m_pMem);
	}
};

class FRAMEWORK_EXPORT AbstractMemPool
{
public:

	template <typename T, class... Types>
	SharedPtr<T> New(Types&&... Args)
	{
		T* p = (T*)Alloc(sizeof(T));
		if (p)
			new (p) T(this, Args...);
		return SharedPtr<T>(p);
	}

	template <typename T, class... Types>
	FWSTATUS InitNew(SharedPtr<T> &Ptr, Types&&... Args)
	{
		T* p = (T*)Alloc(sizeof(T));
		if (!p)
			return 0xC000009AL; // STATUS_INSUFFICIENT_RESOURCES;
		Ptr = SharedPtr<T>(new (p) T(this));
		FWSTATUS s = p->Init(Args...);
		if (!NT_SUCCESS(s))
			Ptr.Clear();
		return s;
	}

	virtual void* Alloc(size_t size, uint32 flags = 0) = 0;
	virtual void Free(void* ptr, uint32 flags = 0) = 0;

	void Adopt(AbstractContainer** ppContainers = NULL)
	{
		for (AbstractContainer** pp = ppContainers; *pp; pp++)
			(*pp)->m_pMem = this;
	}
};

class FRAMEWORK_EXPORT DefaultMemPool : public AbstractMemPool
{
public:
	virtual void* Alloc(size_t size, uint32 flags = 0);
	virtual void Free(void* ptr, uint32 flags = 0);
};

class FRAMEWORK_EXPORT StackedMem : public AbstractMemPool
{
public:
	StackedMem(void* pMem, size_t uSize) { m_pMem = (uint8*)pMem; m_uSize = uSize; m_pPtr = m_pMem;}

	virtual void* Alloc(size_t size, uint32 flags = 0);
	virtual void Free(void* ptr, uint32 flags = 0);

protected:
	struct SMemHeader {
		size_t uSize;
		bool bFreed;
	};

	uint8* m_pMem;
	size_t m_uSize;
	uint8* m_pPtr;
};

FW_NAMESPACE_END

#ifdef NO_CRT

void* __cdecl operator new(size_t size);
void* __cdecl operator new[](size_t size);
void* __cdecl operator new(size_t size, void* ptr);
void __cdecl operator delete(void* ptr, size_t size);
void __cdecl operator delete[](void* ptr);

#endif

#endif

extern "C" {

FRAMEWORK_EXPORT void* MemAlloc(size_t size);
FRAMEWORK_EXPORT void MemFree(void* ptr);

FRAMEWORK_EXPORT void MemCopy(void* dst, const void* src, size_t size);
FRAMEWORK_EXPORT void MemMove(void* dst, const void* src, size_t size);
FRAMEWORK_EXPORT void MemSet(void* dst, int value, size_t size);
FRAMEWORK_EXPORT __forceinline void MemZero(void* dst, size_t size) { MemSet(dst, 0, size); }

FRAMEWORK_EXPORT int MemCmp(const void* l, const void* r, size_t size);

}