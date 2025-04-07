

/////////////////////////////////////////////////////////////////////////////
// Index Serializer
//

#ifndef TWEAKS_RO

void CAbstractTweak::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
    Data.WriteEx(API_V_NAME, m_Name);

    Data.Write(API_V_TWEAK_HINT, (uint32)m_Hint);

    ETweakStatus Status = GetStatus();
    Data.Write(API_V_TWEAK_STATUS, (uint32)Status);
}

void CTweakList::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CAbstractTweak::WriteIVariant(Data, Opts);

	VariantWriter List;
	List.BeginList();
	for (auto I : m_List) {
		if (I->IsAvailable())
			List.WriteVariant(I->ToVariant(Opts));
	}
	Data.WriteVariant(API_V_TWEAK_LIST, List.Finish());
}

void CTweakGroup::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweakList::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eGroup);
}

void CTweakSet::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweakList::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eSet);
}

void CTweak::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CAbstractTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_IS_SET, m_Set);
}

void CRegTweak::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eReg);

	Data.WriteEx(API_V_REG_KEY, m_Key);
	Data.WriteEx(API_V_VALUE, m_Value);
	Data.WriteVariant(API_V_DATA, m_Data);
}

void CGpoTweak::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eGpo);

	Data.WriteEx(API_V_REG_KEY, m_Key);
	Data.WriteEx(API_V_VALUE, m_Value);
	Data.WriteVariant(API_V_DATA, m_Data);
}

void CSvcTweak::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eSvc);

	Data.WriteEx(API_V_SERVICE_TAG, m_SvcTag);
}

void CTaskTweak::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eTask);

	Data.WriteEx(API_V_FOLDER, m_Folder);
	Data.WriteEx(API_V_ENTRY, m_Entry);
}

void CFSTweak::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eFS);

	Data.WriteEx(API_V_FILE_PATH, m_PathPattern);
}

void CExecTweak::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eExec);

	Data.WriteEx(API_V_FILE_PATH, m_PathPattern);
}

void CFwTweak::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eFw);

	Data.WriteVariant(API_V_PROG_ID, m_ProgID.ToVariant(Opts));
}

#endif

#ifndef TWEAKS_WO

void CAbstractTweak::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_NAME:			m_Name = Data.AsQStr(); break;
	case API_V_TWEAK_HINT:		m_Hint = (ETweakHint)Data.To<uint32>(); break;
	case API_V_TWEAK_STATUS:	m_Status = (ETweakStatus)Data.To<uint32>(); break;
	}
}

void CTweakList::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_TWEAK_LIST:	ReadList(Data); break;
	default: CAbstractTweak::ReadIValue(Index, Data);
	}
}

void CTweak::ReadIValue(uint32 Index, const XVariant& Data)
{
	CAbstractTweak::ReadIValue(Index, Data);
}

#endif


/////////////////////////////////////////////////////////////////////////////
// Map Serializer
//

#ifndef TWEAKS_RO

void CAbstractTweak::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	Data.WriteEx(API_S_NAME, m_Name);

	switch (m_Hint)
	{
	case ETweakHint::eNone:         Data.Write(API_S_TWEAK_HINT, API_S_TWEAK_HINT_NONE); break;
	case ETweakHint::eRecommended:  Data.Write(API_S_TWEAK_HINT, API_S_TWEAK_HINT_RECOMMENDED); break;
	}

	ETweakStatus Status = GetStatus();
	switch (Status)
	{
	case ETweakStatus::eNotSet:     Data.Write(API_S_TWEAK_STATUS, API_S_TWEAK_STATUS_NOT_SET); break;
	case ETweakStatus::eApplied:    Data.Write(API_S_TWEAK_STATUS, API_S_TWEAK_STATUS_APPLIED); break;
	case ETweakStatus::eSet:        Data.Write(API_S_TWEAK_STATUS, API_S_TWEAK_STATUS_SET); break;
	case ETweakStatus::eMissing:    Data.Write(API_S_TWEAK_STATUS, API_S_TWEAK_STATUS_MISSING); break;
	default:                        Data.Write(API_S_TWEAK_STATUS, API_S_TWEAK_STATUS_GROUP); 
	}
}

void CTweakList::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CAbstractTweak::WriteMVariant(Data, Opts);

	VariantWriter List;
	List.BeginList();
	for (auto I : m_List) {
		if (I->IsAvailable())
			List.WriteVariant(I->ToVariant(Opts));
	}
	Data.WriteVariant(API_S_TWEAK_LIST, List.Finish());
}

void CTweakGroup::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweakList::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_GROUP);
}

void CTweakSet::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweakList::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_SET);
}

void CTweak::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CAbstractTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_IS_SET, m_Set);
}

void CRegTweak::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_REG);

	Data.WriteEx(API_S_REG_KEY, m_Key);
	Data.WriteEx(API_S_VALUE, m_Value);
	Data.WriteVariant(API_S_DATA, m_Data);
}

void CGpoTweak::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_GPO);

	Data.WriteEx(API_S_REG_KEY, m_Key);
	Data.WriteEx(API_S_VALUE, m_Value);
	Data.WriteVariant(API_S_DATA, m_Data);
}

void CSvcTweak::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_SVC);

	Data.WriteEx(API_S_SERVICE_TAG, m_SvcTag);
}

void CTaskTweak::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_TASK);

	Data.WriteEx(API_S_FOLDER, m_Folder);
	Data.WriteEx(API_S_ENTRY, m_Entry);
}

void CFSTweak::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_FS);

	Data.WriteEx(API_S_FILE_PATH, m_PathPattern);
}

void CExecTweak::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_EXEC);

	Data.WriteEx(API_S_FILE_PATH, m_PathPattern);
}

void CFwTweak::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_FW);

	Data.WriteVariant(API_S_PROG_ID, m_ProgID.ToVariant(Opts));
}

#endif

#ifndef TWEAKS_WO

void CAbstractTweak::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_NAME))					m_Name = Data.AsQStr();

	else if (VAR_TEST_NAME(Name, API_S_TWEAK_HINT))
	{
		QString Hint = Data.AsQStr();
		if (Hint == API_S_TWEAK_HINT_RECOMMENDED)			m_Hint = ETweakHint::eRecommended;
		else												m_Hint = ETweakHint::eNone;
	}

	else if (VAR_TEST_NAME(Name, API_S_TWEAK_STATUS))
	{
		QString Status = Data.AsQStr();
		if (Status == API_S_TWEAK_STATUS_NOT_SET)			m_Status = ETweakStatus::eNotSet;
		else if (Status == API_S_TWEAK_STATUS_SET)			m_Status = ETweakStatus::eSet;
		else if (Status == API_S_TWEAK_STATUS_APPLIED)		m_Status = ETweakStatus::eApplied;
		else if (Status == API_S_TWEAK_STATUS_MISSING)		m_Status = ETweakStatus::eMissing;
		else if (Status == API_S_TWEAK_STATUS_GROUP)		m_Status = ETweakStatus::eGroup;
		else m_Status = ETweakStatus::eNotSet;
	}
}

void CTweakList::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_TWEAK_LIST))
		ReadList(Data);
	else
		CAbstractTweak::ReadMValue(Name, Data);
}

void CTweak::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	CAbstractTweak::ReadMValue(Name, Data);
}

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
		else if (m_Type == ETweakType::eFS)
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
			CProgramID ID;
			ID.FromVariant(Reader.Find(API_S_PROG_ID));
			m_Info = GetTypeStr() + ":\n" +
				ID.ToString();
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
		else if (m_Type == ETweakType::eFS)
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
			CProgramID ID;
			ID.FromVariant(Reader.Find(API_V_PROG_ID));
			m_Info = GetTypeStr() + ":\n" +
				ID.ToString();
		}
	}
}

#endif