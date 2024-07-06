#pragma once

template <class T>
class CScoped
{
public:
	CScoped(T* Val = NULL)			{m_Val = Val;}
	~CScoped()						{delete m_Val;}

	CScoped<T>& operator=(T* Val)	{delete m_Val; m_Val = Val; return *this;}

	inline T* Val() const			{return m_Val;}
	inline T* &Val()				{return m_Val;}
	inline T* Detache()				{T* Val = m_Val; m_Val = NULL; return Val;}
    inline T* operator->() const	{return m_Val;}
    inline T& operator*() const     {return *m_Val;}
    inline operator T*() const		{return m_Val;}

private:
	CScoped(const CScoped&) {} // copying is explicitly forbidden
	CScoped& operator=(const CScoped&) {ASSERT(0); return *this;}

	T*	m_Val;
};

#include "ScopedHandle.h"

class CSectionLock
{
public:
	CSectionLock(CRITICAL_SECTION* CritSect)	{
		m_CritSect = CritSect;
		EnterCriticalSection(m_CritSect);
	}
	~CSectionLock(){
		LeaveCriticalSection(m_CritSect);
	}
private:
	CRITICAL_SECTION* m_CritSect;
};