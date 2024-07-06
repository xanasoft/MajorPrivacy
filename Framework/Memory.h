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

#pragma once

#ifdef __cplusplus

#include "Framework.h"
#include "WeakPtr.h"

FW_NAMESPACE_BEGIN

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
	NTSTATUS InitNew(SharedPtr<T> &Ptr, Types&&... Args)
	{
		T* p = (T*)Alloc(sizeof(T));
		if (!p)
			return 0xC000009AL; // STATUS_INSUFFICIENT_RESOURCES;
		Ptr = SharedPtr<T>(new (p) T(this));
		NTSTATUS s = p->Init(Args...);
		if (!NT_SUCCESS(s))
			Ptr.Clear();
		return s;
	}

	virtual void* Alloc(size_t size) = 0;
	virtual void Free(void* ptr) = 0;
};

class FRAMEWORK_EXPORT DefaultMemPool : public AbstractMemPool
{
public:
	virtual void* Alloc(size_t size);
	virtual void Free(void* ptr);
};

class FRAMEWORK_EXPORT AbstractContainer
{
public:
	AbstractContainer(AbstractMemPool* pMem) { m_pMem = pMem; }
	AbstractContainer(const AbstractContainer& other) { m_pMem = other.m_pMem; }
	virtual ~AbstractContainer() {}
	AbstractMemPool* Allocator() const { return m_pMem; }

protected:
	AbstractMemPool* m_pMem = nullptr;

private:
	AbstractContainer& operator=(const AbstractContainer& other) = delete;
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

void* MemAlloc(size_t size);
void MemFree(void* ptr);

void MemCopy(void* dst, const void* src, size_t size);
void MemMove(void* dst, const void* src, size_t size);
void MemSet(void* dst, int value, size_t size);
__forceinline void MemZero(void* dst, size_t size) { MemSet(dst, 0, size); }

int MemCmp(const void* l, const void* r, size_t size);

}