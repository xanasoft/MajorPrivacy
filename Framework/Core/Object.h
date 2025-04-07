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

#include "Framework.h"
#include "Memory.h"

FW_NAMESPACE_BEGIN

class FRAMEWORK_EXPORT Object
{
public:
	Object(AbstractMemPool* pMem) { m_pMem = pMem; }
	Object(const Object& other) { m_pMem = other.m_pMem; }
	virtual ~Object() {}

	void AddRef(bool bStrong = true) {
		if(bStrong) 
			InterlockedIncrement(&m_Refs);
		else 
			InterlockedIncrement(&m_WeakRefs);
	}

	bool AddRefSafe() {
		for (LONG refs = m_Refs; refs != 0; refs = m_Refs) {
			if (InterlockedCompareExchange(&m_Refs, refs + 1, refs) == refs)
				return true;
		}
		return false;
	}

	void RemoveRef(bool bStrong = true) {
		if (bStrong) {
			if (InterlockedDecrement(&m_Refs) == 0) {
				if (m_pDestroyCallback != nullptr) { // in ~Object() it may be to late as most of the object would be already toren down
					InterlockedIncrement(&m_WeakRefs); // to prevent the object from being freed by the callback
					m_pDestroyCallback(this, m_pDestroyParam);
					InterlockedDecrement(&m_WeakRefs);
				}
				this->~Object();
			}
		} else 
			InterlockedDecrement(&m_WeakRefs);
		if (m_Refs == 0 && m_WeakRefs == 0)
			m_pMem->Free(this);
	}

	ULONG RefCount() const { return m_Refs; }
	
	AbstractMemPool* Allocator() const { return m_pMem; }

	template <typename T, class... Types>
	SharedPtr<T> New(Types&&... Args) { return m_pMem->New<T>(Args...); }

	template <typename T, class... Types>
	FWSTATUS InitNew(SharedPtr<T>& Ptr, Types&&... Args) { return m_pMem->InitNew(Ptr, Args...); }

	void* MemAlloc(size_t size) const { return m_pMem->Alloc(size); }
	void MemFree(void* ptr) const { m_pMem->Free(ptr); }

	void SetDestroyCallback(void(*pDestroyCallback)(PVOID This, PVOID Param), PVOID Param) {
		m_pDestroyCallback = pDestroyCallback;
		m_pDestroyParam = Param;
	}

protected:
	volatile LONG m_Refs = 0;
	volatile LONG m_WeakRefs = 0;
	AbstractMemPool* m_pMem = nullptr;

	void(*m_pDestroyCallback)(PVOID This, PVOID Param) = nullptr;
	PVOID m_pDestroyParam = nullptr;

private:
	Object& operator=(const Object& other) = delete;
};

FW_NAMESPACE_END