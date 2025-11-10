#pragma once
#include "DnsServer.h"
#include "../Library/Status.h"
#include "../Library/API/PrivacyDefs.h"
#include "../Framework/Common/PathTree.h"
#include "..\Library\Common\Address.h"
#include "..\Library\Common\StVariant.h"
#include "..\Library\Common\FlexGuid.h"

class CDnsRule : public CPathEntry
{
public:

	enum EAction { eNone, eBlock, eAllow };

	CDnsRule(FW::AbstractMemPool* pMem) : CPathEntry(pMem) {}
	CDnsRule(FW::AbstractMemPool* pMem, const FW::StringW& Path, EAction Action, bool bTemporary = false) : CPathEntry(pMem, Path) {
		m_Action = Action;
		m_bTemporary = bTemporary;
	}

	void SetGuid(const CFlexGuid& Guid)		{ m_Guid = Guid; }
	CFlexGuid GetGuid() const				{ return m_Guid; }

	bool IsEnabled() const					{ return m_bEnabled; }

	bool IsTemporary() const				{ return m_bTemporary; }

	EAction GetAction() const				{ return m_Action; }

	void IncHitCount()						{ m_HitCount++; }

	virtual StVariant ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr) const;
	virtual STATUS FromVariant(const StVariant& Rule);

protected:
	virtual void WriteIVariant(StVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	virtual void WriteMVariant(StVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	virtual void ReadIValue(uint32 Index, const StVariant& Data);
	virtual void ReadMValue(const SVarName& Name, const StVariant& Data);

	CFlexGuid		m_Guid;
	bool			m_bEnabled = true;

	bool 			m_bTemporary = false;

	std::string		m_HostName;
	EAction			m_Action = eNone;

	int				m_HitCount = 0;
};