

/////////////////////////////////////////////////////////////////////////////
// CAccessRule
//

void CAccessRule::WriteIVariant(XVariant& Rule, const SVarWriteOpt& Opts) const
{
	CGenericRule::WriteIVariant(Rule, Opts);

	Rule.Write(API_V_ACCESS_RULE_ACTION, (uint32)m_Type);
	Rule.Write(API_V_ACCESS_PATH, TO_STR(m_AccessPath));
	Rule.Write(API_V_FILE_PATH, TO_STR(m_ProgramPath));
#ifdef SAVE_NT_PATHS
	if(Opts.Flags & SVarWriteOpt::eSaveNtPaths) {
		Rule.Write(API_V_ACCESS_NT_PATH, TO_STR(m_PathPattern.Get()));
		Rule.Write(API_V_FILE_NT_PATH, TO_STR(m_ProgPattern.Get()));
	}
#endif
	Rule.Write(API_V_VOL_RULE, m_bVolumeRule);
}

void CAccessRule::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_ACCESS_RULE_ACTION: m_Type = (EAccessRuleType)Data.To<uint32>(); break;
	case API_V_ACCESS_PATH: m_AccessPath = AS_STR(Data); break;
	case API_V_FILE_PATH: m_ProgramPath = AS_STR(Data); break;
#ifdef LOAD_NT_PATHS
	case API_V_ACCESS_NT_PATH: m_AccessNtPath = AS_STR(Data); break;
	case API_V_FILE_NT_PATH: m_ProgramNtPath = AS_STR(Data); break;
#endif
	case API_V_VOL_RULE: m_bVolumeRule = Data.To<bool>(); break;
	default: CGenericRule::ReadIValue(Index, Data);
	}
}

/////////////////////////////////////////////////////////////////////////////

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
	case EAccessRuleType::eIgnore:		Rule.Write(API_S_ACCESS_RULE_ACTION, API_S_ACCESS_RULE_ACTION_IGNORE); break;
	}

	Rule.Write(API_S_ACCESS_PATH, TO_STR(m_AccessPath));
	Rule.Write(API_S_FILE_PATH, TO_STR(m_ProgramPath));
#ifdef SAVE_NT_PATHS
	if(Opts.Flags & SVarWriteOpt::eSaveNtPaths) {
		Rule.Write(API_S_ACCESS_NT_PATH, TO_STR(m_PathPattern.Get()));
		Rule.Write(API_S_FILE_NT_PATH, TO_STR(m_ProgPattern.Get()));
	}
#endif
	Rule.Write(API_S_VOL_RULE, m_bVolumeRule);
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
		else if (Type == API_S_ACCESS_RULE_ACTION_IGNORE)
			m_Type = EAccessRuleType::eIgnore;
		//else // todo other
		//	return STATUS_INVALID_PARAMETER;
	}

	else if (VAR_TEST_NAME(Name, API_S_ACCESS_PATH))	m_AccessPath = AS_STR(Data);

	else if (VAR_TEST_NAME(Name, API_S_FILE_PATH))		m_ProgramPath = AS_STR(Data);

#ifdef LOAD_NT_PATHS
	else if (VAR_TEST_NAME(Name, API_S_ACCESS_NT_PATH))	m_AccessNtPath = AS_STR(Data);

	else if (VAR_TEST_NAME(Name, API_S_FILE_NT_PATH))		m_ProgramNtPath = AS_STR(Data);
#endif

	else if (VAR_TEST_NAME(Name, API_S_VOL_RULE))		m_bVolumeRule = Data.To<bool>();
	
	else
		CGenericRule::ReadMValue(Name, Data);
}