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

template <typename T, typename U>
class UniquePtr
{
public:
	UniquePtr(T ptr, U closer) : m_ptr(ptr), m_closer(closer) {}
	UniquePtr(const UniquePtr& other) = delete;
	UniquePtr(UniquePtr&& other) : m_ptr(other.m_ptr), m_closer(other.m_closer) { 
		other.m_ptr = NULL; 
	}
	~UniquePtr() {
		if (m_ptr)
			m_closer(m_ptr);
	}

	UniquePtr& operator=(const UniquePtr& other) = delete;
	UniquePtr& operator=(UniquePtr&& other){
		if (m_ptr != other.m_ptr) {
			if (m_ptr)
				m_closer(m_ptr);
			m_ptr = other.m_ptr;
			m_closer = other.m_closer;
			other.m_ptr = NULL;
		}
		return *this;
	}
	UniquePtr& operator=(T ptr) {
		if (m_ptr)
			m_closer(m_ptr);
		m_ptr = ptr;
		return *this;
	}

	operator T& ()				{ return m_ptr; }
	T* operator&()				{ ASSERT(!m_ptr); return &m_ptr;}
	T operator->()				{ return m_ptr; }

	T Detach() {
		T ptr = m_ptr;
		m_ptr = NULL;
		return ptr;
	}

	explicit operator bool()	{ return m_ptr != NULL; }

private:
	T m_ptr;
	U m_closer;
};

FW_NAMESPACE_END