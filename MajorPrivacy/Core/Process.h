#pragma once

#include "Programs/ProgramId.h"
#include "Network/Socket.h"

class CProcess : public QObject
{
	Q_OBJECT
public:
	CProcess(quint32 Pid, QObject* parent = NULL);

	QIcon DefaultIcon() const;

	quint64 GetProcessId() const { return m_Pid; }
	quint64 GetParentId() const { return m_ParentPid; }
	QString GetName() const { return m_Name; }
	QString GetFileName() const { return m_FileName; }
	QIcon GetIcon() const { return m_Icon; }
	quint32 GetEnclave() const { return m_EnclaveId; }

	QList<QString> GetServices() const { return m_ServiceList; }
	QString GetAppContainerSid() const { return m_AppContainerSid; }

	QList<CSocketPtr> GetSockets() const { return m_Sockets.values(); }
	int GetSocketCount() const { return m_Sockets.count(); }

	void FromVariant(const class XVariant& Process);

protected:

	void UpdateSockets(const class XVariant& Sockets);

	quint64 m_Pid = 0;
	quint64 m_CreationTime = 0;
	quint64 m_ParentPid = 0;
	QString m_Name;
	QString m_FileName;
	QIcon m_Icon;
	//m_ImageHash;
	quint32 m_EnclaveId = 0;

	QList<QString> m_ServiceList;

	QString m_AppContainerSid;
	QString m_AppContainerName;
	//QString m_PackageFullName;

	QMap<quint64, CSocketPtr> m_Sockets;
};

typedef QSharedPointer<CProcess> CProcessPtr;