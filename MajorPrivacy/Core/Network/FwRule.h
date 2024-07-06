#pragma once
#include "../GenericRule.h"
#include "../../Service\Network\Firewall\FirewallDefs.h"

class CFwRule : public CGenericRule
{
	Q_OBJECT
public:
	CFwRule(const CProgramID& ID, QObject* parent = NULL);

    int GetIndex() const                    {return m_Index; }

    QString GetBinaryPath() const           {return m_BinaryPath; }
    QString GetServiceTag() const           {return m_ServiceTag; }
    QString GetAppContainerSid() const      {return m_AppContainerSid; }

    QString GetProgram() const;

    QString GetGrouping() const				{ return m_Grouping; }

    EFwActions GetAction() const            { return m_Action; }
    QString GetActionStr() const            { return ActionToStr(m_Action); }
    void SetAction(EFwActions Action)       { m_Action = Action; }
    EFwDirections GetDirection() const      { return m_Direction; }
    QString GetDirectionStr() const         { return DirectionToStr(m_Direction); }
    void SetDirection(EFwDirections Direction) { m_Direction = Direction; }
    void SetProfile(int Profile)            { m_Profile = Profile; }
    int GetProfile() const                  { return m_Profile; }
    QString GetProfileStr() const;

    void SetProtocol(EFwKnownProtocols Protocol) { m_Protocol = Protocol; }
    EFwKnownProtocols GetProtocol() const   { return m_Protocol; }
    void SetInterface(int Interface)        { m_Interface = Interface; }
    int GetInterface() const                { return m_Interface; }
    QString GetInterfaceStr() const;

    QStringList GetLocalAddresses() const   { return m_LocalAddresses; }
    QStringList GetLocalPorts() const       { return m_LocalPorts; }
    QStringList GetRemoteAddresses() const  { return m_RemoteAddresses; }
    QStringList GetRemotePorts() const      { return m_RemotePorts; }
    QStringList GetIcmpTypesAndCodes() const { return m_IcmpTypesAndCodes; }

    QStringList GetOsPlatformValidity() const { return m_OsPlatformValidity; }

    int GetEdgeTraversal() const            { return m_EdgeTraversal; }

    CFwRule* Clone() const;

    static QString StateToStr(EFwEventStates State);
    static QString ActionToStr(EFwActions Action);
    static QString DirectionToStr(EFwDirections Direction);
    static QString ProtocolToStr(EFwKnownProtocols Protocol);

protected:
    friend class CFirewallRuleWnd;

    void WriteIVariant(XVariant& Rule, const SVarWriteOpt& Opts) const override;
    void WriteMVariant(XVariant& Rule, const SVarWriteOpt& Opts) const override;
    void ReadIValue(uint32 Index, const XVariant& Data) override;
    void ReadMValue(const SVarName& Name, const XVariant& Data) override;

    // Note: m_Guid usually this is a guid but some default windows rules use a string name instead
    int m_Index = 0; // this is only used for sorting by newest rules

    QString m_BinaryPath;
    QString m_ServiceTag;
    QString m_AppContainerSid;
    QString m_LocalUserOwner;
    QString m_PackageFamilyName;

    QString m_Grouping;

    EFwActions m_Action;
    EFwDirections m_Direction;
    int m_Profile = (int)EFwProfiles::All;

    EFwKnownProtocols m_Protocol = EFwKnownProtocols::Any;
    int m_Interface = 0;

    QStringList m_LocalAddresses;
    QStringList m_LocalPorts;
    QStringList m_RemoteAddresses;
    QStringList m_RemotePorts;
    QStringList m_IcmpTypesAndCodes;

    QStringList m_OsPlatformValidity;

    int m_EdgeTraversal = 0;
};

typedef QSharedPointer<CFwRule> CFwRulePtr;