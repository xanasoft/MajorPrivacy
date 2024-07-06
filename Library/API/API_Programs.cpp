

/////////////////////////////////////////////////////////////////////////////
// Index Serializer
//

void CProgramLibrary::WriteIVariant(XVariant& Entry, const SVarWriteOpt& Opts) const
{
	Entry.Write(API_V_PROG_UID, m_UID);
	Entry.Write(API_V_FILE_PATH, GET_PATH(m_Path));
}

void CProgramLibrary::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_PROG_UID: m_UID = Data.To<uint64>(); break;
	case API_V_FILE_PATH: SET_PATH(m_Path, Data); break;
	default: break;
	}
}

// CProgramItem

void CProgramItem::WriteIVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	Data.Write(API_V_PROG_UID, m_UID);
	Data.Write(API_V_PROG_TYPE, (uint32)GetType());

	Data.WriteVariant(API_V_PROG_ID, m_ID.ToVariant(Opts));

	Data.Write(API_V_NAME, TO_STR(m_Name));
	Data.Write(API_V_ICON, TO_STR(m_IconFile));
	Data.Write(API_V_INFO, TO_STR(m_Info));

#ifdef PROG_SVC
	Data.WriteVariant(API_V_FW_RULES, CollectFwRules());
	Data.WriteVariant(API_V_PROGRAM_RULES, CollectProgRules());
	Data.WriteVariant(API_V_ACCESS_RULES, CollectResRules());
#endif
}

void CProgramItem::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_PROG_UID: break; // dont read UID

	case API_V_PROG_ID:			m_ID.FromVariant(Data); break;

	case API_V_NAME:			m_Name = AS_STR(Data); break;
	case API_V_ICON:			m_IconFile = AS_STR(Data); break;
	case API_V_INFO:			m_Info = AS_STR(Data); break;

#ifdef PROG_GUI
	case API_V_FW_RULES:		m_FwRuleIDs = Data.AsQSet(); break;
	case API_V_PROGRAM_RULES:	m_ProgRuleIDs = Data.AsQSet(); break;
	case API_V_ACCESS_RULES:	m_ResRuleIDs = Data.AsQSet(); break;
#else
	case API_V_FW_RULES: break;
	case API_V_PROGRAM_RULES: break;
	case API_V_ACCESS_RULES: break;
#endif

	default: break;
	}
}

// CWindowsService

void CWindowsService::WriteIVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CProgramItem::WriteIVariant(Data, Opts);

	Data.Write(API_V_SVC_TAG, TO_STR(m_ServiceId));

#ifdef PROG_SVC
	Data.Write(API_V_PID, m_pProcess ? m_pProcess->GetProcessId() : 0);

	SStats Stats;
	CollectStats(Stats);

	Data.Write(API_V_PROG_SOCKETS, Stats.SocketRefs);

	Data.Write(API_V_SOCK_LAST_ACT, Stats.LastActivity);
	Data.Write(API_V_SOCK_UPLOAD, Stats.Upload);
	Data.Write(API_V_SOCK_DOWNLOAD, Stats.Download);
	Data.Write(API_V_SOCK_UPLOADED, Stats.Uploaded);
	Data.Write(API_V_SOCK_DOWNLOADED, Stats.Downloaded);
#endif
}

void CWindowsService::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_SVC_TAG:				m_ServiceId = AS_STR(Data); break;

#ifdef PROG_GUI
	case API_V_PID:					m_ProcessId = Data; break;

	case API_V_PROG_SOCKETS:		m_SocketRefs = Data.AsQSet<quint64>(); break;

	case API_V_SOCK_LAST_ACT:		m_Stats.LastActivity = Data; break;

	case API_V_SOCK_UPLOAD:			m_Stats.Upload = Data; break;
	case API_V_SOCK_DOWNLOAD:		m_Stats.Download = Data; break;
	case API_V_SOCK_UPLOADED:		m_Stats.Uploaded = Data; break;
	case API_V_SOCK_DOWNLOADED:		m_Stats.Downloaded = Data; break;
#else
	case API_V_PID: break;

	case API_V_PROG_SOCKETS: break;

	case API_V_SOCK_LAST_ACT: break;

	case API_V_SOCK_UPLOAD: break;
	case API_V_SOCK_DOWNLOAD: break;
	case API_V_SOCK_UPLOADED: break;
	case API_V_SOCK_DOWNLOADED: break;
#endif
	default: CProgramItem::ReadIValue(Index, Data);
	}
}

// CProgramSet

void CProgramSet::WriteIVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CProgramItem::WriteIVariant(Data, Opts);

	XVariant Items;
	Items.BeginList();
	for (auto pItem : m_Nodes) {
		//Items.Write(pItem->ToVariant());
		XVariant Item;
		Item[API_V_PROG_UID] = pItem->GetUID();
		Item[API_V_PROG_ID] = pItem->GetID().ToVariant(Opts);	
		Items.WriteVariant(Item);
	}
	Items.Finish();
	Data.WriteVariant(API_V_PROG_ITEMS, Items);
}

void CProgramSet::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index) {
	case API_V_PROG_ITEMS: break;
	default: CProgramItem::ReadIValue(Index, Data);
	}
}

// CProgramPattern

void CProgramPattern::WriteIVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CProgramList::WriteIVariant(Data, Opts);

	Data.Write(API_V_PROG_PATTERN, GET_PATH(m_Pattern));
}

void CProgramPattern::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_PROG_PATTERN: SET_PATH(m_Pattern,Data); break;
	default: CProgramList::ReadIValue(Index, Data);
	}
}

// CProgramFile

void CProgramFile::WriteIVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CProgramSet::WriteIVariant(Data, Opts);

	Data.Write(API_V_FILE_PATH, GET_PATH(m_Path));

	Data.Write(API_V_SIGN_INFO, m_SignInfo.Data);

#ifdef PROG_SVC
	SStats Stats;
	CollectStats(Stats);

	Data.Write(API_V_PROG_PROC_PIDS, Stats.Pids);

	Data.Write(API_V_PROG_SOCKETS, Stats.SocketRefs);

	Data.Write(API_V_SOCK_LAST_ACT, Stats.LastActivity);
	Data.Write(API_V_SOCK_UPLOAD, Stats.Upload);
	Data.Write(API_V_SOCK_DOWNLOAD, Stats.Download);
	Data.Write(API_V_SOCK_UPLOADED, Stats.Uploaded);
	Data.Write(API_V_SOCK_DOWNLOADED, Stats.Downloaded);
#endif
}

void CProgramFile::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_FILE_PATH:		SET_PATH(m_Path, Data); break;

	case API_V_SIGN_INFO:		m_SignInfo.Data = Data.To<uint64>(); break;

#ifdef PROG_GUI
	case API_V_PROG_PROC_PIDS:	m_ProcessPids = Data.AsQSet<quint64>(); break;

	case API_V_PROG_SOCKETS:	m_SocketRefs = Data.AsQSet<quint64>(); break;

	case API_V_SOCK_LAST_ACT:	m_Stats.LastActivity = Data; break;

	case API_V_SOCK_UPLOAD:		m_Stats.Upload = Data; break;
	case API_V_SOCK_DOWNLOAD:	m_Stats.Download = Data; break;
	case API_V_SOCK_UPLOADED:	m_Stats.Uploaded = Data; break;
	case API_V_SOCK_DOWNLOADED:	m_Stats.Downloaded = Data; break;
#else
	case API_V_PROG_PROC_PIDS: break;

	case API_V_PROG_SOCKETS: break;

	case API_V_SOCK_LAST_ACT: break;

	case API_V_SOCK_UPLOAD: break;
	case API_V_SOCK_DOWNLOAD: break;
	case API_V_SOCK_UPLOADED: break;
	case API_V_SOCK_DOWNLOADED: break;
#endif
	default: CProgramSet::ReadIValue(Index, Data);
	}
}

// CAppPackage

void CAppPackage::WriteIVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CProgramList::WriteIVariant(Data, Opts);

	Data.Write(API_V_APP_SID, TO_STR(m_AppContainerSid));
	Data.Write(API_V_APP_NAME, TO_STR(m_AppContainerName));
	Data.Write(API_V_FILE_PATH, GET_PATH(m_Path));
}

void CAppPackage::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_APP_SID: m_AppContainerSid = AS_STR(Data); break;
	case API_V_APP_NAME: m_AppContainerName = AS_STR(Data); break;
	case API_V_FILE_PATH: SET_PATH(m_Path, Data); break;
	default: CProgramList::ReadIValue(Index, Data);
	}
}

// SAppInstallation

void CAppInstallation::WriteIVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CProgramPattern::WriteIVariant(Data, Opts);

	Data.Write(API_V_REG_KEY, TO_STR(m_RegKey));
	Data.Write(API_V_FILE_PATH, GET_PATH(m_Path));
}

void CAppInstallation::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index) {
	case API_V_REG_KEY: m_RegKey = AS_STR(Data); break;
	case API_V_FILE_PATH: SET_PATH(m_Path, Data); break;
	default: CProgramPattern::ReadIValue(Index, Data);
	}
}

/////////////////////////////////////////////////////////////////////////////
// Map Serializer
//

void CProgramLibrary::WriteMVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	Data.Write(API_S_PROG_UID, m_UID);
	Data.Write(API_S_FILE_PATH, GET_PATH(m_Path));
}

void CProgramLibrary::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_PROG_UID))				m_UID = Data.To<uint64>();
	else if (VAR_TEST_NAME(Name, API_S_FILE_PATH))			SET_PATH(m_Path, Data);
}

// CProgramItem

void CProgramItem::WriteMVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	Data.Write(API_S_PROG_UID, m_UID);
	Data.Write(API_S_PROG_TYPE, CProgramID::TypeToStr(GetType()));

	Data.WriteVariant(API_S_PROG_ID, m_ID.ToVariant(Opts));

	Data.Write(API_S_NAME, TO_STR(m_Name));
	Data.Write(API_S_ICON, TO_STR(m_IconFile));
	Data.Write(API_S_INFO, TO_STR(m_Info));

#ifdef PROG_SVC
	Data.WriteVariant(API_S_FW_RULES, CollectFwRules());
	Data.WriteVariant(API_S_PROGRAM_RULES, CollectProgRules());
	Data.WriteVariant(API_S_ACCESS_RULES, CollectResRules());
#endif
}

void CProgramItem::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_PROG_UID))				; // dont read UID
	else if (VAR_TEST_NAME(Name, API_S_PROG_ID))			m_ID.FromVariant(Data);

	else if (VAR_TEST_NAME(Name, API_S_NAME))				m_Name = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_ICON))				m_IconFile = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_INFO))				m_Info = AS_STR(Data);

#ifdef PROG_GUI
	else if (VAR_TEST_NAME(Name, API_S_FW_RULES))			m_FwRuleIDs = Data.AsQSet();
	else if (VAR_TEST_NAME(Name, API_S_PROGRAM_RULES))		m_ProgRuleIDs = Data.AsQSet();
	else if (VAR_TEST_NAME(Name, API_S_ACCESS_RULES))		m_ResRuleIDs = Data.AsQSet();

#else
	else if (VAR_TEST_NAME(Name, API_S_FW_RULES))			(void)0;
	else if (VAR_TEST_NAME(Name, API_S_PROGRAM_RULES))		(void)0;
	else if (VAR_TEST_NAME(Name, API_S_ACCESS_RULES))		(void)0;
#endif

	// else unknown tag
}

// CWindowsService

void CWindowsService::WriteMVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CProgramItem::WriteMVariant(Data, Opts);

	Data.Write(API_S_SVC_TAG, TO_STR(m_ServiceId));

#ifdef PROG_SVC
	Data.Write(API_S_PID, m_pProcess ? m_pProcess->GetProcessId() : 0);

	SStats Stats;
	CollectStats(Stats);

	Data.Write(API_S_PROG_SOCKETS, Stats.SocketRefs);

	Data.Write(API_S_SOCK_LAST_ACT, Stats.LastActivity);
	Data.Write(API_S_SOCK_UPLOAD, Stats.Upload);
	Data.Write(API_S_SOCK_DOWNLOAD, Stats.Download);
	Data.Write(API_S_SOCK_UPLOADED, Stats.Uploaded);
	Data.Write(API_S_SOCK_DOWNLOADED, Stats.Downloaded);
#endif
}

void CWindowsService::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_SVC_TAG)) m_ServiceId = AS_STR(Data);

#ifdef PROG_GUI
	else if (VAR_TEST_NAME(Name, API_S_PID))				m_ProcessId = Data;

	else if (VAR_TEST_NAME(Name, API_S_PROG_SOCKETS))		m_SocketRefs = Data.AsQSet<quint64>();

	else if (VAR_TEST_NAME(Name, API_S_SOCK_LAST_ACT))		m_Stats.LastActivity = Data;

	else if (VAR_TEST_NAME(Name, API_S_SOCK_UPLOAD))		m_Stats.Upload = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_DOWNLOAD))		m_Stats.Download = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_UPLOADED))		m_Stats.Uploaded = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_DOWNLOADED))	m_Stats.Downloaded = Data;
#else
	else if (VAR_TEST_NAME(Name, API_S_PID))				(void)0;

	else if (VAR_TEST_NAME(Name, API_S_PROG_SOCKETS))		(void)0;

	else if (VAR_TEST_NAME(Name, API_S_SOCK_LAST_ACT))		(void)0;

	else if (VAR_TEST_NAME(Name, API_S_SOCK_UPLOAD))		(void)0;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_DOWNLOAD))		(void)0;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_UPLOADED))		(void)0;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_DOWNLOADED))	(void)0;
#endif

	else
		CProgramItem::ReadMValue(Name, Data);
}

// CProgramSet

void CProgramSet::WriteMVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CProgramItem::WriteMVariant(Data, Opts);

	XVariant Items;
	Items.BeginList();
	for (auto pItem : m_Nodes) {
		//Items.Write(pItem->ToVariant());
		XVariant Item;
		Item[API_S_PROG_UID] = pItem->GetUID();
		Item[API_S_PROG_ID] = pItem->GetID().ToVariant(Opts);	
		Items.WriteVariant(Item);
	}
	Items.Finish();
	Data.WriteVariant(API_S_PROG_ITEMS, Items);
}

void CProgramSet::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_PROG_ITEMS)) 
		(void)0;
	else
		CProgramItem::ReadMValue(Name, Data);
}

// CProgramPattern

void CProgramPattern::WriteMVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CProgramList::WriteMVariant(Data, Opts);

	Data.Write(API_S_PROG_PATTERN, GET_PATH(m_Pattern));
}

void CProgramPattern::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_PROG_PATTERN)) SET_PATH(m_Pattern, Data);
	else
		CProgramList::ReadMValue(Name, Data);
}

// CProgramFile

void CProgramFile::WriteMVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CProgramSet::WriteMVariant(Data, Opts);

	Data.Write(API_S_FILE_PATH, GET_PATH(m_Path));

	Data.Write(API_S_SIGN_INFO, m_SignInfo.Data);

#ifdef PROG_SVC
	SStats Stats;
	CollectStats(Stats);

	Data.Write(API_S_PIDS, Stats.Pids);

	Data.Write(API_S_PROG_SOCKETS, Stats.SocketRefs);

	Data.Write(API_S_SOCK_LAST_ACT, Stats.LastActivity);
	Data.Write(API_S_SOCK_UPLOAD, Stats.Upload);
	Data.Write(API_S_SOCK_DOWNLOAD, Stats.Download);
	Data.Write(API_S_SOCK_UPLOADED, Stats.Uploaded);
	Data.Write(API_S_SOCK_DOWNLOADED, Stats.Downloaded);
#endif
}

void CProgramFile::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_FILE_PATH)) SET_PATH(m_Path, Data);

	else if (VAR_TEST_NAME(Name, API_S_SIGN_INFO))			m_SignInfo.Data = Data.To<uint64>();

#ifdef PROG_GUI
	else if (VAR_TEST_NAME(Name, API_S_PIDS))				m_ProcessPids = Data.AsQSet<quint64>();

	else if (VAR_TEST_NAME(Name, API_S_PROG_SOCKETS))		m_SocketRefs = Data.AsQSet<quint64>();

	else if (VAR_TEST_NAME(Name, API_S_SOCK_LAST_ACT))		m_Stats.LastActivity = Data;

	else if (VAR_TEST_NAME(Name, API_S_SOCK_UPLOAD))		m_Stats.Upload = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_DOWNLOAD))		m_Stats.Download = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_UPLOADED))		m_Stats.Uploaded = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_DOWNLOADED))	m_Stats.Downloaded = Data;
#else
	else if (VAR_TEST_NAME(Name, API_S_PIDS))				(void)0;

	else if (VAR_TEST_NAME(Name, API_S_PROG_SOCKETS))		(void)0;

	else if (VAR_TEST_NAME(Name, API_S_SOCK_LAST_ACT))		(void)0;

	else if (VAR_TEST_NAME(Name, API_S_SOCK_UPLOAD))		(void)0;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_DOWNLOAD))		(void)0;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_UPLOADED))		(void)0;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_DOWNLOADED))	(void)0;
#endif
	else
		CProgramSet::ReadMValue(Name, Data);
}

// CAppPackage

void CAppPackage::WriteMVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CProgramList::WriteMVariant(Data, Opts);

	Data.Write(API_S_APP_SID, TO_STR(m_AppContainerSid));
	Data.Write(API_S_APP_NAME, TO_STR(m_AppContainerName));
	Data.Write(API_S_FILE_PATH, GET_PATH(m_Path));
}

void CAppPackage::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_APP_SID))			m_AppContainerSid = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_APP_NAME))	m_AppContainerName = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_FILE_PATH))	SET_PATH(m_Path, Data);
	else
		CProgramList::ReadMValue(Name, Data);
}

// SAppInstallation

void CAppInstallation::WriteMVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CProgramPattern::WriteMVariant(Data, Opts);

	Data.Write(API_S_REG_KEY, TO_STR(m_RegKey));
	Data.Write(API_S_FILE_PATH, GET_PATH(m_Path));
}

void CAppInstallation::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_REG_KEY))			m_RegKey = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_FILE_PATH))	SET_PATH(m_Path, Data);
	else
		CProgramPattern::ReadMValue(Name, Data);
}

