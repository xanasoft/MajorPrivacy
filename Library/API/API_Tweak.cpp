

/////////////////////////////////////////////////////////////////////////////
// CAbstractTweak
//

#ifdef TWEAK_SVC
void CAbstractTweak::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	Data.WriteEx(API_V_TWEAK_ID, m_Id);
	Data.WriteEx(API_V_PARENT, m_ParentId);

    Data.WriteEx(API_V_TWEAK_NAME, m_Name);
	Data.Write(API_V_TWEAK_INDEX, m_Index);

    Data.Write(API_V_TWEAK_HINT, (uint32)m_Hint);

    //ETweakStatus Status = GetStatus();
    //Data.Write(API_V_TWEAK_STATUS, (uint32)Status);
}
#endif

void CAbstractTweak::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_TWEAK_ID:		m_Id = AS_STR(Data); break;
	case API_V_PARENT:			m_ParentId = AS_STR(Data); break;
	case API_V_TWEAK_NAME:		m_Name = AS_STR(Data); break;
	case API_V_TWEAK_INDEX:		m_Index = Data.To<sint32>(); break;
	case API_V_TWEAK_HINT:		m_Hint = (ETweakHint)Data.To<uint32>(); break;
//#ifdef TWEAK_GUI
	//case API_V_TWEAK_STATUS:	m_Status = (ETweakStatus)Data.To<uint32>(); break;
//#endif
	}
}

#ifdef TWEAK_SVC
void CAbstractTweak::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	Data.WriteEx(API_S_TWEAK_ID, m_Id);
	Data.WriteEx(API_S_PARENT, m_ParentId);

	Data.WriteEx(API_S_TWEAK_NAME, m_Name);
	Data.Write(API_S_TWEAK_INDEX, m_Index);

	switch (m_Hint)
	{
	case ETweakHint::eNone:         Data.Write(API_S_TWEAK_HINT, API_S_TWEAK_HINT_NONE); break;
	case ETweakHint::eRecommended:  Data.Write(API_S_TWEAK_HINT, API_S_TWEAK_HINT_RECOMMENDED); break;
	case ETweakHint::eNotRecommended: Data.Write(API_S_TWEAK_HINT, API_S_TWEAK_HINT_NOT_RECOMMENDED); break;
	case ETweakHint::eBreaking:     Data.Write(API_S_TWEAK_HINT, API_S_TWEAK_HINT_BREAKING); break;
	}

	//ETweakStatus Status = GetStatus();
	//switch (Status)
	//{
	//case ETweakStatus::eNotSet:     Data.Write(API_S_TWEAK_STATUS, API_S_TWEAK_STATUS_NOT_SET); break;
	//case ETweakStatus::eApplied:    Data.Write(API_S_TWEAK_STATUS, API_S_TWEAK_STATUS_APPLIED); break;
	//case ETweakStatus::eSet:        Data.Write(API_S_TWEAK_STATUS, API_S_TWEAK_STATUS_SET); break;
	//case ETweakStatus::eMissing:    Data.Write(API_S_TWEAK_STATUS, API_S_TWEAK_STATUS_MISSING); break;
	//default:                        Data.Write(API_S_TWEAK_STATUS, API_S_TWEAK_STATUS_GROUP); 
	//}
}
#endif

void CAbstractTweak::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_TWEAK_ID))				m_Id = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_PARENT))				m_ParentId = AS_STR(Data);

	else if (VAR_TEST_NAME(Name, API_S_TWEAK_NAME))			m_Name = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_TWEAK_INDEX))		m_Index = Data.To<sint32>();

	else if (VAR_TEST_NAME(Name, API_S_TWEAK_HINT))
	{
		ASTR Hint = Data;
		if (Hint == API_S_TWEAK_HINT_RECOMMENDED)			m_Hint = ETweakHint::eRecommended;
		else if (Hint == API_S_TWEAK_HINT_NOT_RECOMMENDED)	m_Hint = ETweakHint::eNotRecommended;
		else if (Hint == API_S_TWEAK_HINT_BREAKING)			m_Hint = ETweakHint::eBreaking;
		else												m_Hint = ETweakHint::eNone;
	}

//#ifdef TWEAK_GUI
	//else if (VAR_TEST_NAME(Name, API_S_TWEAK_STATUS))
	//{
	//	auto Status = AS_STR(Data);
	//	if (Status == API_S_TWEAK_STATUS_NOT_SET)			m_Status = ETweakStatus::eNotSet;
	//	else if (Status == API_S_TWEAK_STATUS_SET)			m_Status = ETweakStatus::eSet;
	//	else if (Status == API_S_TWEAK_STATUS_APPLIED)		m_Status = ETweakStatus::eApplied;
	//	else if (Status == API_S_TWEAK_STATUS_MISSING)		m_Status = ETweakStatus::eMissing;
	//	else if (Status == API_S_TWEAK_STATUS_GROUP)		m_Status = ETweakStatus::eGroup;
	//	else m_Status = ETweakStatus::eNotSet;
	//}
//#endif
}

/////////////////////////////////////////////////////////////////////////////
// CTweakList
//

#ifdef TWEAK_SVC
void CTweakList::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CAbstractTweak::WriteIVariant(Data, Opts);
}
#endif

void CTweakList::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	default: CAbstractTweak::ReadIValue(Index, Data);
	}
}

#ifdef TWEAK_SVC
void CTweakList::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CAbstractTweak::WriteMVariant(Data, Opts);
}
#endif

void CTweakList::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	CAbstractTweak::ReadMValue(Name, Data);
}

/////////////////////////////////////////////////////////////////////////////
// CTweakGroup
//

#ifdef TWEAK_SVC
void CTweakGroup::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweakList::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eGroup);
}
#endif

void CTweakGroup::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	default: CTweakList::ReadIValue(Index, Data);
	}
}

#ifdef TWEAK_SVC
void CTweakGroup::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweakList::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_GROUP);
}
#endif

void CTweakGroup::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	CTweakList::ReadMValue(Name, Data);
}

/////////////////////////////////////////////////////////////////////////////
// CTweakSet
//

#ifdef TWEAK_SVC
void CTweakSet::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweakList::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eSet);
}
#endif

void CTweakSet::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	default: CTweakList::ReadIValue(Index, Data);
	}
}

#ifdef TWEAK_SVC
void CTweakSet::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweakList::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_SET);
}
#endif

void CTweakSet::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	CTweakList::ReadMValue(Name, Data);
}

/////////////////////////////////////////////////////////////////////////////
// CTweak
//

#ifdef TWEAK_SVC
void CTweak::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CAbstractTweak::WriteIVariant(Data, Opts);

	Data.WriteEx(API_V_TWEAK_WIN_VER, CTweak::Ver2Str(m_Ver));

	Data.Write(API_V_TWEAK_IS_SET, m_Set);
	//Data.Write(API_V_TWEAK_IS_APPLIED, m_Applied);
}
#endif

void CTweak::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_TWEAK_WIN_VER:
#ifdef TWEAK_GUI
		m_WinVer = AS_STR(Data);
#else
		m_Ver = CTweak::Str2Ver(AS_STR(Data));
#endif
		break;
#ifdef TWEAK_SVC
	case API_V_TWEAK_IS_SET:		m_Set = Data.To<bool>(); break;
#endif
	default: CAbstractTweak::ReadIValue(Index, Data);
	}
}

#ifdef TWEAK_SVC
void CTweak::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CAbstractTweak::WriteMVariant(Data, Opts);

	Data.WriteEx(API_S_TWEAK_WIN_VER, CTweak::Ver2Str(m_Ver));

	Data.Write(API_S_TWEAK_IS_SET, m_Set);
	//Data.Write(API_S_TWEAK_IS_APPLIED, m_Applied);
}
#endif

void CTweak::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_TWEAK_WIN_VER))
#ifdef TWEAK_GUI
		m_WinVer = AS_STR(Data);
#else
		m_Ver = CTweak::Str2Ver(AS_STR(Data));
#endif
#ifdef TWEAK_SVC
	else if (VAR_TEST_NAME(Name, API_S_TWEAK_IS_SET))
		m_Set = Data.To<bool>();
#endif
	else
		CAbstractTweak::ReadMValue(Name, Data);
}

#ifdef TWEAK_GUI
void CTweak::FromVariant(const class XVariant& Tweak)
{
	CAbstractTweak::FromVariant(Tweak);

	QtVariantReader Reader(Tweak);

	if (Tweak.GetType() == VAR_TYPE_MAP)
	{
		ETweakType Type = (ETweakType)Reader.Find(API_S_TWEAK_TYPE).To<uint32>();
		if (Type == ETweakType::eReg)
		{
			m_Info = GetTypeStr() + ":\n" +
				Reader.Find(API_S_REG_KEY).AsQStr() + "\n" +
				Reader.Find(API_S_VALUE).AsQStr() + " = " + Reader.Find(API_S_DATA).AsQStr();
		}
		else if (m_Type == ETweakType::eGpo)
		{
			m_Info = GetTypeStr() + ":\n" +
				Reader.Find(API_S_REG_KEY).AsQStr() + "\n" +
				Reader.Find(API_S_VALUE).AsQStr() + " = " + Reader.Find(API_S_DATA).AsQStr();
		}
		else if (m_Type == ETweakType::eSvc)
		{
			m_Info = GetTypeStr() + ":\n" +
				Reader.Find(API_S_SERVICE_TAG).AsQStr();
		}
		else if (m_Type == ETweakType::eTask)
		{
			m_Info = GetTypeStr() + ":\n" +
				Reader.Find(API_S_FOLDER).AsQStr() + "\\" + Reader.Find(API_S_ENTRY).AsQStr();
		}
		else if (m_Type == ETweakType::eRes)
		{
			m_Info = GetTypeStr() + ":\n" +
				Reader.Find(API_S_FILE_PATH).AsQStr();
		}
		else if (m_Type == ETweakType::eExec)
		{
			m_Info = GetTypeStr() + ":\n" +
				Reader.Find(API_S_FILE_PATH).AsQStr();
		}
		else if (m_Type == ETweakType::eFw)
		{
			m_Info = GetTypeStr() + ":\n" +
				Reader.Find(API_S_FILE_PATH).AsQStr();
		}
	}
	else if (Tweak.GetType() == VAR_TYPE_INDEX)
	{
		ETweakType Type = (ETweakType)Reader.Find(API_V_TWEAK_TYPE).To<uint32>();
		if (Type == ETweakType::eReg)
		{
			m_Info = GetTypeStr() + ":\n" +
				Reader.Find(API_V_REG_KEY).AsQStr() + "\n" +
				Reader.Find(API_V_VALUE).AsQStr() + " = " + Reader.Find(API_V_DATA).AsQStr();
		}
		else if (m_Type == ETweakType::eGpo)
		{
			m_Info = GetTypeStr() + ":\n" +
				Reader.Find(API_V_REG_KEY).AsQStr() + "\n" +
				Reader.Find(API_V_VALUE).AsQStr() + " = " + Reader.Find(API_V_DATA).AsQStr();
		}
		else if (m_Type == ETweakType::eSvc)
		{
			m_Info = GetTypeStr() + ":\n" +
				Reader.Find(API_V_SERVICE_TAG).AsQStr();
		}
		else if (m_Type == ETweakType::eTask)
		{
			m_Info = GetTypeStr() + ":\n" +
				Reader.Find(API_V_FOLDER).AsQStr() + "\\" + Reader.Find(API_V_ENTRY).AsQStr();
		}
		else if (m_Type == ETweakType::eRes)
		{
			m_Info = GetTypeStr() + ":\n" +
				Reader.Find(API_V_FILE_PATH).AsQStr();
		}
		else if (m_Type == ETweakType::eExec)
		{
			m_Info = GetTypeStr() + ":\n" +
				Reader.Find(API_V_FILE_PATH).AsQStr();
		}
		else if (m_Type == ETweakType::eFw)
		{
			m_Info = GetTypeStr() + ":\n" +
				Reader.Find(API_V_FILE_PATH).AsQStr();
		}
	}
}
#endif

/////////////////////////////////////////////////////////////////////////////
// CRegTweak
//

#ifdef TWEAK_SVC
void CRegTweak::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eReg);

	Data.WriteEx(API_V_REG_KEY, m_Key);
	Data.WriteEx(API_V_VALUE, m_Value);
	Data.WriteVariant(API_V_DATA, m_Data);
}

void CRegTweak::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_REG_KEY:		m_Key = AS_STR(Data); break;
	case API_V_VALUE:		m_Value = AS_STR(Data); break;
	case API_V_DATA:		m_Data = Data; break;
	default: CTweak::ReadIValue(Index, Data);
	}
}
#endif

#ifdef TWEAK_SVC
void CRegTweak::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_REG);

	Data.WriteEx(API_S_REG_KEY, m_Key);
	Data.WriteEx(API_S_VALUE, m_Value);
	Data.WriteVariant(API_S_DATA, m_Data);
}

void CRegTweak::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_REG_KEY))				m_Key = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_VALUE))			m_Value = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_DATA))				m_Data = Data;
	else
		CTweak::ReadMValue(Name, Data);
}
#endif

/////////////////////////////////////////////////////////////////////////////
// CGpoTweak
//

#ifdef TWEAK_SVC
void CGpoTweak::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eGpo);

	Data.WriteEx(API_V_REG_KEY, m_Key);
	Data.WriteEx(API_V_VALUE, m_Value);
	Data.WriteVariant(API_V_DATA, m_Data);
}

void CGpoTweak::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_REG_KEY:		m_Key = AS_STR(Data); break;
	case API_V_VALUE:		m_Value = AS_STR(Data); break;
	case API_V_DATA:		m_Data = Data; break;
	default: CTweak::ReadIValue(Index, Data);
	}
}
#endif

#ifdef TWEAK_SVC
void CGpoTweak::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_GPO);

	Data.WriteEx(API_S_REG_KEY, m_Key);
	Data.WriteEx(API_S_VALUE, m_Value);
	Data.WriteVariant(API_S_DATA, m_Data);
}

void CGpoTweak::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_REG_KEY))				m_Key = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_VALUE))			m_Value = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_DATA))				m_Data = Data;
	else
		CTweak::ReadMValue(Name, Data);
}
#endif

/////////////////////////////////////////////////////////////////////////////
// CSvcTweak
//

#ifdef TWEAK_SVC
void CSvcTweak::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eSvc);

	Data.WriteEx(API_V_SERVICE_TAG, m_SvcTag);
}

void CSvcTweak::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_SERVICE_TAG:		m_SvcTag = AS_STR(Data); break;
	default: CTweak::ReadIValue(Index, Data);
	}
}
#endif

#ifdef TWEAK_SVC
void CSvcTweak::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_SVC);

	Data.WriteEx(API_S_SERVICE_TAG, m_SvcTag);
}

void CSvcTweak::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_SERVICE_TAG))			m_SvcTag = AS_STR(Data);
	else
		CTweak::ReadMValue(Name, Data);
}
#endif

/////////////////////////////////////////////////////////////////////////////
// CTaskTweak
//

#ifdef TWEAK_SVC
void CTaskTweak::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eTask);

	Data.WriteEx(API_V_FOLDER, m_Folder);
	Data.WriteEx(API_V_ENTRY, m_Entry);
}

void CTaskTweak::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_FOLDER:			m_Folder = AS_STR(Data); break;
	case API_V_ENTRY:			m_Entry = AS_STR(Data); break;
	default: CTweak::ReadIValue(Index, Data);
	}
}
#endif

#ifdef TWEAK_SVC
void CTaskTweak::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_TASK);

	Data.WriteEx(API_S_FOLDER, m_Folder);
	Data.WriteEx(API_S_ENTRY, m_Entry);
}

void CTaskTweak::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_FOLDER))				m_Folder = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_ENTRY))			m_Entry = AS_STR(Data);
	else
		CTweak::ReadMValue(Name, Data);
}
#endif

/////////////////////////////////////////////////////////////////////////////
// CResTweak
//

#ifdef TWEAK_SVC
void CResTweak::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eRes);

	Data.WriteEx(API_V_FILE_PATH, m_PathPattern);
}

void CResTweak::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_FILE_PATH:		m_PathPattern = AS_STR(Data); break;
	default: CTweak::ReadIValue(Index, Data);
	}
}
#endif

#ifdef TWEAK_SVC
void CResTweak::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_RES);

	Data.WriteEx(API_S_FILE_PATH, m_PathPattern);
}

void CResTweak::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_FILE_PATH))			m_PathPattern = AS_STR(Data);
	else
		CTweak::ReadMValue(Name, Data);
}
#endif

/////////////////////////////////////////////////////////////////////////////
// CExecTweak
//

#ifdef TWEAK_SVC
void CExecTweak::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eExec);

	Data.WriteEx(API_V_FILE_PATH, m_PathPattern);
}

void CExecTweak::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_FILE_PATH:		m_PathPattern = AS_STR(Data); break;
	default: CTweak::ReadIValue(Index, Data);
	}
}
#endif

#ifdef TWEAK_SVC
void CExecTweak::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_EXEC);

	Data.WriteEx(API_S_FILE_PATH, m_PathPattern);
}

void CExecTweak::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_FILE_PATH))			m_PathPattern = AS_STR(Data);
	else
		CTweak::ReadMValue(Name, Data);
}
#endif

/////////////////////////////////////////////////////////////////////////////
// CFwTweak
//

#ifdef TWEAK_SVC
void CFwTweak::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eFw);

	Data.WriteEx(API_V_FILE_PATH, m_PathPattern);
	Data.WriteEx(API_V_SERVICE_TAG, m_SvcTag);
}

void CFwTweak::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_FILE_PATH:		m_PathPattern = AS_STR(Data); break;
	case API_V_SERVICE_TAG:		m_SvcTag = AS_STR(Data); break;
	default: CTweak::ReadIValue(Index, Data);
	}
}
#endif

#ifdef TWEAK_SVC
void CFwTweak::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_FW);

	Data.WriteEx(API_S_FILE_PATH, m_PathPattern);
	Data.WriteEx(API_S_SERVICE_TAG, m_SvcTag);
}

void CFwTweak::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_FILE_PATH))			m_PathPattern = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_SERVICE_TAG))	m_SvcTag = AS_STR(Data);
	else
		CTweak::ReadMValue(Name, Data);
}
#endif