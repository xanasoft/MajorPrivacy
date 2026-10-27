#include "pch.h"
#include "AbstractInfo.h"

#include <atomic>

volatile uint64 CAbstractInfoEx::m_PersistenceTime = 5000;
volatile uint64 CAbstractInfoEx::m_HighlightTime = 2500;

//
// Monotonic and never reused, which is what makes a recycled allocation harmless:
// the allocator may hand the block back, but this will not hand the number back
// with it. See CAbstractInfoEx::GetObjectUid.
//
static std::atomic<uint64> g_NextObjectUid = 0;

uint64 MakeObjectUid()
{
	return ++g_NextObjectUid;
}

CAbstractInfoEx::CAbstractInfoEx()
{
	m_Uid = MakeObjectUid();
	m_NewlyCreated = true;
	m_CreateTimeStamp = 0;
	m_RemoveTimeStamp = 0;
}

CAbstractInfoEx::~CAbstractInfoEx()
{
}

bool CAbstractInfoEx::CanBeRemoved() const
{ 
	std::shared_lock Lock(m_Mutex);
	if (m_RemoveTimeStamp == 0)
		return false;
	return GetCurTick() - m_RemoveTimeStamp > m_PersistenceTime;
}

void CAbstractInfoEx::ClearPersistence()
{
	std::shared_lock Lock(m_Mutex);
	if (m_RemoveTimeStamp != 0)
		m_RemoveTimeStamp = GetCurTick() - m_PersistenceTime;
}

bool CAbstractInfoEx::IsNewlyCreated() const
{
	std::unique_lock Lock(m_Mutex);
	if (m_NewlyCreated)
	{
		// Note: GetTime() is very slow, and there is no point to check it over and over agian once we know that this object is old
		uint64 curTime = GetCurTime() * 1000ULL;
		if (!(curTime - m_CreateTimeStamp < m_HighlightTime))
			m_NewlyCreated = false;
	}
	return m_NewlyCreated;
}
