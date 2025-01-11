#pragma once

#include "Types.h"

#ifndef STATUS_SUCCESS
#include <ntstatus.h>
#endif

#include "lib_global.h"

class LIBRARY_EXPORT CStatus
{
public:
	CStatus()
	{
		m = NULL;
	}
	CStatus(sint32 Status) : CStatus()
	{
		if (Status != 0)
			Attach(MkError(Status));
	}
	CStatus(sint32 Status, const std::wstring& Message) : CStatus(Status)
	{
		Attach(MkError(Status));
		m->Message = Message;
	}
	CStatus(const CStatus& other) : CStatus() 
	{
		Attach(&other);
	}
	virtual ~CStatus()
	{
		Detach();
	}

	CStatus& operator=(const CStatus& Status) 
	{
		Attach(&Status); 
		return *this; 
	}

	__inline bool IsError() const		{ return m != NULL; }
	__inline bool IsSuccess() const		{ return m == NULL; }
	__inline sint32 GetStatus() const	{ return m ? m->Status : 0; }
	__inline const wchar_t* GetMessageText() const	{ return m ? m->Message.c_str() : NULL; }

	void SetMessageText(const std::wstring& Message)
	{
		if (m != NULL)
			m->Message = Message;
	}

	operator bool() const				{return !IsError();}

protected:
	void Attach(const CStatus* p)
	{
		Attach(p->m);
	}

private:
	struct SError
	{
		sint32 Status;
		std::wstring Message;

		mutable std::atomic<int> aRefCnt;
	} *m;

	SError* MkError(long Status)
	{
		SError* p = new SError();
		p->Status = Status;
		return p;
	}

	void Attach(SError* p)
	{
		Detach();

		if (p != NULL)
		{
			p->aRefCnt.fetch_add(1);
			m = p;
		}
	}

	void Detach()
	{
		if (m != NULL)
		{
			if (m->aRefCnt.fetch_sub(1) == 1)
				delete m;
			m = NULL;
		}
	}
};

typedef CStatus STATUS;
#define OK CStatus()
#define ERR CStatus

template <class T>
class CResult : public CStatus
{
public:
	CResult(const T& value = T()) : CStatus()
	{
		v = value;
	}
	CResult(sint32 Status, const T& value = T()) : CStatus(Status)
	{
		v = value;
	}
	CResult(const CStatus& other) : CResult()
	{
		Attach(&other);
	}
	CResult(const CResult& other) : CStatus(other)
	{
		v = other.v;
	}

	CResult& operator=(const CResult& Status) 
	{
		Attach(&Status); 
		v = Status.v;
		return *this; 
	}

	__inline T& GetValue() { return v; }

	//__inline T* operator->() const {return &v;}

private:
	T v;
};

#define RESULT(x) CResult<x>
#define RETURN(x) return CResult(x) // requires C++17