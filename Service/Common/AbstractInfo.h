#pragma once
#include "../Library/Helpers/MiscHelpers.h"

//
// A number from the counter CAbstractInfoEx uses, for the things that need an
// identity of their own without being one - see CAccessTree::SPathNode.
//
// One counter for the whole service rather than one per kind, so that nothing has
// to reason about two numbering spaces meeting on the wire.
//
uint64 MakeObjectUid();

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
	//
	// A number of our own, taken from a global counter in the constructor, so it
	// belongs to the object rather than to whatever the system happens to be
	// calling it, and lasts exactly as long as the object does. No lock is needed,
	// it is set before anyone else can see the object and never changes after.
	//
	// This is what a handle and a socket travel under. It used to be the address of
	// the object itself, which is wrong twice over: the allocator hands back
	// recently freed blocks in preference to fresh ones, so a new object very
	// commonly lands on a dead one and anything remembering the dead one by number
	// silently inherits it - and a service running as SYSTEM that publishes the
	// addresses of its own live objects tells every client where its heap is.
	//
	uint64						GetObjectUid() const				{ return m_Uid; }

	uint64						GetCreateTimeStamp() const			{ std::shared_lock Lock(m_Mutex); return m_CreateTimeStamp; }
	bool						IsNewlyCreated() const;
	
	static void					SetPersistenceTime(uint64 time)		{ m_PersistenceTime = time; }
	static uint64				GetPersistenceTime()				{ return m_PersistenceTime; }
	void						InitTimeStamp()						{ std::unique_lock Lock(m_Mutex); m_CreateTimeStamp = GetCurTime() * 1000; }
	void						MarkForRemoval()					{ std::unique_lock Lock(m_Mutex); m_RemoveTimeStamp = GetCurTick(); }
	bool						IsMarkedForRemoval() const			{ std::shared_lock Lock(m_Mutex); return m_RemoveTimeStamp != 0; }
	bool						CanBeRemoved() const;
	void						ClearPersistence();

protected:
	volatile mutable bool		m_NewlyCreated;
	uint64						m_CreateTimeStamp;

	uint64						m_RemoveTimeStamp;

	uint64						m_Uid;

	static volatile uint64		m_PersistenceTime;
	static volatile uint64		m_HighlightTime;
};