

/////////////////////////////////////////////////////////////////////////////
// CProgramRule
//

void CProgramRule::WriteIVariant(VariantWriter& Rule, const SVarWriteOpt& Opts) const
{
	CGenericRule::WriteIVariant(Rule, Opts);

	Rule.Write(API_V_EXEC_RULE_ACTION, (uint32)m_Type);
	Rule.WriteEx(API_V_FILE_PATH, TO_STR(m_ProgramPath));
#ifdef SAVE_NT_PATHS
	if(Opts.Flags & SVarWriteOpt::eSaveNtPaths) {
		Rule.Write(API_V_FILE_NT_PATH, TO_STR(m_PathPattern.Get()));
	}
#endif
	if (m_Type == EExecRuleType::eAllow || m_Type == EExecRuleType::eProtect)
	{
		Rule.Write(API_V_EXEC_ALLOWED_SIGNERS, m_AllowedSignatures.Value);
		VariantWriter Collections(Rule.Allocator());
		Collections.BeginList();
		for(const auto& Collection : m_AllowedCollections)
			Collections.WriteEx(TO_STR(Collection));
		Rule.WriteVariant(API_V_EXEC_ALLOWED_COLLECTIONS, Collections.Finish());
	}
	Rule.Write(API_V_IMAGE_LOAD_PROTECTION, m_ImageLoadProtection);
	Rule.Write(API_V_IMAGE_COHERENCY_CHECKING, m_ImageCoherencyChecking);

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
	case API_V_EXEC_ALLOWED_SIGNERS: m_AllowedSignatures.Value = Data.To<uint32>(); break;
	case API_V_EXEC_ALLOWED_COLLECTIONS:
		LIST_CLEAR(m_AllowedCollections);
		for (uint32 i = 0; i < Data.Count(); i++)
			LIST_APPEND(m_AllowedCollections, AS_STR(Data[i]));
		break;
	case API_V_IMAGE_LOAD_PROTECTION: m_ImageLoadProtection = Data.To<bool>(); break;
	case API_V_IMAGE_COHERENCY_CHECKING: m_ImageCoherencyChecking = Data.To<bool>(); break;
	default: CGenericRule::ReadIValue(Index, Data);
	}
}

/////////////////////////////////////////////////////////////////////////////

void CProgramRule::WriteMVariant(VariantWriter& Rule, const SVarWriteOpt& Opts) const
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

	Rule.WriteEx(API_S_FILE_PATH, TO_STR(m_ProgramPath));
#ifdef SAVE_NT_PATHS
	if(Opts.Flags & SVarWriteOpt::eSaveNtPaths) {
		Rule.Write(API_S_FILE_NT_PATH, TO_STR(m_PathPattern.Get()));
	}
#endif

	if (m_Type == EExecRuleType::eAllow || m_Type == EExecRuleType::eProtect)
	{
		VariantWriter Signers(Rule.Allocator());
		Signers.BeginList();
		if(m_AllowedSignatures.Windows)
			Signers.Write(API_S_EXEC_ALLOWED_SIGNERS_WINDOWS);
		if(m_AllowedSignatures.Microsoft)
			Signers.Write(API_S_EXEC_ALLOWED_SIGNERS_MICROSOFT);
		if(m_AllowedSignatures.Antimalware)
			Signers.Write(API_S_EXEC_ALLOWED_SIGNERS_ANTIMALWARE);
		if(m_AllowedSignatures.Authenticode)
			Signers.Write(API_S_EXEC_ALLOWED_SIGNERS_AUTHENTICODE);
		if (m_AllowedSignatures.Store)
			Signers.Write(API_S_EXEC_ALLOWED_SIGNERS_STORE);
		if(m_AllowedSignatures.User)
			Signers.Write(API_S_EXEC_ALLOWED_SIGNERS_USER);
		if(m_AllowedSignatures.Enclave)
			Signers.Write(API_S_EXEC_ALLOWED_SIGNERS_ENCLAVE);
		for(const auto& Collection : m_AllowedCollections)
			Signers.WriteEx(TO_STR(Collection));
		Rule.WriteVariant(API_S_EXEC_ALLOWED_SIGNERS, Signers.Finish());
	}

	Rule.Write(API_S_IMAGE_LOAD_PROTECTION, m_ImageLoadProtection);
	Rule.Write(API_S_IMAGE_COHERENCY_CHECKING, m_ImageCoherencyChecking);
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
			m_AllowedSignatures.User = TRUE;
		else if (SignReq == API_S_EXEC_SIGN_REQ_MICROSOFT || SignReq == API_S_EXEC_SIGN_REQ_TRUSTED)
			m_AllowedSignatures.User = m_AllowedSignatures.Windows = m_AllowedSignatures.Microsoft = m_AllowedSignatures.Antimalware = TRUE;
	}

	else if (VAR_TEST_NAME(Name, API_S_EXEC_ALLOWED_SIGNERS))
	{
		m_AllowedSignatures.Value = 0;
		LIST_CLEAR(m_AllowedCollections);
		for (uint32 i = 0; i < Data.Count(); i++) {
			ASTR SignReq = Data[i];
			if(SignReq == API_S_EXEC_ALLOWED_SIGNERS_WINDOWS || SignReq == "[MS]")			m_AllowedSignatures.Windows = TRUE;
			else if(SignReq == API_S_EXEC_ALLOWED_SIGNERS_MICROSOFT)	m_AllowedSignatures.Microsoft = TRUE;
			else if(SignReq == API_S_EXEC_ALLOWED_SIGNERS_ANTIMALWARE)	m_AllowedSignatures.Antimalware = TRUE;
			else if(SignReq == API_S_EXEC_ALLOWED_SIGNERS_AUTHENTICODE)	m_AllowedSignatures.Authenticode = TRUE;
			else if(SignReq == API_S_EXEC_ALLOWED_SIGNERS_STORE)		m_AllowedSignatures.Store = TRUE;
			else if(SignReq == API_S_EXEC_ALLOWED_SIGNERS_USER)			m_AllowedSignatures.User = TRUE;
			else if(SignReq == API_S_EXEC_ALLOWED_SIGNERS_ENCLAVE)		m_AllowedSignatures.Enclave = TRUE;
			else
				LIST_APPEND(m_AllowedCollections, AS_STR(Data[i]));
		}
	}

	else if (VAR_TEST_NAME(Name, API_S_IMAGE_LOAD_PROTECTION))	m_ImageLoadProtection = Data.To<bool>();
	else if (VAR_TEST_NAME(Name, API_S_IMAGE_COHERENCY_CHECKING))	m_ImageCoherencyChecking = Data.To<bool>();

	else 
		CGenericRule::ReadMValue(Name, Data);
}
