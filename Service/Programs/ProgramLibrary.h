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

	//
	// Counts up on the one thing about a library that can change after it is
	// enlisted, its signature. The path and the uid are fixed, so anyone holding a
	// copy can tell in one comparison whether it is worth building again.
	//
	uint64 GetVersion() const { std::unique_lock lock(m_Mutex); return m_Version; }
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
	uint64							m_Version = 0;

};

typedef std::shared_ptr<CProgramLibrary> CProgramLibraryPtr;
