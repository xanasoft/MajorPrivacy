#pragma once
#include "../Library/Common/Variant.h"
#include "../Library/API/PrivacyDefs.h"

class CProgramLibrary
{
public:
	CProgramLibrary(const std::wstring& Path = L"");

	uint64 GetUID() const { return m_UID; }
	std::wstring GetPath() const { std::unique_lock lock(m_Mutex); return m_Path; }

	virtual CVariant ToVariant(const SVarWriteOpt& Opts) const;
	virtual NTSTATUS FromVariant(const CVariant& Data);

protected:

	virtual void WriteIVariant(CVariant& Rule, const SVarWriteOpt& Opts) const;
	virtual void WriteMVariant(CVariant& Rule, const SVarWriteOpt& Opts) const;
	virtual void ReadIValue(uint32 Index, const CVariant& Data);
	virtual void ReadMValue(const SVarName& Name, const CVariant& Data);

	mutable std::recursive_mutex					m_Mutex;

	uint64											m_UID;
	std::wstring 									m_Path;

};

typedef std::shared_ptr<CProgramLibrary> CProgramLibraryPtr;
