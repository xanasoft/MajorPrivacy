#pragma once
#include "../Library/Common/StVariant.h"
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

	void SetStrGuid(const std::wstring& Guid) { std::unique_lock Lock(m_Mutex); m_Guid = CFlexGuid(Guid); }
	CFlexGuid GetGuid() const				{ std::shared_lock Lock(m_Mutex); return m_Guid; }
	
	void SetName(const std::wstring& Name)	{ std::unique_lock Lock(m_Mutex); m_Name = Name; }
	std::wstring GetName() const			{ std::shared_lock Lock(m_Mutex); return m_Name; }
	//std::wstring GetGrouping() const		{ std::shared_lock Lock(m_Mutex); return m_Grouping; }
	std::wstring GetDescription() const		{ std::shared_lock Lock(m_Mutex); return m_Description; }

	const CProgramID& GetProgramID()		{ std::shared_lock Lock(m_Mutex); return m_ProgramID; } // the prog id is unmutable

	void SetData(const StVariant& Data)		{ std::unique_lock Lock(m_Mutex); m_Data = Data; }
	StVariant GetData() const				{ std::shared_lock Lock(m_Mutex); return m_Data; }
	void SetData(const char* Name, const StVariant& Value)	{ std::unique_lock Lock(m_Mutex); m_Data[Name] = Value; }
	StVariant GetData(const char* Name) const				{ std::shared_lock Lock(m_Mutex); return m_Data.Get(Name); }
	//void SetData(uint32 Index, const StVariant& Value)	{ std::unique_lock Lock(m_Mutex); m_Data[Index] = Value; }
	//StVariant GetData(uint32 Index) const				{ std::shared_lock Lock(m_Mutex); return m_Data.Get(Index); }


	void Update(const std::shared_ptr<CGenericRule>& Rule);

	virtual StVariant ToVariant(const SVarWriteOpt& Opts) const;
	virtual NTSTATUS FromVariant(const StVariant& Rule);

protected:
	void CopyTo(CGenericRule* Rule, bool CloneGuid = false) const;

	virtual void WriteIVariant(StVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	virtual void WriteMVariant(StVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	virtual void ReadIValue(uint32 Index, const StVariant& Data);
	virtual void ReadMValue(const SVarName& Name, const StVariant& Data);

	mutable std::shared_mutex  m_Mutex;

	CFlexGuid m_Guid;
	bool m_bEnabled = true;

	CFlexGuid m_Enclave;

	std::wstring m_User;
	StVariant m_UserSid;

	bool m_bTemporary = false;
	uint64 m_uTimeOut = -1;

	std::wstring m_Name;
	//std::wstring m_Grouping;
	std::wstring m_Description;

	CProgramID m_ProgramID;

	StVariant m_Data;
};

