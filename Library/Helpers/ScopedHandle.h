#pragma once

template <typename T, typename U>
class CScopedHandle
{
public:
	CScopedHandle(T handle = NULL, U closer = NULL)
	{
		m_handle = handle;
		m_closer = closer;
	}
	~CScopedHandle()
	{
		if (m_handle)
			m_closer(m_handle);
	}

	void Set(T handle, U closer = NULL)
	{
		if (m_handle)
			m_closer(m_handle);
		m_handle = handle;
		if(closer) m_closer = closer;
	}

	inline operator T& ()		{ return m_handle; }
	inline T* operator&()		{ ASSERT(!m_handle); return &m_handle;}

	explicit operator bool()	{ return m_handle != NULL; }

private:
	CScopedHandle(const CScopedHandle&) {} // copying is explicitly forbidden
	CScopedHandle& operator=(const CScopedHandle&) {ASSERT(0); return *this;}

	T m_handle;
	U m_closer;
};

template <typename T, typename U, typename P>
class CScopedHandleEx
{
public:
	CScopedHandleEx(T handle = NULL, U closer = NULL, P param = NULL)
	{
		m_handle = handle;
		m_closer = closer;
		m_param = param;
	}
	~CScopedHandleEx()
	{
		if (m_handle)
			m_closer(m_handle, m_param);
	}

	void Set(T handle, U closer = NULL, P param = NULL)
	{
		if (m_handle)
			m_closer(m_handle, m_param);
		m_handle = handle;
		if(closer) m_closer = closer;
		if(param) m_param = param;
	}

	inline operator T& ()		{ return m_handle; }
	inline T* operator&()		{ ASSERT(!m_handle); return &m_handle;}

	explicit operator bool()	{ return m_handle != NULL; }

private:
	CScopedHandleEx(const CScopedHandleEx&) {} // copying is explicitly forbidden
	CScopedHandleEx& operator=(const CScopedHandleEx&) {ASSERT(0); return *this;}

	T m_handle;
	U m_closer;
	P m_param;
};