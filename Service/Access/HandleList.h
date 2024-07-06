#pragma once
#include "../Library/Status.h"
#include "AccessRule.h"
#include "Handle.h"


class CHandleList
{
public:
	CHandleList();
	~CHandleList();

	STATUS Init();

	void Update();

	//void EnumAllHandlesAsync();
	STATUS EnumAllHandles();

	std::set<CHandlePtr> FindHandles(const CProgramID& ID);

	std::set<CHandlePtr> GetAllHandles();

protected:

	std::shared_mutex m_Mutex;

	uint32 m_FileObjectTypeIndex = ULONG_MAX;
	uint32 m_KeyObjectTypeIndex = ULONG_MAX;

	//HANDLE m_hThread = NULL;

	std::unordered_multimap<PVOID, CHandlePtr> m_List;
};