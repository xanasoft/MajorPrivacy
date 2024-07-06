

/////////////////////////////////////////////////////////////////////////////
// Index Serializer
//

void CAccessRule::WriteIVariant(XVariant& Rule, const SVarWriteOpt& Opts) const
{
	CGenericRule::WriteIVariant(Rule, Opts);

	Rule.Write(API_V_ACCESS_RULE_ACTION, (uint32)m_Type);
	Rule.Write(API_V_ACCESS_PATH, TO_STR(m_AccessPath));
	Rule.Write(API_V_FILE_PATH, TO_STR(m_ProgramPath));
}

void CAccessRule::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_ACCESS_RULE_ACTION: m_Type = (EAccessRuleType)Data.To<uint32>(); break;
	case API_V_ACCESS_PATH: m_AccessPath = AS_STR(Data); break;
	case API_V_FILE_PATH: m_ProgramPath = AS_STR(Data); break;
	default: CGenericRule::ReadIValue(Index, Data);
	}
}


/////////////////////////////////////////////////////////////////////////////
// Map Serializer
//

void CAccessRule::WriteMVariant(XVariant& Rule, const SVarWriteOpt& Opts) const
{
	CGenericRule::WriteMVariant(Rule, Opts);

	switch (m_Type)
	{
	case EAccessRuleType::eAllow:		Rule.Write(API_S_ACCESS_RULE_ACTION, API_S_ACCESS_RULE_ACTION_ALLOW); break;
	case EAccessRuleType::eAllowRO:		Rule.Write(API_S_ACCESS_RULE_ACTION, API_S_ACCESS_RULE_ACTION_ALLOW_RO); break;
	case EAccessRuleType::eEnum:		Rule.Write(API_S_ACCESS_RULE_ACTION, API_S_ACCESS_RULE_ACTION_ENUM); break;
	case EAccessRuleType::eBlock:		Rule.Write(API_S_ACCESS_RULE_ACTION, API_S_ACCESS_RULE_ACTION_BLOCK); break;
	case EAccessRuleType::eProtect:		Rule.Write(API_S_ACCESS_RULE_ACTION, API_S_ACCESS_RULE_ACTION_PROTECT); break;
	}

	Rule.Write(API_S_ACCESS_PATH, TO_STR(m_AccessPath));
	Rule.Write(API_S_FILE_PATH, TO_STR(m_ProgramPath));
}

void CAccessRule::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_ACCESS_RULE_ACTION))
	{
		ASTR Type = Data;
		if (Type == API_S_ACCESS_RULE_ACTION_ALLOW)
			m_Type = EAccessRuleType::eAllow;
		else if (Type == API_S_ACCESS_RULE_ACTION_ALLOW_RO)
			m_Type = EAccessRuleType::eAllowRO;
		else if (Type == API_S_ACCESS_RULE_ACTION_ENUM)
			m_Type = EAccessRuleType::eEnum;
		else if (Type == API_S_ACCESS_RULE_ACTION_BLOCK)
			m_Type = EAccessRuleType::eBlock;
		else if (Type == API_S_ACCESS_RULE_ACTION_PROTECT)
			m_Type = EAccessRuleType::eProtect;
		//else // todo other
		//	return STATUS_INVALID_PARAMETER;
	}

	else if (VAR_TEST_NAME(Name, API_S_ACCESS_PATH))	m_AccessPath = AS_STR(Data);

	else if (VAR_TEST_NAME(Name, API_S_FILE_PATH))		m_ProgramPath = AS_STR(Data);
	
	else
		CGenericRule::ReadMValue(Name, Data);
}