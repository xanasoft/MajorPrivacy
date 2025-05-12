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

template <typename T>
class SafeRef
{
public:
	SafeRef(T* ptr = nullptr) : m_ptr(ptr) {}
	SafeRef(const SafeRef& ptr) : m_ptr(ptr.m_ptr) {}

	SafeRef& operator=(const SafeRef& ptr) {
		m_ptr = ptr.m_ptr;
		return *this;
	}

	SafeRef& operator=(const T& val) {
		if (m_ptr)
			*m_ptr = val;
		return *this;
	}

	operator T() const { 
		if (m_ptr)
			return *m_ptr;
		return T();
	}

	template <typename U>
	SafeRef& operator=(const U& val) {
		if (m_ptr)
			*m_ptr = val;
		return *this;
	}

	template <typename U>
	operator U() const { 
		if (m_ptr)
			return (U)*m_ptr;
		return U();
	}

	T* operator&() {
		return m_ptr;
	}

	bool IsValid() const {
		return m_ptr != nullptr;
	}

private:
	T* m_ptr = nullptr;
};

FW_NAMESPACE_END