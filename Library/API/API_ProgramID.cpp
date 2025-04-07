

/////////////////////////////////////////////////////////////////////////////
// CProgramID
//

EProgramType CProgramID::ReadType(const XVariant& Data, SVarWriteOpt::EFormat& Format)
{
	EProgramType Type = EProgramType::eUnknown;
	if (Data.GetType() == VAR_TYPE_MAP)
	{
		Format = SVarWriteOpt::eMap;
		ASTR TypeStr = VariantReader(Data).Find(API_S_PROG_TYPE);
		if (TypeStr == API_S_PROG_TYPE_FILE)			Type = EProgramType::eProgramFile;
		else if (TypeStr == API_S_PROG_TYPE_PATTERN)	Type = EProgramType::eFilePattern;
		else if (TypeStr == API_S_PROG_TYPE_INSTALL)	Type = EProgramType::eAppInstallation;
		else if (TypeStr == API_S_PROG_TYPE_SERVICE)	Type = EProgramType::eWindowsService;
		else if (TypeStr == API_S_PROG_TYPE_PACKAGE)	Type = EProgramType::eAppPackage;
		else if (TypeStr == API_S_PROG_TYPE_GROUP)		Type = EProgramType::eProgramGroup;
		else if (TypeStr == API_S_PROG_TYPE_ROOT)		Type = EProgramType::eProgramRoot;
		else if (TypeStr == API_S_PROG_TYPE_All)		Type = EProgramType::eAllPrograms;
	}
	else if (Data.GetType() == VAR_TYPE_INDEX)
	{
		Format = SVarWriteOpt::eIndex;
		Type = (EProgramType)VariantReader(Data).Find(API_V_PROG_TYPE).To<uint32>();
	}
	return Type;
}

ASTR CProgramID::TypeToStr(EProgramType Type)
{
	switch (Type)
	{
	case EProgramType::eProgramFile:		return API_S_PROG_TYPE_FILE;
	case EProgramType::eFilePattern:		return API_S_PROG_TYPE_PATTERN;
	case EProgramType::eAppInstallation:	return API_S_PROG_TYPE_INSTALL;
	case EProgramType::eWindowsService:		return API_S_PROG_TYPE_SERVICE;
	case EProgramType::eAppPackage:			return API_S_PROG_TYPE_PACKAGE;
	case EProgramType::eProgramGroup:		return API_S_PROG_TYPE_GROUP;
	case EProgramType::eAllPrograms:		return API_S_PROG_TYPE_All;
	case EProgramType::eProgramRoot:		return API_S_PROG_TYPE_ROOT;
	default:
		ASSERT(0);
		return "";
	}
}

XVariant CProgramID::ToVariant(const SVarWriteOpt& Opts) const
{
	VariantWriter ID;
	if (Opts.Format == SVarWriteOpt::eIndex) 
	{
		ID.BeginIndex();

		ID.Write(API_V_PROG_TYPE, (uint32)m_Type);
		if (m_Type != EProgramType::eAllPrograms && m_Type != EProgramType::eProgramGroup)
			ID.WriteEx(API_V_FILE_PATH, TO_STR(m_FilePath));

		switch (m_Type)
		{
		case EProgramType::eAppInstallation:	ID.WriteEx(API_V_REG_KEY, TO_STR(m_AuxValue)); break;
		case EProgramType::eWindowsService:		ID.WriteEx(API_V_SERVICE_TAG, TO_STR(m_ServiceTag)); break;
		case EProgramType::eProgramGroup:		ID.WriteEx(API_V_GUID, TO_STR(m_AuxValue)); break;
		case EProgramType::eAppPackage:			ID.WriteEx(API_V_APP_SID, TO_STR(m_AuxValue)); break;
		}
	} 
	else 
	{ 
		ID.BeginMap();

		ID.WriteEx(API_S_PROG_TYPE, TypeToStr(m_Type));
		if (m_Type != EProgramType::eAllPrograms && m_Type != EProgramType::eProgramGroup)
			ID.WriteEx(API_S_FILE_PATH, TO_STR(m_FilePath));

		switch (m_Type)
		{
		case EProgramType::eAppInstallation:	ID.WriteEx(API_S_REG_KEY, TO_STR(m_AuxValue)); break;
		case EProgramType::eWindowsService:		ID.WriteEx(API_S_SERVICE_TAG, TO_STR(m_ServiceTag)); break;
		case EProgramType::eProgramGroup:		ID.WriteEx(API_S_GUID, TO_STR(m_AuxValue)); break;
		case EProgramType::eAppPackage:			ID.WriteEx(API_S_APP_SID, TO_STR(m_AuxValue)); break;
		}
	}
	return ID.Finish();
}

bool CProgramID::FromVariant(const class XVariant& _ID)
{
	VariantReader ID(_ID);
	SVarWriteOpt::EFormat Format;
	m_Type = ReadType(_ID, Format); // todo pass reader !! ZZZZZZZZZZZZZZ

	if (m_Type != EProgramType::eAllPrograms && m_Type != EProgramType::eProgramGroup)
		m_FilePath = AS_STR(Format == SVarWriteOpt::eMap ? ID.Find(API_S_FILE_PATH) : ID.Find(API_V_FILE_PATH));

	switch (m_Type)
	{
	case EProgramType::eAppInstallation:	m_AuxValue = AS_STR(Format == SVarWriteOpt::eMap ? ID.Find(API_S_REG_KEY) : ID.Find(API_V_REG_KEY)); break;
	case EProgramType::eWindowsService:		m_ServiceTag = AS_STR(Format == SVarWriteOpt::eMap ? ID.Find(API_S_SERVICE_TAG) : ID.Find(API_V_SERVICE_TAG)); break;
	case EProgramType::eProgramGroup:		m_AuxValue = AS_STR(Format == SVarWriteOpt::eMap ? ID.Find(API_S_GUID) : ID.Find(API_V_GUID)); break;
	case EProgramType::eAppPackage:			m_AuxValue = AS_STR(Format == SVarWriteOpt::eMap ? ID.Find(API_S_APP_SID) : ID.Find(API_V_APP_SID)); break;
	}

	return true;
}