#pragma once
#include "../Library/Common/Variant.h"
#include "Programs/ProgramID.h"
#include "../Library/Common/FlexGuid.h"

class CGenericRule
{
public:
	CGenericRule(const CProgramID& ID);
	~CGenericRule() {}

	bool IsEnabled() const					{ std::shared_lock Lock(m_Mutex); return m_bEnabled; }
	void SetEnabled(bool bEnabled)			{ std::unique_lock Lock(m_Mutex); m_bEnabled = bEnabled; }

	bool IsTemporary() const				{ std::shared_lock Lock(m_Mutex); return m_bTemporary; }
	void SetTemporary(bool bTemporary)		{ std::unique_lock Lock(m_Mutex); m_bTemporary = bTemporary; }

	bool HasTimeOut() const					{ std::shared_lock Lock(m_Mutex); return m_uTimeOut != -1; }
	bool IsExpired() const;

	CFlexGuid GetGuid() const				{ std::shared_lock Lock(m_Mutex); return m_Guid; }
	
	void SetName(const std::wstring& Name)	{ std::unique_lock Lock(m_Mutex); m_Name = Name; }
	std::wstring GetName() const			{ std::shared_lock Lock(m_Mutex); return m_Name; }
	//std::wstring GetGrouping() const		{ std::shared_lock Lock(m_Mutex); return m_Grouping; }
	std::wstring GetDescription() const		{ std::shared_lock Lock(m_Mutex); return m_Description; }

	const CProgramID& GetProgramID()		{ std::shared_lock Lock(m_Mutex); return m_ProgramID; } // the prog id is unmutable

	void SetData(const CVariant& Data)		{ std::unique_lock Lock(m_Mutex); m_Data = Data; }
	CVariant GetData() const				{ std::shared_lock Lock(m_Mutex); return m_Data; }
	void SetData(const char* Name, const CVariant& Value)	{ std::unique_lock Lock(m_Mutex); m_Data[Name] = Value; }
	CVariant GetData(const char* Name) const				{ std::shared_lock Lock(m_Mutex); return m_Data[Name]; }
	void SetData(uint32 Index, const CVariant& Value)	{ std::unique_lock Lock(m_Mutex); m_Data[Index] = Value; }
	CVariant GetData(uint32 Index) const				{ std::shared_lock Lock(m_Mutex); return m_Data[Index]; }


	void Update(const std::shared_ptr<CGenericRule>& Rule);

	virtual CVariant ToVariant(const SVarWriteOpt& Opts) const;
	virtual NTSTATUS FromVariant(const CVariant& Rule);

protected:
	void CopyTo(CGenericRule* Rule, bool CloneGuid = false) const;

	virtual void WriteIVariant(CVariant& Rule, const SVarWriteOpt& Opts) const;
	virtual void WriteMVariant(CVariant& Rule, const SVarWriteOpt& Opts) const;
	virtual void ReadIValue(uint32 Index, const CVariant& Data);
	virtual void ReadMValue(const SVarName& Name, const CVariant& Data);

	mutable std::shared_mutex  m_Mutex;

	CFlexGuid m_Guid;
	bool m_bEnabled = true;

	CFlexGuid m_Enclave;

	bool m_bTemporary = true;
	uint64 m_uTimeOut = -1;

	std::wstring m_Name;
	//std::wstring m_Grouping;
	std::wstring m_Description;

	CProgramID m_ProgramID;

	CVariant m_Data;
};

