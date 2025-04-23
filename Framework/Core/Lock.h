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

class FRAMEWORK_EXPORT Lock
{
public:
	Lock();
	~Lock();

protected:
	friend class Locker;

#ifdef KERNEL_MODE
	ERESOURCE m_Resource;
#else
	CRITICAL_SECTION m_CriticalSection;
#endif
};

class FRAMEWORK_EXPORT Locker
{
public:
	Locker(FW::Lock& lock, bool bLock = true);
	~Locker();

	void Lock();
	void Unlock();

protected:
	FW::Lock* m_pLock;
	bool m_bLocked;
#ifdef KERNEL_MODE
	KIRQL m_irql;
#endif
};

FW_NAMESPACE_END