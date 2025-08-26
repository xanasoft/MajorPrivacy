#pragma once
#include "../Library/Common/StVariant.h"
#include "../Library/API/PrivacyDefs.h"
#include "ImageSignInfo.h"

class CProgramLibrary
{
	TRACK_OBJECT(CProgramLibrary)
public:
	CProgramLibrary(const std::wstring& Path = L"");

	uint64 GetUID() const { return m_UID; }
	std::wstring GetPath() const { std::unique_lock lock(m_Mutex); return m_Path; }

	virtual void UpdateSignInfo(const struct SVerifierInfo* pVerifyInfo);
	virtual void UpdateSignInfo(const CImageSignInfo& Info);
	virtual CImageSignInfo GetSignInfo() const { std::unique_lock lock(m_Mutex); return m_SignInfo; }

	virtual StVariant ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr) const;
	virtual NTSTATUS FromVariant(const StVariant& Data);

protected:

	virtual void WriteIVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const;
	virtual void WriteMVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const;
	virtual void ReadIValue(uint32 Index, const StVariant& Data);
	virtual void ReadMValue(const SVarName& Name, const StVariant& Data);

	mutable std::recursive_mutex	m_Mutex;

	uint64							m_UID;
	std::wstring 					m_Path;

	CImageSignInfo					m_SignInfo;

};

typedef std::shared_ptr<CProgramLibrary> CProgramLibraryPtr;
