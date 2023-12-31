#pragma once

#include "ProgramID.h"
#include "../Processes/Process.h"

class CProgramItem
{
public:
	CProgramItem();

	virtual uint64 GetUID() const							{ return m_UID; }

	virtual const CProgramID& GetID() const					{ return m_ID; }

	virtual void SetName(const std::wstring& Name)			{ std::unique_lock lock(m_Mutex); m_Name = Name; }
	virtual std::wstring GetName() const					{ std::shared_lock lock(m_Mutex); return m_Name; }
	//std::vector<std::wstring> GetCategories() const		{ std::shared_lock lock(m_Mutex); return m_Categories; }
	virtual void SetIcon(const std::wstring& Icon)			{ std::unique_lock lock(m_Mutex); m_Icon = Icon; }
	virtual std::wstring GetIcon() const					{ std::shared_lock lock(m_Mutex); return m_Icon; }
	virtual void SetInfo(const std::wstring& Info)			{ std::unique_lock lock(m_Mutex); m_Info = Info; }
	virtual std::wstring GetInfo() const					{ std::shared_lock lock(m_Mutex); return m_Info; }

	virtual std::map<void*, std::weak_ptr<class CProgramSet>> GetGroups() const { 
		std::shared_lock lock(m_Mutex); 
		return m_Groups; 
	}

	virtual std::set<std::shared_ptr<class CFirewallRule>> GetFwRules() const { 
		std::shared_lock lock(m_Mutex); 
		return m_FwRules; 
	}

	virtual CVariant ToVariant() const;

protected:
	friend class CProgramManager;

	virtual void WriteVariant(CVariant& Data) const;

	mutable std::shared_mutex						m_Mutex;

	uint64											m_UID;
	CProgramID										m_ID;
	std::wstring									m_Name;
	//std::vector<std::wstring>						m_Categories;
	std::wstring									m_Icon;
	std::wstring									m_Info;

	std::map<void*, std::weak_ptr<class CProgramSet>> m_Groups;

	std::set<std::shared_ptr<class CFirewallRule>>	m_FwRules;
	
};

typedef std::shared_ptr<CProgramItem> CProgramItemPtr;
typedef std::weak_ptr<CProgramItem> CProgramItemRef;