

/////////////////////////////////////////////////////////////////////////////
// CDnsRule
//

void CDnsRule::WriteIVariant(VariantWriter& Rule, const SVarWriteOpt& Opts) const
{
	if(!m_Guid.IsNull())
		Rule.WriteVariant(API_V_GUID, m_Guid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids));
	Rule.Write(API_V_ENABLED, m_bEnabled);
	Rule.Write(API_V_TEMP, m_bTemporary);

	Rule.WriteEx(API_V_DNS_HOST, TO_STR(m_HostName));
	Rule.Write(API_V_DNS_RULE_ACTION, (uint32)m_Action);

	Rule.Write(API_V_RULE_HIT_COUNT, m_HitCount);
}

void CDnsRule::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_GUID:			m_Guid.FromVariant(Data); break;
	case API_V_ENABLED:			m_bEnabled = Data.To<bool>(); break;
	case API_V_TEMP:			m_bTemporary = Data.To<bool>(); break;

	case API_V_DNS_HOST:		m_HostName = AS_ASTR(Data); break;
	case API_V_DNS_RULE_ACTION:	m_Action = (EAction)Data.To<uint32>(); break;

	case API_V_RULE_HIT_COUNT:	m_HitCount = Data.To<int>(); break;
	//default: ;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CDnsRule::WriteMVariant(VariantWriter& Rule, const SVarWriteOpt& Opts) const
{
	if (!m_Guid.IsNull())
		Rule.WriteVariant(API_S_GUID, m_Guid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids));
	Rule.Write(API_S_ENABLED, m_bEnabled);
	Rule.Write(API_S_TEMP, m_bTemporary);

	Rule.WriteEx(API_S_DNS_HOST, TO_STR(m_HostName));
	switch (m_Action)
	{
	case eBlock:	Rule.Write(API_S_DNS_RULE_ACTION, API_S_DNS_RULE_ACTION_BLOCK); break;
	case eAllow:	Rule.Write(API_S_DNS_RULE_ACTION, API_S_DNS_RULE_ACTION_ALLOW); break;
	}

	Rule.Write(API_S_RULE_HIT_COUNT, m_HitCount);
}

void CDnsRule::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_GUID))			m_Guid.FromVariant(Data);
	else if (VAR_TEST_NAME(Name, API_S_ENABLED))	m_bEnabled = Data.To<bool>();
	else if (VAR_TEST_NAME(Name, API_S_TEMP))		m_bTemporary = Data.To<bool>();

	else if (VAR_TEST_NAME(Name, API_S_DNS_HOST))	m_HostName = AS_ASTR(Data);
	else if (VAR_TEST_NAME(Name, API_S_DNS_RULE_ACTION))
	{
		ASTR Type = Data;
		if (Type == API_S_DNS_RULE_ACTION_ALLOW)
			m_Action = EAction::eAllow;
		else if (Type == API_S_DNS_RULE_ACTION_BLOCK)
			m_Action = EAction::eBlock;
	}

	else if (VAR_TEST_NAME(Name, API_S_RULE_HIT_COUNT))	m_HitCount = Data.To<int>();
}