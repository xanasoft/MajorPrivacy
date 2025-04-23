#pragma once
#include "../Library/Common/StVariant.h"
#include "../Library/API/PrivacyDefs.h"

class CProgramLibrary
{
	TRACK_OBJECT(CProgramLibrary)
public:
	CProgramLibrary(const std::wstring& Path = L"");

	uint64 GetUID() const { return m_UID; }
	std::wstring GetPath() const { std::unique_lock lock(m_Mutex); return m_Path; }

	virtual StVariant ToVariant(const SVarWriteOpt& Opts) const;
	virtual NTSTATUS FromVariant(const StVariant& Data);

protected:

	virtual void WriteIVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const;
	virtual void WriteMVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const;
	virtual void ReadIValue(uint32 Index, const StVariant& Data);
	virtual void ReadMValue(const SVarName& Name, const StVariant& Data);

	mutable std::recursive_mutex					m_Mutex;

	uint64											m_UID;
	std::wstring 									m_Path;

};

typedef std::shared_ptr<CProgramLibrary> CProgramLibraryPtr;
