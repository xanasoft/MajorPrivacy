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
#include "Lock.h"

FW_NAMESPACE_BEGIN

Lock::Lock()
{
#ifdef KERNEL_MODE
	ExInitializeResourceLite(&m_Resource);
#else
	InitializeCriticalSection(&m_CriticalSection);
#endif
}

Lock::~Lock()
{
#ifdef KERNEL_MODE
	ExDeleteResourceLite(&m_Resource);
#else
	DeleteCriticalSection(&m_CriticalSection);
#endif
}

Locker::Locker(FW::Lock& lock, bool bLock)
	: m_pLock(&lock)
	, m_bLocked(false)
{
	if (bLock)
		Lock();
}

Locker::~Locker()
{
	Unlock();
}

void Locker::Lock()
{
	if(m_bLocked)
		return;
#ifdef KERNEL_MODE
	KeRaiseIrql(APC_LEVEL, &m_irql);
	ExAcquireResourceExclusiveLite(&m_pLock->m_Resource, TRUE);
#else
	EnterCriticalSection(&m_pLock->m_CriticalSection);
#endif
	m_bLocked = true;
}

void Locker::Unlock()
{
	if (!m_bLocked)
		return;
#ifdef KERNEL_MODE
	ExReleaseResourceLite(&m_pLock->m_Resource);
	KeLowerIrql(m_irql);
#else
	LeaveCriticalSection(&m_pLock->m_CriticalSection);
#endif
	m_bLocked = false;
}

FW_NAMESPACE_END
