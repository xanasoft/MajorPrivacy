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

FW_NAMESPACE_BEGIN

template <class T>
class SharedPtr
{
public:
	SharedPtr() {}
	explicit SharedPtr(T* ptr)			{ Set(ptr); }
	SharedPtr(const SharedPtr& ptr)		{ Set(ptr.m_ptr); }
	template <class U>
	SharedPtr(const SharedPtr<U>& ptr)	{ Set((U*)ptr.Get()); }
	~SharedPtr()						{ Clear(); }
	SharedPtr& operator=(const SharedPtr& ptr) {
		if(m_ptr != ptr.m_ptr)
			Set(ptr.m_ptr);
		return *this;
	}

	bool operator==(const SharedPtr& ptr) const		{ return m_ptr == ptr.m_ptr; }
	template <class U>
	bool operator==(const SharedPtr<U>& ptr) const	{ return m_ptr == ptr.m_ptr; }

	T& operator*()				{ return *m_ptr; }
	const T& operator*() const	{ return *m_ptr; }
	T* operator->()				{ return m_ptr; }
	const T* operator->() const { return m_ptr; }
	T* Get()					{ return m_ptr; }
	const T* Get() const		{ return m_ptr; }

	template<class U>
	SharedPtr<U> Cast() {
		if(!m_ptr)
			return SharedPtr<U>();
		// todo add check
		return SharedPtr<U>((U*)m_ptr);
	}

	template<class U>
	const SharedPtr<U> Cast() const {
		// todo add check
		return SharedPtr<U>((U*)m_ptr);
	}

	void Clear() {
		if(m_ptr)
			m_ptr->RemoveRef();
		m_ptr = nullptr;
	}
	void reset() { Clear(); } // for STL compatibility

	explicit operator bool() const		{ return m_ptr != nullptr; }

protected:
	void Set(T* ptr) {
		Clear();
		m_ptr = ptr;
		if(m_ptr)
			m_ptr->AddRef();
	}

	T* m_ptr = nullptr;
};

FW_NAMESPACE_END