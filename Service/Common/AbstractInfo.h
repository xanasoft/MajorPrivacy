#pragma once
#include "../Library/Helpers/MiscHelpers.h"

class CAbstractInfo
{
public:
	CAbstractInfo() {}
	virtual ~CAbstractInfo() {}

protected:
	mutable std::shared_mutex	m_Mutex;
};


class CAbstractInfoEx : public CAbstractInfo
{
public:
	CAbstractInfoEx();
	virtual ~CAbstractInfoEx();

	static void					SetHighlightTime(uint64 time)		{ m_HighlightTime = time; }
	static uint64				GetHighlightTime()					{ return m_HighlightTime; }
	uint64						GetCreateTimeStamp() const			{ std::shared_lock Lock(m_Mutex); return m_CreateTimeStamp; }
	bool						IsNewlyCreated() const;
	
	static void					SetPersistenceTime(uint64 time)		{ m_PersistenceTime = time; }
	static uint64				GetPersistenceTime()				{ return m_PersistenceTime; }
	void						InitTimeStamp()						{ std::unique_lock Lock(m_Mutex); m_CreateTimeStamp = GetTime() * 1000; }
	void						MarkForRemoval()					{ std::unique_lock Lock(m_Mutex); m_RemoveTimeStamp = GetCurTick(); }
	bool						IsMarkedForRemoval() const			{ std::shared_lock Lock(m_Mutex); return m_RemoveTimeStamp != 0; }
	bool						CanBeRemoved() const;
	void						ClearPersistence();

protected:
	volatile mutable bool		m_NewlyCreated;
	uint64						m_CreateTimeStamp;

	uint64						m_RemoveTimeStamp;

	static volatile uint64		m_PersistenceTime;
	static volatile uint64		m_HighlightTime;
};