

/////////////////////////////////////////////////////////////////////////////
// CGenericRule
//

void CGenericRule::WriteIVariant(VariantWriter& Rule, const SVarWriteOpt& Opts) const
{
#ifdef KERNEL_MODE
	if(!IS_EMPTY(m_Guid))
		Rule.Write(API_V_GUID, TO_STR(m_Guid));
#else
	if(!m_Guid.IsNull())
		Rule.WriteVariant(API_V_GUID, m_Guid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids, Rule.Allocator()));
#endif
	Rule.Write(API_V_ENABLED, m_bEnabled);

#ifdef KERNEL_MODE
	Rule.Write(API_V_ENCLAVE, TO_STR(m_Enclave));
#else
	if(!m_Enclave.IsNull())
		Rule.WriteVariant(API_V_ENCLAVE, m_Enclave.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids, Rule.Allocator()));
#endif

	Rule.WriteEx(API_V_USER, TO_STR(m_User));
	Rule.WriteVariant(API_V_USER_SID, m_UserSid);

	Rule.Write(API_V_TEMP, m_bTemporary);
	Rule.Write(API_V_TIMEOUT, m_uTimeOut);

	Rule.WriteEx(API_V_NAME, TO_STR(m_Name));
	//Rule.WriteEx(API_V_RULE_GROUP, TO_STR(m_Grouping));
	Rule.WriteEx(API_V_RULE_DESCR, TO_STR(m_Description));

	Rule.WriteVariant(API_V_DATA, m_Data);
}

void CGenericRule::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch(Index)
	{
#ifdef KERNEL_MODE
	case API_V_GUID:		if(IS_EMPTY(m_Guid)) m_Guid = AS_STR(Data); break;
#else
	case API_V_GUID:		if(m_Guid.IsNull()) m_Guid.FromVariant(Data); break;
#endif
	case API_V_ENABLED:		m_bEnabled = Data.To<bool>(); break;

#ifdef KERNEL_MODE
	case API_V_ENCLAVE:		m_Enclave = AS_STR(Data); break;
#else
	case API_V_ENCLAVE:		m_Enclave.FromVariant(Data); break;
#endif

	case API_V_USER:		m_User = AS_STR(Data); break;
	case API_V_USER_SID:	m_UserSid = Data.Clone(); break;

	case API_V_TEMP:		m_bTemporary = Data.To<bool>(); break;
	case API_V_TIMEOUT:		m_uTimeOut = Data.To<uint64>(); break;

	case API_V_NAME:		m_Name = AS_STR(Data); break;
	//case API_V_RULE_GROUP:	m_Grouping = AS_STR(Data); break;
	case API_V_RULE_DESCR:	m_Description = AS_STR(Data); break;

	case API_V_DATA:		m_Data = Data.Clone(); break;

	default: break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CGenericRule::WriteMVariant(VariantWriter& Rule, const SVarWriteOpt& Opts) const
{
#ifdef KERNEL_MODE
	if(!IS_EMPTY(m_Guid))
		Rule.Write(API_S_GUID, TO_STR(m_Guid));
#else
	if(!m_Guid.IsNull())
		Rule.WriteVariant(API_S_GUID, m_Guid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids, Rule.Allocator()));
#endif
	Rule.Write(API_S_ENABLED, m_bEnabled);

#ifdef KERNEL_MODE
	Rule.Write(API_S_ENCLAVE, TO_STR(m_Enclave));
#else
	if(!m_Enclave.IsNull())
		Rule.WriteVariant(API_S_ENCLAVE, m_Enclave.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids, Rule.Allocator()));
#endif

	Rule.WriteEx(API_S_USER, TO_STR(m_User));
	Rule.WriteVariant(API_S_USER_SID, m_UserSid);

	Rule.Write(API_S_TEMP, m_bTemporary);
	Rule.Write(API_S_TIMEOUT, m_uTimeOut);

	Rule.WriteEx(API_S_NAME, TO_STR(m_Name));
	//Rule.WriteEx(API_S_RULE_GROUP, TO_STR(m_Grouping));
	Rule.WriteEx(API_S_RULE_DESCR, TO_STR(m_Description));

	Rule.WriteVariant(API_S_DATA, m_Data);
}

void CGenericRule::ReadMValue(const SVarName& Name, const XVariant& Data)
{
#ifdef KERNEL_MODE
	if (VAR_TEST_NAME(Name, API_S_GUID))				{ if(IS_EMPTY(m_Guid)) m_Guid = AS_STR(Data); }
#else
	if (VAR_TEST_NAME(Name, API_S_GUID))				{ if(m_Guid.IsNull()) m_Guid.FromVariant(Data); }
#endif
	else if (VAR_TEST_NAME(Name, API_S_ENABLED))		m_bEnabled = Data.To<bool>();

#ifdef KERNEL_MODE
	else if (VAR_TEST_NAME(Name, API_S_ENCLAVE))		m_Enclave = AS_STR(Data);
#else
	else if (VAR_TEST_NAME(Name, API_S_ENCLAVE))		m_Enclave.FromVariant(Data);
#endif

	else if (VAR_TEST_NAME(Name, API_S_USER))			m_User = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_USER_SID))		m_UserSid = Data.Clone();

	else if (VAR_TEST_NAME(Name, API_S_TEMP))			m_bTemporary = Data.To<bool>();
	else if (VAR_TEST_NAME(Name, API_S_TIMEOUT))		m_uTimeOut = Data.To<uint64>();

	else if (VAR_TEST_NAME(Name, API_S_NAME))			m_Name = AS_STR(Data);
	//else if (VAR_TEST_NAME(Name, API_S_RULE_GROUP))	m_Grouping = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_RULE_DESCR))		m_Description = AS_STR(Data);

	else if (VAR_TEST_NAME(Name, API_S_DATA))	        m_Data = Data.Clone();

	// else unknown tag
}