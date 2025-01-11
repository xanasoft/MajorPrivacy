#ifdef CFwRule
#define M(x) m_Data->##x

#else
#define M(x) m_##x
#endif


/////////////////////////////////////////////////////////////////////////////
// CFwRule
//

void CFwRule::WriteIVariant(XVariant& Rule, const SVarWriteOpt& Opts) const
{
#ifndef CFwRule
    CGenericRule::WriteIVariant(Rule, Opts);
#endif

#ifdef CFwRule
    Rule.Write(API_V_GUID, M(Guid));
#endif
    Rule.Write(API_V_INDEX, M(Index));

#ifdef CFwRule
    Rule.Write(API_V_ENABLED, m_Data->Enabled);
    //Rule.Write(API_V_TEMP, ...
#endif

    Rule.Write(API_V_FILE_PATH, TO_STR(m_BinaryPath));
    Rule.Write(API_V_SERVICE_TAG, TO_STR(M(ServiceTag)));
    Rule.Write(API_V_APP_SID, TO_STR(M(AppContainerSid)));
    Rule.Write(API_V_OWNER, TO_STR(M(LocalUserOwner)));
    Rule.Write(API_V_APP_NAME, TO_STR(M(PackageFamilyName)));

#ifdef CFwRule
    Rule.WriteVariant(API_V_PROG_ID, m_ProgramID.ToVariant(Opts));

    Rule.Write(API_V_NAME, TO_STR(M(Name)));
#endif
    Rule.Write(API_V_RULE_GROUP, TO_STR(M(Grouping)));
#ifdef CFwRule
    Rule.Write(API_V_RULE_DESCR, TO_STR(M(Description)));
#endif

    Rule.Write(API_V_FW_RULE_ACTION, (uint32)M(Action));
    Rule.Write(API_V_FW_RULE_DIRECTION, (uint32)M(Direction));
    Rule.Write(API_V_FW_RULE_PROFILE, (uint32)M(Profile));

    Rule.Write(API_V_FW_RULE_PROTOCOL, (uint32)M(Protocol));

    Rule.Write(API_V_FW_RULE_INTERFACE, (uint32)M(Interface));

    Rule.WriteVariant(API_V_FW_RULE_LOCAL_ADDR, XVariant(M(LocalAddresses)));
    Rule.WriteVariant(API_V_FW_RULE_LOCAL_PORT, XVariant(M(LocalPorts)));
    Rule.WriteVariant(API_V_FW_RULE_REMOTE_ADDR, XVariant(M(RemoteAddresses)));
    Rule.WriteVariant(API_V_FW_RULE_REMOTE_PORT, XVariant(M(RemotePorts)));

#ifdef CFwRule
    std::vector<std::string> IcmpTypesAndCodes;
    for (auto I : m_Data->IcmpTypesAndCodes)
        IcmpTypesAndCodes.push_back(std::to_string(I.Type) + ":" + (I.Code == 256 ? "*" : std::to_string(I.Code)));
    Rule.WriteVariant(API_V_FW_RULE_ICMP, CVariant(IcmpTypesAndCodes));
#else
    Rule.WriteVariant(API_V_FW_RULE_ICMP, XVariant(M(IcmpTypesAndCodes)));
#endif

    Rule.WriteVariant(API_V_FW_RULE_OS, XVariant(M(OsPlatformValidity)));

    Rule.Write(API_V_FW_RULE_EDGE, M(EdgeTraversal));

#ifdef CFwRule
	Rule.Write(API_V_TEMP, m_bTemporary);
	Rule.Write(API_V_TIMEOUT, m_uTimeOut);
#endif

    Rule.Write(API_V_RULE_HIT_COUNT, m_HitCount);
    
}

void CFwRule::ReadIValue(uint32 Index, const XVariant& Data)
{
    switch (Index)
    {
#ifdef CFwRule
    case API_V_GUID: M(Guid) = AS_STR(Data); break;
#endif
    case API_V_INDEX: M(Index) = Data.To<int>(); break;

#ifdef CFwRule
    case API_V_ENABLED: M(Enabled) = Data.To<bool>(); break;
    //case API_V_TEMP: ...
#endif

    case API_V_FILE_PATH: m_BinaryPath = AS_STR(Data); break;
    case API_V_SERVICE_TAG: M(ServiceTag) = AS_STR(Data); break;
    case API_V_APP_SID: M(AppContainerSid) = AS_STR(Data); break;
    case API_V_OWNER: M(LocalUserOwner) = AS_STR(Data); break;
    case API_V_APP_NAME: M(PackageFamilyName) = AS_STR(Data); break;

#ifdef CFwRule
    case API_V_PROG_ID: break; // set from actual data
    case API_V_NAME: M(Name) = AS_STR(Data); break;
#endif
    case API_V_RULE_GROUP: M(Grouping) = AS_STR(Data); break;
#ifdef CFwRule
    case API_V_RULE_DESCR: M(Description) = AS_STR(Data); break;
#endif

    case API_V_FW_RULE_ACTION: M(Action) = (EFwActions)Data.To<uint32>(); break;
    case API_V_FW_RULE_DIRECTION: M(Direction) = (EFwDirections)Data.To<uint32>(); break;
    case API_V_FW_RULE_PROFILE: M(Profile) = (int)Data.To<uint32>(); break;

    case API_V_FW_RULE_PROTOCOL: M(Protocol) = (EFwKnownProtocols)Data.To<uint32>(); break;

    case API_V_FW_RULE_INTERFACE: M(Interface) = (int)Data.To<uint32>(); break;

    case API_V_FW_RULE_LOCAL_ADDR: M(LocalAddresses) = AS_LIST(Data); break;
    case API_V_FW_RULE_LOCAL_PORT: M(LocalPorts) = AS_LIST(Data); break;
    case API_V_FW_RULE_REMOTE_ADDR: M(RemoteAddresses) = AS_LIST(Data); break;
    case API_V_FW_RULE_REMOTE_PORT: M(RemotePorts) = AS_LIST(Data); break;

#ifdef CFwRule
    case API_V_FW_RULE_ICMP:
    {
        std::vector<std::string> IcmpTypesAndCodes = Data.AsList<std::string>();
        for (auto Icmp : IcmpTypesAndCodes)
        {
            auto TypeCode = Split2(Icmp, ":");
            SWindowsFwRule::IcmpTypeCode IcmpTypeCode;
            IcmpTypeCode.Type = std::atoi(TypeCode.first.c_str());
            IcmpTypeCode.Code = TypeCode.second == "*" ? 256 : std::atoi(TypeCode.second.c_str());
            m_Data->IcmpTypesAndCodes.push_back(IcmpTypeCode);
        }
        break;
    }
#else
    case API_V_FW_RULE_ICMP: M(IcmpTypesAndCodes) = AS_LIST(Data); break;
#endif

    case API_V_FW_RULE_OS: M(OsPlatformValidity) = AS_LIST(Data); break;

    case API_V_FW_RULE_EDGE: M(EdgeTraversal) = Data.To<int>(); break;

#ifdef CFwRule
	case API_V_TEMP: m_bTemporary = Data.To<bool>(); break;
	case API_V_TIMEOUT: m_uTimeOut = Data.To<uint64>(); break;
#endif

	case API_V_RULE_HIT_COUNT: m_HitCount = Data.To<int>(); break;

#ifndef CFwRule
    default: CGenericRule::ReadIValue(Index, Data);
#endif
    }
}

/////////////////////////////////////////////////////////////////////////////

void CFwRule::WriteMVariant(XVariant& Rule, const SVarWriteOpt& Opts) const
{
#ifndef CFwRule
    CGenericRule::WriteMVariant(Rule, Opts);
#endif

#ifdef CFwRule
    Rule.Write(API_S_GUID, M(Guid));
#endif
    Rule.Write(API_S_INDEX, M(Index));

#ifdef CFwRule
    Rule.Write(API_S_ENABLED, M(Enabled));
    //Rule.Write(API_S_TEMP, ...
#endif

    Rule.Write(API_S_FILE_PATH, TO_STR(m_BinaryPath));
    Rule.Write(API_S_SERVICE_TAG, TO_STR(M(ServiceTag)));
    Rule.Write(API_S_APP_SID, TO_STR(M(AppContainerSid)));
    Rule.Write(API_S_OWNER, TO_STR(M(LocalUserOwner)));
    Rule.Write(API_S_APP_NAME, TO_STR(M(PackageFamilyName)));

#ifdef CFwRule
    Rule.WriteVariant(API_S_PROG_ID, m_ProgramID.ToVariant(Opts));

    Rule.Write(API_S_NAME, TO_STR(M(Name)));
#endif
    Rule.Write(API_S_RULE_GROUP, TO_STR(M(Grouping)));
#ifdef CFwRule
    Rule.Write(API_S_RULE_DESCR, TO_STR(M(Description)));
#endif

    switch (M(Action)) {
    case EFwActions::Allow: Rule.Write(API_S_FW_RULE_ACTION, API_S_FW_RULE_ACTION_ALLOW); break;
    case EFwActions::Block: Rule.Write(API_S_FW_RULE_ACTION, API_S_FW_RULE_ACTION_BLOCK); break;
    }

    switch (M(Direction)) {
    case EFwDirections::Bidirectional:   Rule.Write(API_S_FW_RULE_DIRECTION, API_S_FW_RULE_DIRECTION_BIDIRECTIONAL); break;
    case EFwDirections::Inbound:	    Rule.Write(API_S_FW_RULE_DIRECTION, API_S_FW_RULE_DIRECTION_INBOUND); break;
    case EFwDirections::Outbound:	    Rule.Write(API_S_FW_RULE_DIRECTION, API_S_FW_RULE_DIRECTION_OUTBOUND); break;
    }

    std::vector<std::string> Profiles;
    if (M(Profile) == (uint32)EFwProfiles::All)
        Profiles.push_back(API_S_FW_RULE_PROFILE);
    else
    {
        if(M(Profile) & (int)EFwProfiles::Domain)	Profiles.push_back(API_S_FW_RULE_PROFILE_DOMAIN);
        if(M(Profile) & (int)EFwProfiles::Private)	Profiles.push_back(API_S_FW_RULE_PROFILE_PRIVATE);
        if(M(Profile) & (int)EFwProfiles::Public)	Profiles.push_back(API_S_FW_RULE_PROFILE_PUBLIC);
    }
    Rule.Write(API_S_FW_RULE_PROFILE, Profiles);

    Rule.Write(API_S_FW_RULE_PROTOCOL, (uint32)M(Protocol));

    std::vector<std::string> Interfaces;
    if (M(Interface) == (uint32)EFwInterfaces::All)
        Interfaces.push_back(API_S_FW_RULE_INTERFACE_ANY);
    else
    {
        if(M(Interface) & (int)EFwInterfaces::Lan)			Interfaces.push_back(API_S_FW_RULE_INTERFACE_LAN);
        if(M(Interface) & (int)EFwInterfaces::Wireless)	    Interfaces.push_back(API_S_FW_RULE_INTERFACE_WIRELESS);
        if(M(Interface) & (int)EFwInterfaces::RemoteAccess) Interfaces.push_back(API_S_FW_RULE_INTERFACE_REMOTEACCESS);
    }
    Rule.Write(API_S_FW_RULE_INTERFACE, Interfaces);

    Rule.WriteVariant(API_S_FW_RULE_LOCAL_ADDR, XVariant(M(LocalAddresses)));
    Rule.WriteVariant(API_S_FW_RULE_LOCAL_PORT, XVariant(M(LocalPorts)));
    Rule.WriteVariant(API_S_FW_RULE_REMOTE_ADDR, XVariant(M(RemoteAddresses)));
    Rule.WriteVariant(API_S_FW_RULE_LOCAL_PORT, XVariant(M(RemotePorts)));

#ifdef CFwRule
    std::vector<std::string> IcmpTypesAndCodes;
    for (auto I : m_Data->IcmpTypesAndCodes)
        IcmpTypesAndCodes.push_back(std::to_string(I.Type) + ":" + (I.Code == 256 ? "*" : std::to_string(I.Code)));
    Rule.Write(API_S_FW_RULE_ICMP, IcmpTypesAndCodes);
#else
    Rule.WriteVariant(API_S_FW_RULE_ICMP, XVariant(M(IcmpTypesAndCodes)));
#endif

    Rule.WriteVariant(API_S_FW_RULE_OS, XVariant(M(OsPlatformValidity)));

    Rule.Write(API_S_FW_RULE_EDGE, M(EdgeTraversal));

#ifdef CFwRule
    Rule.Write(API_S_TEMP, m_bTemporary);
    Rule.Write(API_S_TIMEOUT, m_uTimeOut);
#endif

	Rule.Write(API_S_RULE_HIT_COUNT, m_HitCount);
}

void CFwRule::ReadMValue(const SVarName& Name, const XVariant& Data)
{
#ifdef CFwRule
    if (VAR_TEST_NAME(Name, API_S_GUID))
        M(Guid) = AS_STR(Data);
    else
#endif
    if (VAR_TEST_NAME(Name, API_S_INDEX))
        M(Index) = Data.To<int>();

#ifdef CFwRule
    else if (VAR_TEST_NAME(Name, API_S_ENABLED))
        M(Enabled) = Data.To<bool>();
    //else if (VAR_TEST_NAME(Name, API_S_TEMP))
    //    ... = Data.To<bool>();
#endif

    else if (VAR_TEST_NAME(Name, API_S_FILE_PATH))
        m_BinaryPath = AS_STR(Data);
    else if (VAR_TEST_NAME(Name, API_S_SERVICE_TAG))
        M(ServiceTag) = AS_STR(Data);
    else if (VAR_TEST_NAME(Name, API_S_APP_SID))
        M(AppContainerSid) = AS_STR(Data);
    else if (VAR_TEST_NAME(Name, API_S_OWNER))
        M(LocalUserOwner) = AS_STR(Data);
    else if (VAR_TEST_NAME(Name, API_S_APP_NAME))
        M(PackageFamilyName) = AS_STR(Data);

#ifdef CFwRule
    else if (VAR_TEST_NAME(Name, API_S_PROG_ID))
        ; // set from actual data
    else if (VAR_TEST_NAME(Name, API_S_NAME))
        M(Name) = AS_STR(Data);
#endif
    else if (VAR_TEST_NAME(Name, API_S_RULE_GROUP))
        M(Grouping) = AS_STR(Data);
#ifdef CFwRule
    else if (VAR_TEST_NAME(Name, API_S_RULE_DESCR))
        M(Description) = AS_STR(Data);
#endif

    else if (VAR_TEST_NAME(Name, API_S_FW_RULE_ACTION))
	{
		ASTR Action = Data;
		if (Action == API_S_FW_RULE_ACTION_ALLOW)
			M(Action) = EFwActions::Allow;
		else if (Action == API_S_FW_RULE_ACTION_BLOCK)
			M(Action) = EFwActions::Block;
	}

	else if (VAR_TEST_NAME(Name, API_S_FW_RULE_DIRECTION))
	{
		ASTR Direction = Data;
		if (Direction == API_S_FW_RULE_DIRECTION_BIDIRECTIONAL)
			M(Direction) = EFwDirections::Bidirectional;
		else if (Direction == API_S_FW_RULE_DIRECTION_INBOUND)
			M(Direction) = EFwDirections::Inbound;
		else if (Direction == API_S_FW_RULE_DIRECTION_OUTBOUND)
			M(Direction) = EFwDirections::Outbound;
	}

	else if (VAR_TEST_NAME(Name, API_S_FW_RULE_PROFILE))
	{
        auto Profiles = AS_ALIST(Data);
        if (IS_EMPTY(Profiles) || (Profiles.size() == 1 && (Profiles[0] == API_S_FW_RULE_PROFILE_ALL || IS_EMPTY(Profiles[0]))))
            M(Profile) = (int)EFwProfiles::All;
        else {
            M(Profile) = 0;
            for (auto Profile : Profiles)
            {
                if (Profile == API_S_FW_RULE_PROFILE_DOMAIN)
                    M(Profile) |= (int)EFwProfiles::Domain;
                else if (Profile == API_S_FW_RULE_PROFILE_PRIVATE)
                    M(Profile) |= (int)EFwProfiles::Private;
                else if (Profile == API_S_FW_RULE_PROFILE_PUBLIC)
                    M(Profile) |= (int)EFwProfiles::Public;
            }
        }
	}

	else if (VAR_TEST_NAME(Name, API_S_FW_RULE_PROTOCOL))
		M(Protocol) = (EFwKnownProtocols)Data.To<uint32>();

	else if (VAR_TEST_NAME(Name, API_S_FW_RULE_INTERFACE))
	{
        auto Interfaces = AS_ALIST(Data);
        if(IS_EMPTY(Interfaces) || (Interfaces.size() == 1 && (Interfaces[0] == API_S_FW_RULE_INTERFACE_ANY || IS_EMPTY(Interfaces[0]))))
		    M(Interface) = 0;
        else {
            for (auto Interface : Interfaces)
            {
                if (Interface == API_S_FW_RULE_INTERFACE_LAN)
                    M(Interface) |= (int)EFwInterfaces::Lan;
                else if (Interface == API_S_FW_RULE_INTERFACE_WIRELESS)
                    M(Interface) |= (int)EFwInterfaces::Wireless;
                else if (Interface == API_S_FW_RULE_INTERFACE_REMOTEACCESS)
                    M(Interface) |= (int)EFwInterfaces::RemoteAccess;
            }
        }
	}

	else if (VAR_TEST_NAME(Name, API_S_FW_RULE_LOCAL_ADDR))
		M(LocalAddresses) = AS_LIST(Data);
    else if (VAR_TEST_NAME(Name, API_S_FW_RULE_LOCAL_PORT))
        M(LocalPorts) = AS_LIST(Data);
	else if (VAR_TEST_NAME(Name, API_S_FW_RULE_REMOTE_ADDR))
        M(RemoteAddresses) = AS_LIST(Data);
	else if (VAR_TEST_NAME(Name, API_S_FW_RULE_LOCAL_PORT))
		M(RemotePorts) = AS_LIST(Data);

#ifdef CFwRule
    else if (VAR_TEST_NAME(Name, API_S_FW_RULE_ICMP))
    {
        std::vector<std::string> IcmpTypesAndCodes = Data.AsList<std::string>();
        for (auto Icmp : IcmpTypesAndCodes)
        {
            auto TypeCode = Split2(Icmp, ":");
            SWindowsFwRule::IcmpTypeCode IcmpTypeCode;
            IcmpTypeCode.Type = std::atoi(TypeCode.first.c_str());
            IcmpTypeCode.Code = TypeCode.second == "*" ? 256 : std::atoi(TypeCode.second.c_str());
            m_Data->IcmpTypesAndCodes.push_back(IcmpTypeCode);
        }
    }
#else
    else if (VAR_TEST_NAME(Name, API_S_FW_RULE_ICMP))
	    M(IcmpTypesAndCodes) = AS_LIST(Data);
#endif

    else if (VAR_TEST_NAME(Name, API_S_FW_RULE_OS))
		M(OsPlatformValidity) = AS_LIST(Data);

	else if (VAR_TEST_NAME(Name, API_S_FW_RULE_EDGE))
		M(EdgeTraversal) = Data.To<int>();

#ifdef CFwRule
	else if (VAR_TEST_NAME(Name, API_S_TEMP))
		m_bTemporary = Data.To<bool>();
	else if (VAR_TEST_NAME(Name, API_S_TIMEOUT))
		m_uTimeOut = Data.To<uint64>();
#endif

	else if (VAR_TEST_NAME(Name, API_S_RULE_HIT_COUNT))
		m_HitCount = Data.To<int>();

#ifndef CFwRule
    else
        CGenericRule::ReadMValue(Name, Data);
#endif
}