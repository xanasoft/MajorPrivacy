#pragma once
#include "../Programs/ProgramID.h"

class CFwRule : public QObject
{
	Q_OBJECT
public:
	CFwRule(const CProgramID& ID, QObject* parent = NULL);

    QString GetGuid() const                 {return m_Guid; }
    int GetIndex() const                    {return m_Index; }


    QString GetBinaryPath() const           {return m_BinaryPath; }
    QString GetServiceTag() const           {return m_ServiceTag; }
    QString GetAppContainerSid() const      {return m_AppContainerSid; }

    const CProgramID& GetProgramID() const  { return m_ProgramID; }
    QString GetProgram() const;

    QString GetName() const                 { return m_Name; }
    void SetName(const QString& Name)       { m_Name = Name; }
    QString GetGrouping() const             { return m_Grouping; }
    QString GetDescription() const          { return m_Description; }

    void SetEnabled(bool Enabled)           { m_Enabled = Enabled; }
    bool IsEnabled() const                  { return m_Enabled; }
    QString GetAction() const               { return m_Action; }
    void SetAction(const QString& Action)   { m_Action = Action; }
    QString GetDirection() const            { return m_Direction; }
    void SetDirection(const QString& Direction) { m_Direction = Direction; }
    QStringList GetProfile() const          { return m_Profile; }

    quint32 GetProtocol() const             { return m_Protocol; }
    QStringList GetInterface() const        { return m_Interface; }
    QStringList GetLocalAddresses() const   { return m_LocalAddresses; }
    QStringList GetLocalPorts() const       { return m_LocalPorts; }
    QStringList GetRemoteAddresses() const  { return m_RemoteAddresses; }
    QStringList GetRemotePorts() const      { return m_RemotePorts; }
    QStringList GetIcmpTypesAndCodes() const { return m_IcmpTypesAndCodes; }

    QStringList GetOsPlatformValidity() const { return m_OsPlatformValidity; }

    int GetEdgeTraversal() const            { return m_EdgeTraversal; }

    CFwRule* Clone() const;

	XVariant ToVariant() const;
	void FromVariant(const class XVariant& FwRule);

protected:
    friend class CFirewallRuleWnd;

    QString m_Guid;     // Note: usually this is a guid but some default windows rules use a string name instead
    int m_Index = 0;    // this is only used for sorting by newest rules

    QString m_BinaryPath;
    QString m_ServiceTag;
    QString m_AppContainerSid;

    CProgramID m_ProgramID;
        
    QString m_Name;
    QString m_Grouping;
    QString m_Description;

    bool m_Enabled = false;
    QString m_Action;
    QString m_Direction;
    QStringList m_Profile;

    quint32 m_Protocol = 0;
    QStringList m_Interface;
    QStringList m_LocalAddresses;
    QStringList m_LocalPorts;
    QStringList m_RemoteAddresses;
    QStringList m_RemotePorts;
    QStringList m_IcmpTypesAndCodes;

    QStringList m_OsPlatformValidity;

    int m_EdgeTraversal = 0;
};

typedef QSharedPointer<CFwRule> CFwRulePtr;