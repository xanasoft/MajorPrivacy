#include "pch.h"
#include "FwRule.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NtUtil.h"

CFwRule::CFwRule(const CProgramID& ID, QObject* parent)
	: CGenericRule(ID, parent)
{
    m_BinaryPath = ID.GetFilePath();
    m_ServiceTag = ID.GetServiceTag();
    m_AppContainerSid = ID.GetAppContainerSid();
}

QString CFwRule::GetSourceStr() const
{
    switch (GetSource()) {
    case EFwRuleSource::eWindowsDefault: return tr("Windows Default");
	case EFwRuleSource::eWindowsStore:   return tr("Windows Store");
    case EFwRuleSource::eMajorPrivacy:   return tr("Major Privacy");
    case EFwRuleSource::eAutoTemplate:   return tr("Privacy Template");
    default:                             return "";
	}
}

QString CFwRule::GetStateStr() const
{
    switch (GetState()) {
    case EFwRuleState::eUnapprovedDisabled:
    case EFwRuleState::eUnapproved: return tr("Unapproved");
    case EFwRuleState::eApproved:   return tr("Approved");
    case EFwRuleState::eBackup:     return tr("Backup");
	case EFwRuleState::eDiverged:   return tr("Diverged");
	//case EFwRuleState::eDeleted:    return tr("Deleted");
    }
    return tr("Unknown");
}

QColor CFwRule::GetStateColor() const
{
    switch (GetState()) {
    case EFwRuleState::eUnapprovedDisabled:
    case EFwRuleState::eUnapproved: return QColor(255, 255, 128); // Yellow
    case EFwRuleState::eApproved:   return QColor(128, 255, 128); // Green
    case EFwRuleState::eBackup:     return QColor(255, 128, 128); // Red
	case EFwRuleState::eDiverged:   return QColor(255, 192, 128);   // Orange
	//case EFwRuleState::eDeleted:    return QColor(128, 128, 128); // Gray
    }
	return QColor(0, 0, 0); // Black for unknown state
}

void CFwRule::SetTemporary(bool bTemporary)
{
    m_Grouping = bTemporary ? "#Temporary" : "";
    CGenericRule::SetTemporary(bTemporary);
}

QString CFwRule::GetProgram() const
{
    if (!m_AppContainerSid.isEmpty())
        return m_AppContainerSid;
    if (!m_ServiceTag.isEmpty())
        return m_ServiceTag;
    return m_BinaryPath;
}

CFwRule* CFwRule::Clone(bool CloneGuid) const
{
	CFwRule* pRule = new CFwRule(m_ProgramID);
    CopyTo(pRule, CloneGuid);
    if (CloneGuid) {
        pRule->m_Index = m_Index;
        pRule->m_State = m_State;
        pRule->m_Source = m_Source;
    }
    else {
        pRule->m_State = EFwRuleState::eApproved;
        pRule->m_Source = EFwRuleSource::eMajorPrivacy;
    }

	pRule->m_BinaryPath = m_BinaryPath;
	pRule->m_ServiceTag = m_ServiceTag;
	pRule->m_AppContainerSid = m_AppContainerSid;
    pRule->m_LocalUserOwner = m_LocalUserOwner;
    pRule->m_PackageFamilyName = m_PackageFamilyName;

    pRule->m_Grouping = m_Grouping;

	pRule->m_Action = m_Action;
	pRule->m_Direction = m_Direction;
	pRule->m_Profile = m_Profile;

	pRule->m_Protocol = m_Protocol;
	pRule->m_Interface = m_Interface;
	pRule->m_LocalAddresses = m_LocalAddresses;
	pRule->m_LocalPorts = m_LocalPorts;
	pRule->m_RemoteAddresses = m_RemoteAddresses;
	pRule->m_RemotePorts = m_RemotePorts;
	pRule->m_IcmpTypesAndCodes = m_IcmpTypesAndCodes;

	pRule->m_OsPlatformValidity = m_OsPlatformValidity;

	pRule->m_EdgeTraversal = m_EdgeTraversal;

	return pRule;
}

QString CFwRule::GetProfileStr() const
{
    QStringList Profiles;
    if (m_Profile == (int)EFwProfiles::All)
        Profiles.append(tr("All"));
    else
    {
        if(m_Profile & (int)EFwProfiles::Domain)	Profiles.append(tr("Domain"));
        if(m_Profile & (int)EFwProfiles::Private)	Profiles.append(tr("Private"));
        if(m_Profile & (int)EFwProfiles::Public)	Profiles.append(tr("Public"));
    }
    return Profiles.join(", ");
}

QString CFwRule::GetInterfaceStr() const
{
    QStringList Interfaces;
    if (m_Interface == (int)EFwInterfaces::All)
        Interfaces.append(tr("Any"));
    else
    {
        if(m_Interface & (int)EFwInterfaces::Lan)			Interfaces.append(tr("LAN"));
        if(m_Interface & (int)EFwInterfaces::Wireless)	    Interfaces.append(tr("WiFi"));
        if(m_Interface & (int)EFwInterfaces::RemoteAccess)  Interfaces.append(tr("VPN"));
    }
    return Interfaces.join(", ");
}

QString CFwRule::StateToStr(EFwEventStates State) 
{ 
    switch (State)
    {
    case EFwEventStates::FromLog: return tr("FromLog");
    case EFwEventStates::UnRuled: return tr("UnRuled");
    case EFwEventStates::RuleAllowed: return tr("Rule Allowed");
    case EFwEventStates::RuleBlocked: return tr("Rule Blocked");
    case EFwEventStates::RuleError: return tr("Rule Error");
    case EFwEventStates::RuleGenerated: return tr("Rule Generated");
    default: return tr("Undefined");
    }
}

QString CFwRule::ActionToStr(EFwActions Action) 
{ 
    switch (Action)
    {
    case EFwActions::Allow: return tr("Allow");
    case EFwActions::Block: return tr("Block");
    default: return tr("Undefined");
    }
}

QString CFwRule::DirectionToStr(EFwDirections Direction) 
{ 
    switch (Direction)
    {
    case EFwDirections::Inbound: return tr("Inbound");
    case EFwDirections::Outbound: return tr("Outbound");
    case EFwDirections::Bidirectional: return tr("Bidirectional");
    default: return tr("Unknown");
    }
}

QString CFwRule::ProtocolToStr(EFwKnownProtocols Protocol)
{
    /*switch (Protocol)
    {
    case EFwKnownProtocols::Any: return tr("Any");
    case EFwKnownProtocols::TCP: return tr("TCP");
    case EFwKnownProtocols::UDP: return tr("UDP");
    case EFwKnownProtocols::ICMP: return tr("ICMP");
    default: return tr("%1").arg((int)Protocol);
    }*/

    /*switch ((int)Protocol)
    {
    case 0: return "HOPOPT (IPv6 Hop-by-Hop Option)";
    case 1: return "ICMP (Internet Control Message Protocol)";
    case 2: return "IGMP (Internet Group Management Protocol)";
    case 3: return "GGP (Gateway-to-Gateway)";
    case 4: return "IP (IP in IP (encapsulation))";
    case 5: return "Stream";
    case 6: return "TCP (Transmission Control Protocol)";
    case 7: return "CBT (Core Based Trees)";
    case 8: return "EGP (Exterior Gateway Protocol)";
    case 9: return "IGP (any private interior gateway)";
    case 10: return "BBN-RCC-MON (BBN RCC Monitoring)";
    case 11: return "NVP-II (Network Voice Protocol)";
    case 12: return "PUP";
    case 13: return "ARGUS";
    case 14: return "EMCON";
    case 15: return "XNET (Cross Net Debugger)";
    case 16: return "CHAOS";
    case 17: return "UDP (User Datagram Protocol)";
    case 18: return "Multiplexing";
    case 19: return "DCN-MEAS (DCN Measurement Subsystems)";
    case 20: return "HMP (Host Monitoring)";
    case 21: return "PRM (Packet Radio Measurement)";
    case 22: return "XNS-IDP (XEROX NS IDP)";
    case 23: return "TRUNK-1";
    case 24: return "TRUNK-2";
    case 25: return "LEAF-1";
    case 26: return "LEAF-2";
    case 27: return "RDP (Reliable Data Protocol)";
    case 28: return "IRTP (Internet Reliable Transaction Protocol)";
    case 29: return "ISO-TP4 (ISO Transport Protocol Class 4)";
    case 30: return "NETBLT (Bulk Data Transfer Protocol)";
    case 31: return "MFE-NSP (MFE Network Services Protocol)";
    case 32: return "MERIT-INP (MERIT Internodal Protocol)";
    case 33: return "DCCP (Datagram Congestion Control Protocol)";
    case 34: return "3PC (Third Party Connect Protocol)";
    case 35: return "IDPR (Inter-Domain Policy Routing Protocol)";
    case 36: return "XTP";
    case 37: return "DDP (Datagram Delivery Protocol)";
    case 38: return "IDPR-CMTP (IDPR Control Message Transport Proto)";
    case 39: return "TP++ (TP++ Transport Protocol)";
    case 40: return "IL (IL Transport Protocol)";
    case 41: return "Verkapselung von IPv6- in IPv4-Pakete";
    case 42: return "SDRP (Source Demand Routing Protocol)";
    case 43: return "IPv6-Route (Routing Header for IPv6)";
    case 44: return "IPv6-Frag (Fragment Header for IPv6)";
    case 45: return "IDRP (Inter-Domain Routing Protocol)";
    case 46: return "RSVP (Reservation Protocol)";
    case 47: return "GRE (Generic Routing Encapsulation)";
    case 48: return "MHRP (Mobile Host Routing Protocol)";
    case 49: return "BNA";
    case 50: return "ESP (Encapsulating Security Payload)";
    case 51: return "AH (Authentication Header)";
    case 52: return "I-NLSP (Integrated Net Layer Security TUBA)";
    case 53: return "SWIPE (IP with Encryption)";
    case 54: return "NARP (NBMA Address Resolution Protocol)";
    case 55: return "MOBILE (IP Mobility)";
    case 56: return "TLSP (Transport Layer Security Protocol)";
    case 57: return "SKIP";
    case 58: return "IPv6-ICMP (ICMP for IPv6)";
    case 59: return "IPv6-NoNxt (Kein nächster Header für IPv6)";
    case 60: return "IPv6-Opts (Destination Options for IPv6)";
    case 61: return "Jedes Host-interne Protokoll";
    case 62: return "CFTP";
    case 63: return "Jedes lokale Netz";
    case 64: return "SAT-EXPAK (SATNET and Backroom EXPAK)";
    case 65: return "KRYPTOLAN";
    case 66: return "RVD (MIT Remote Virtual Disk Protocol)";
    case 67: return "IPPC (Internet Pluribus Packet Core)";
    case 68: return "Jedes verteilte Dateisystem";
    case 69: return "SAT-MON (SATNET Monitoring)";
    case 70: return "VISA";
    case 71: return "IPCV (Internet Packet Core Utility)";
    case 72: return "CPNX (Computer Protocol Network Executive)";
    case 73: return "CPHB (Computer Protocol Heart Beat)";
    case 74: return "WSN (Wang Span Network)";
    case 75: return "PVP (Packet Video Protocol)";
    case 76: return "BR-SAT-MON (Backroom SATNET Monitoring)";
    case 77: return "SUN-ND (SUN ND PROTOCOL-Temporary)";
    case 78: return "WB-MON (WIDEBAND Monitoring)";
    case 79: return "WB-EXPAK (WIDEBAND EXPAK)";
    case 80: return "ISO-IP (ISO Internet Protocol)";
    case 81: return "VMTP";
    case 82: return "SECURE-VMTP";
    case 83: return "VINES";
    case 84: return "TTP";
    case 85: return "NSFNET-IGP (NSFNET-IGP)";
    case 86: return "DGP (Dissimilar Gateway Protocol)";
    case 87: return "TCF";
    case 88: return "EIGRP";
    case 89: return "OSPF";
    case 90: return "Sprite-RPC (Sprite RPC Protocol)";
    case 91: return "LARP (Locus Address Resolution Protocol)";
    case 92: return "MTP (Multicast Transport Protocol)";
    case 93: return "AX.25 (AX.25 Frames)";
    case 94: return "IPIP (IP-within-IP Encapsulation Protocol)";
    case 95: return "MICP (Mobile Internetworking Control Pro.)";
    case 96: return "SCC-SP (Semaphore Communications Sec. Pro.)";
    case 97: return "ETHERIP (Ethernet-within-IP Encapsulation)";
    case 98: return "ENCAP (Encapsulation Header)";
    case 99: return "Jeder private Verschlüsselungsentwurf";
    case 100: return "GMTP";
    case 101: return "IFMP (Ipsilon Flow Management Protocol)";
    case 102: return "PNNI (over IP)";
    case 103: return "PIM (Protocol Independent Multicast)";
    case 104: return "ARIS";
    case 105: return "SCPS";
    case 106: return "QNX";
    case 107: return "A/N (Active Networks)";
    case 108: return "IPComp (IP Payload Compression Protocol)";
    case 109: return "SNP (Sitara Networks Protocol)";
    case 110: return "Compaq-Peer (Compaq Peer Protocol)";
    case 111: return "IPX-in-IP (IPX in IP)";
    case 112: return "VRRP (Virtual Router Redundancy Protocol)";
    case 113: return "PGM (PGM Reliable Transport Protocol)";
    case 114: return "any 0-hop protocol";
    case 115: return "L2TP (Layer Two Tunneling Protocol)";
    case 116: return "DDX (D-II Data Exchange (DDX))";
    case 117: return "IATP (Interactive Agent Transfer Protocol)";
    case 118: return "STP (Schedule Transfer Protocol)";
    case 119: return "SRP (SpectraLink Radio Protocol)";
    case 120: return "UTI";
    case 121: return "SMP (Simple Message Protocol)";
    case 122: return "SM";
    case 123: return "PTP (Performance Transparency Protocol)";
    case 124: return "ISIS over IPv4";
    case 125: return "FIRE";
    case 126: return "CRTP (Combat Radio Transport Protocol)";
    case 127: return "CRUDP (Combat Radio User Datagram)";
    case 128: return "SSCOPMCE";
    case 129: return "IPLT";
    case 130: return "SPS (Secure Packet Shield)";
    case 131: return "PIPE (Private IP Encapsulation within IP)";
    case 132: return "SCTP (Stream Control Transmission Protocol)";
    case 133: return "FC (Fibre Channel)";
    case 134: return "RSVP-E2E-IGNORE";
    case 135: return "Mobility Header";
    case 136: return "UDPLite";
    case 137: return "MPLS-in-IP";
    case 138: return "manet (MANET Protocols)";
    case 139: return "HIP (Host Identity Protocol)";
    case 140: return "Shim6 (Shim6 Protocol)";
    case 141: return "WESP (Wrapped Encapsulating Security Payload)";
    case 142: return "ROHC (Robust Header Compression)";
    default: return tr("#%1").arg((int)Protocol);
    }*/

    switch ((int)Protocol)
    {
    case 0: return "HOPOPT";
    case 1: return "ICMP";
    case 2: return "IGMP";
    case 3: return "GGP";
    case 4: return "IP";
    //case 5: return "Stream";
    case 6: return "TCP";
    case 7: return "CBT";
    case 8: return "EGP";
    case 9: return "IGP";
    case 10: return "BBN-RCC-MON";
    case 11: return "NVP-II";
    case 12: return "PUP";
    case 13: return "ARGUS";
    case 14: return "EMCON";
    case 15: return "XNET";
    case 16: return "CHAOS";
    case 17: return "UDP";
     //case 18: return "Multiplexing";
    case 19: return "DCN-MEAS";
    case 20: return "HMP";
    case 21: return "PRM";
    case 22: return "XNS-IDP";
    case 23: return "TRUNK-1";
    case 24: return "TRUNK-2";
    case 25: return "LEAF-1";
    case 26: return "LEAF-2";
    case 27: return "RDP";
    case 28: return "IRTP";
    case 29: return "ISO-TP4";
    case 30: return "NETBLT";
    case 31: return "MFE-NSP";
    case 32: return "MERIT-INP";
    case 33: return "DCCP";
    case 34: return "3PC";
    case 35: return "IDPR";
    case 36: return "XTP";
    case 37: return "DDP";
    case 38: return "IDPR-CMTP";
    case 39: return "TP++";
    case 40: return "IL";
     ///case 41: return "Verkapselung von IPv6- in IPv4-Pakete";
    case 42: return "SDRP";
    case 43: return "IPv6-Route";
    case 44: return "IPv6-Frag";
    case 45: return "IDRP";
    case 46: return "RSVP";
    case 47: return "GRE";
    case 48: return "MHRP";
    case 49: return "BNA";
    case 50: return "ESP";
    case 51: return "AH";
    case 52: return "I-NLSP";
    case 53: return "SWIPE";
    case 54: return "NARP";
    case 55: return "MOBILE";
    case 56: return "TLSP";
    case 57: return "SKIP";
    case 58: return "IPv6-ICMP";
    case 59: return "IPv6-NoNxt";
    case 60: return "IPv6-Opts";
    //case 61: return "Jedes Host-interne Protokoll";
    case 62: return "CFTP";
    //case 63: return "Jedes lokale Netz";
    case 64: return "SAT-EXPAK";
    case 65: return "KRYPTOLAN";
    case 66: return "RVD";
    case 67: return "IPPC";
    //case 68: return "Jedes verteilte Dateisystem";
    case 69: return "SAT-MON";
    case 70: return "VISA";
    case 71: return "IPCV";
    case 72: return "CPNX";
    case 73: return "CPHB";
    case 74: return "WSN";
    case 75: return "PVP";
    case 76: return "BR-SAT-MON";
    case 77: return "SUN-ND";
    case 78: return "WB-MON";
    case 79: return "WB-EXPAK";
    case 80: return "ISO-IP";
    case 81: return "VMTP";
    case 82: return "SECURE-VMTP";
    case 83: return "VINES";
    case 84: return "TTP";
    case 85: return "NSFNET-IGP";
    case 86: return "DGP";
    case 87: return "TCF";
    case 88: return "EIGRP";
    case 89: return "OSPF";
    case 90: return "Sprite-RPC";
    case 91: return "LARP";
    case 92: return "MTP";
    case 93: return "AX.25";
    case 94: return "IPIP";
    case 95: return "MICP";
    case 96: return "SCC-SP";
    case 97: return "ETHERIP";
    case 98: return "ENCAP";
    //case 99: return "Jeder private Verschlüsselungsentwurf";
    case 100: return "GMTP";
    case 101: return "IFMP";
    case 102: return "PNNI";
    case 103: return "PIM";
    case 104: return "ARIS";
    case 105: return "SCPS";
    case 106: return "QNX";
    case 107: return "A/N";
    case 108: return "IPComp";
    case 109: return "SNP";
    case 110: return "Compaq-Peer";
    case 111: return "IPX-in-IP";
    case 112: return "VRRP";
    case 113: return "PGM";
    case 114: return "any 0-hop protocol";
    case 115: return "L2TP";
    case 116: return "DDX";
    case 117: return "IATP";
    case 118: return "STP";
    case 119: return "SRP";
    case 120: return "UTI";
    case 121: return "SMP";
    case 122: return "SM";
    case 123: return "PTP";
    case 124: return "ISIS";
    case 125: return "FIRE";
    case 126: return "CRTP";
    case 127: return "CRUDP";
    case 128: return "SSCOPMCE";
    case 129: return "IPLT";
    case 130: return "SPS";
    case 131: return "PIPE";
    case 132: return "SCTP";
    case 133: return "FC";
    case 134: return "RSVP-E2E-IGNORE";
    case 135: return "Mobility Header";
    case 136: return "UDPLite";
    case 137: return "MPLS-in-IP";
    case 138: return "manet";
    case 139: return "HIP";
    case 140: return "Shim6";
    case 141: return "WESP";
    case 142: return "ROHC";
    case 256: return tr("Any");
    default: return QString("#%1").arg((int)Protocol);
    }
}