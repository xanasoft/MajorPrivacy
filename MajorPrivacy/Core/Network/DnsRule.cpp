#include "pch.h"
#include "DnsRule.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NtUtil.h"

CDnsRule::CDnsRule(QObject* parent)
	: QObject(parent)
{
}

CDnsRule* CDnsRule::Clone() const
{
    CDnsRule* pRule = new CDnsRule();
	
	pRule->m_bEnabled = m_bEnabled;

	pRule->m_bTemporary = m_bTemporary;

	pRule->m_HostName = m_HostName;
	pRule->m_Action = m_Action;

	return pRule;
}

QtVariant CDnsRule::ToVariant(const SVarWriteOpt& Opts) const
{
	QtVariantWriter Rule;
	if (Opts.Format == SVarWriteOpt::eIndex) {
		Rule.BeginIndex();
		WriteIVariant(Rule, Opts);
	} else {  
		Rule.BeginMap();
		WriteMVariant(Rule, Opts);
	}
	return Rule.Finish();
}

void CDnsRule::FromVariant(const class QtVariant& Rule)
{
	if (Rule.GetType() == VAR_TYPE_MAP)			QtVariantReader(Rule).ReadRawMap([&](const SVarName& Name, const QtVariant& Data) { ReadMValue(Name, Data); });
	else if (Rule.GetType() == VAR_TYPE_INDEX)  QtVariantReader(Rule).ReadRawIndex([&](uint32 Index, const QtVariant& Data) { ReadIValue(Index, Data); });
	// todo err
}

QString CDnsRule::GetActionStr() const 
{
	switch (m_Action)
	{
	case eBlock: return tr("Block");
	case eAllow: return tr("Allow");
	default: return tr("None");
	}
}

