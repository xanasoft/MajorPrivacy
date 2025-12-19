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
#include "SharedPtr.h"

FW_NAMESPACE_BEGIN

template <class T>
class WeakPtr
{
public:
	WeakPtr() {}
	WeakPtr(const SharedPtr<T>& ptr) {
		Set((T*)ptr.Get());
	}
	WeakPtr(const WeakPtr& ptr) {
		Set(ptr.m_ptr);
	}
	~WeakPtr() { 
		Clear();
	}
	WeakPtr& operator=(const SharedPtr<T>& ptr) {
		if(m_ptr != ptr.Get())
			Set((T*)ptr.Get());
		return *this;
	}
	WeakPtr& operator=(const WeakPtr& ptr) {
		if(m_ptr != ptr.m_ptr)
			Set(ptr.m_ptr);
		return *this;
	}

	SharedPtr<T> Acquire() {
		if (!m_ptr || !m_ptr->AddRefSafe()) // this adds a strong ref
			return SharedPtr<T>();
		SharedPtr<T> p(m_ptr); // this adds another strong ref
		m_ptr->RemoveRef(); // remove the extra strong ref
		return p;
	}

	void Clear() {
		if(m_ptr)
			m_ptr->RemoveRef(false);
		m_ptr = nullptr;
	}
	//void reset() { Clear(); } // for STL compatibility

	bool IsEmpty() const		{ return m_ptr == nullptr; }

protected:

	void Set(T* ptr) {
		Clear();
		m_ptr = ptr;
		if(m_ptr)
			m_ptr->AddRef(false);
	}

	T* m_ptr = nullptr;
};

FW_NAMESPACE_END