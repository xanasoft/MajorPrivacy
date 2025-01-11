

/////////////////////////////////////////////////////////////////////////////
// CProgramRule
//

void CProgramRule::WriteIVariant(XVariant& Rule, const SVarWriteOpt& Opts) const
{
	CGenericRule::WriteIVariant(Rule, Opts);

	Rule.Write(API_V_EXEC_RULE_ACTION, (uint32)m_Type);
	Rule.Write(API_V_FILE_PATH, TO_STR(m_ProgramPath));
#ifdef SAVE_NT_PATHS
	if(Opts.Flags & SVarWriteOpt::eSaveNtPaths) {
		Rule.Write(API_V_FILE_NT_PATH, TO_STR(m_PathPattern.Get()));
	}
#endif
	if (m_Type == EExecRuleType::eAllow || m_Type == EExecRuleType::eProtect)
		Rule.Write(API_V_EXEC_SIGN_REQ, (uint32)m_SignatureLevel);
	Rule.Write(API_V_IMAGE_LOAD_PROTECTION, m_ImageLoadProtection);
}

void CProgramRule::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_EXEC_RULE_ACTION: m_Type = (EExecRuleType)Data.To<uint32>(); break;
	case API_V_FILE_PATH: m_ProgramPath = AS_STR(Data); break;
#ifdef LOAD_NT_PATHS
	case API_V_FILE_NT_PATH: m_ProgramNtPath = AS_STR(Data); break;
#endif
	case API_V_EXEC_SIGN_REQ: m_SignatureLevel = (KPH_VERIFY_AUTHORITY)Data.To<uint32>(); break;
	case API_V_IMAGE_LOAD_PROTECTION: m_ImageLoadProtection = Data.To<bool>(); break;
	default: CGenericRule::ReadIValue(Index, Data);
	}
}

/////////////////////////////////////////////////////////////////////////////

void CProgramRule::WriteMVariant(XVariant& Rule, const SVarWriteOpt& Opts) const
{
	CGenericRule::WriteMVariant(Rule, Opts);

	switch (m_Type)
	{
	case EExecRuleType::eAllow:		Rule.Write(API_S_EXEC_RULE_ACTION, API_S_EXEC_RULE_ACTION_ALLOW); break;
	case EExecRuleType::eBlock:		Rule.Write(API_S_EXEC_RULE_ACTION, API_S_EXEC_RULE_ACTION_BLOCK); break;
	case EExecRuleType::eProtect:	Rule.Write(API_S_EXEC_RULE_ACTION, API_S_EXEC_RULE_ACTION_PROTECT); break;
	case EExecRuleType::eIsolate:	Rule.Write(API_S_EXEC_RULE_ACTION, API_S_EXEC_RULE_ACTION_ISOLATE); break;
	case EExecRuleType::eAudit:		Rule.Write(API_S_EXEC_RULE_ACTION, API_S_EXEC_RULE_ACTION_AUDIT); break;
	// todo other:
	}

	Rule.Write(API_S_FILE_PATH, TO_STR(m_ProgramPath));
#ifdef SAVE_NT_PATHS
	if(Opts.Flags & SVarWriteOpt::eSaveNtPaths) {
		Rule.Write(API_S_FILE_NT_PATH, TO_STR(m_PathPattern.Get()));
	}
#endif

	if (m_Type == EExecRuleType::eAllow || m_Type == EExecRuleType::eProtect)
	{
		switch (m_SignatureLevel) 
		{
		case KphDevAuthority:
		case KphUserAuthority:	Rule.Write(API_S_EXEC_SIGN_REQ, API_S_EXEC_SIGN_REQ_VERIFIED); break;

		case KphMsAuthority:
		case KphAvAuthority:	Rule.Write(API_S_EXEC_SIGN_REQ, API_S_EXEC_SIGN_REQ_MICROSOFT); break;

		case KphMsCodeAuthority:Rule.Write(API_S_EXEC_SIGN_REQ, API_S_EXEC_SIGN_REQ_TRUSTED); break;

		case KphUnkAuthority:	Rule.Write(API_S_EXEC_SIGN_REQ, API_S_EXEC_SIGN_REQ_OTHER); break;

		case KphUntestedAuthority:
		case KphNoAuthority:	Rule.Write(API_S_EXEC_SIGN_REQ, API_S_EXEC_SIGN_REQ_NONE); break;
		}
	}

	Rule.Write(API_S_IMAGE_LOAD_PROTECTION, m_ImageLoadProtection);
}

void CProgramRule::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_EXEC_RULE_ACTION))
	{
		ASTR Type = Data;
		if (Type == API_S_EXEC_RULE_ACTION_ALLOW)
			m_Type = EExecRuleType::eAllow;
		else if (Type == API_S_EXEC_RULE_ACTION_BLOCK)
			m_Type = EExecRuleType::eBlock;
		else if (Type == API_S_EXEC_RULE_ACTION_PROTECT)
			m_Type = EExecRuleType::eProtect;
		else if (Type == API_S_EXEC_RULE_ACTION_ISOLATE)
			m_Type = EExecRuleType::eIsolate;
		else if (Type == API_S_EXEC_RULE_ACTION_AUDIT)
			m_Type = EExecRuleType::eAudit;
		//else // todo other
		//	return STATUS_INVALID_PARAMETER;
	}

	else if (VAR_TEST_NAME(Name, API_S_FILE_PATH))
	{
		m_ProgramPath = AS_STR(Data);
	}
#ifdef LOAD_NT_PATHS
	else if (VAR_TEST_NAME(Name, API_S_FILE_NT_PATH))	m_ProgramNtPath = AS_STR(Data);
#endif

	else if (VAR_TEST_NAME(Name, API_S_EXEC_SIGN_REQ))
	{
		ASTR SignReq = Data;
		if (SignReq == API_S_EXEC_SIGN_REQ_VERIFIED)
			m_SignatureLevel = KphUserAuthority;
		else if (SignReq == API_S_EXEC_SIGN_REQ_MICROSOFT)
			m_SignatureLevel = KphAvAuthority;
		else if (SignReq == API_S_EXEC_SIGN_REQ_TRUSTED)
			m_SignatureLevel = KphMsCodeAuthority;
		else if (SignReq == API_S_EXEC_SIGN_REQ_OTHER)
			m_SignatureLevel = KphUnkAuthority;
		//else if (SignReq == "None")
		//	m_SignatureLevel = KphNoAuthority; // dont require signature
		//else
		//	return STATUS_INVALID_PARAMETER; // todo
	}

	else if (VAR_TEST_NAME(Name, API_S_IMAGE_LOAD_PROTECTION))	m_ImageLoadProtection = Data.To<bool>();

	else 
		CGenericRule::ReadMValue(Name, Data);
}
