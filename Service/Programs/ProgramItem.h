#pragma once

#include "ProgramID.h"
#include "../Processes/Process.h"

class CProgramItem
{
public:
	CProgramItem();

	virtual EProgramType GetType() const = 0;

	virtual uint64 GetUID() const							{ return m_UID; }

	virtual const CProgramID& GetID() const					{ return m_ID; }

	virtual void SetName(const std::wstring& Name)			{ std::unique_lock lock(m_Mutex); m_Name = Name; }
	virtual std::wstring GetName() const					{ std::unique_lock lock(m_Mutex); return m_Name; }
	//std::vector<std::wstring> GetCategories() const		{ std::unique_lock lock(m_Mutex); return m_Categories; }
	virtual void SetIcon(const std::wstring& Icon)			{ std::unique_lock lock(m_Mutex); m_IconFile = Icon; }
	virtual std::wstring GetIcon() const					{ std::unique_lock lock(m_Mutex); return m_IconFile; }
	virtual void SetInfo(const std::wstring& Info)			{ std::unique_lock lock(m_Mutex); m_Info = Info; }
	virtual std::wstring GetInfo() const					{ std::unique_lock lock(m_Mutex); return m_Info; }

	virtual std::map<void*, std::weak_ptr<class CProgramSet>> GetGroups() const { std::unique_lock lock(m_Mutex); return m_Groups; }
	virtual size_t GetGroupCount() const					{ std::unique_lock lock(m_Mutex); return m_Groups.size(); }

	virtual std::set<std::shared_ptr<class CFirewallRule>> GetFwRules() const { std::unique_lock lock(m_Mutex); return m_FwRules; }
	virtual bool HasFwRules() const { std::unique_lock lock(m_Mutex); return !m_FwRules.empty(); }
	virtual std::set<std::shared_ptr<class CProgramRule>> GetProgRules() const { std::unique_lock lock(m_Mutex); return m_ProgRules; }
	virtual bool HasProgRules() const { std::unique_lock lock(m_Mutex); return !m_ProgRules.empty(); }
	virtual std::set<std::shared_ptr<class CAccessRule>> GetResRules() const { std::unique_lock lock(m_Mutex); return m_ResRules; }
	virtual bool HasResRules() const { std::unique_lock lock(m_Mutex); return !m_ResRules.empty(); }

	virtual CVariant ToVariant(const SVarWriteOpt& Opts) const;
	virtual NTSTATUS FromVariant(const CVariant& Data);

protected:
	friend class CProgramManager;

	virtual void WriteIVariant(CVariant& Rule, const SVarWriteOpt& Opts) const;
	virtual void WriteMVariant(CVariant& Rule, const SVarWriteOpt& Opts) const;
	virtual void ReadIValue(uint32 Index, const CVariant& Data);
	virtual void ReadMValue(const SVarName& Name, const CVariant& Data);

	mutable std::recursive_mutex					m_Mutex;

	uint64											m_UID;
	CProgramID										m_ID;
	std::wstring									m_Name;
	//std::vector<std::wstring>						m_Categories;
	std::wstring									m_IconFile;
	std::wstring									m_Info;

#ifdef _DEBUG
public:
#endif

	std::map<void*, std::weak_ptr<class CProgramSet>> m_Groups;

	std::set<std::shared_ptr<class CFirewallRule>>	m_FwRules;
	std::set<std::shared_ptr<class CProgramRule>>	m_ProgRules;
	std::set<std::shared_ptr<class CAccessRule>>	m_ResRules;

private:
	CVariant CollectFwRules() const;
	CVariant CollectProgRules() const;
	CVariant CollectResRules() const;
	
};

typedef std::shared_ptr<CProgramItem> CProgramItemPtr;
typedef std::weak_ptr<CProgramItem> CProgramItemRef;