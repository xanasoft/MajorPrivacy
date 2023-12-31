#pragma once

template <class T>
class CScoped
{
public:
	CScoped(T* Val = NULL)			{m_Val = Val;}
	~CScoped()						{delete m_Val;}

	CScoped<T>& operator=(const CScoped<T>& Scoped)	{ASSERT(0); return *this;} // copying is explicitly forbidden
	CScoped<T>& operator=(T* Val)	{ASSERT(!m_Val); m_Val = Val; return *this;}

	inline T* Val() const			{return m_Val;}
	inline T* &Val()				{return m_Val;}
	inline T* Detache()				{T* Val = m_Val; m_Val = NULL; return Val;}
    inline T* operator->() const	{return m_Val;}
    inline T& operator*() const     {return *m_Val;}
    inline operator T*() const		{return m_Val;}

private:
	T*	m_Val;
};

template <typename T, typename U>
class CScopedHandle
{
public:
	CScopedHandle(T handle, U closer)
	{
		m_handle = handle;
		m_closer = closer;
	}
	~CScopedHandle()
	{
		if (m_handle)
			m_closer(m_handle);
	}

	void Set(T handle)
	{
		m_handle = handle;
	}

	inline operator T& ()		{ return m_handle; }
	inline T* operator&()		{ ASSERT(!m_handle); return &m_handle;}

	explicit operator bool()	{ return m_handle != NULL; }

private:
	T m_handle;
	U m_closer;
};

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