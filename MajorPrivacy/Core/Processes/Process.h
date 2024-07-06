#pragma once

#include "../Programs/ProgramId.h"
#include "../Access/Handle.h"
#include "../Network/Socket.h"
#include "../../Helpers/FilePath.h"

class CProcess : public QObject
{
	Q_OBJECT
public:
	CProcess(quint32 Pid, QObject* parent = NULL);

	static QIcon DefaultIcon();

	quint64 GetProcessId() const { return m_Pid; }
	quint64 GetCreationTime() const { return m_CreationTime; }
	quint64 GetParentId() const { return m_ParentPid; }
	QString GetName() const { return m_Name; }
	QString GetPath(EPathType Type) const { return m_Path.Get(Type); }
	QString GetCmdLine() const { return m_CmdLine; }
	QIcon GetIcon() const { return m_Icon; }
	quint64 GetEnclaveId() const { return m_EnclaveId; }
	bool IsProtected(bool bAlsoLite = false) const;

	SLibraryInfo::USign GetSignInfo() const	{ return m_SignInfo; }

	QString GetStatus() const;
	QString GetImgStats() const;

	QList<QString> GetServices() const { return m_ServiceList; }
	QString GetAppContainerSid() const { return m_AppContainerSid; }

	QList<CHandlePtr> GetHandles() const { return m_Handles.values(); }
	int GetHandleCount() const { return m_Handles.count(); }

	QList<CSocketPtr> GetSockets() const { return m_Sockets.values(); }
	int GetSocketCount() const { return m_Sockets.count(); }

	void FromVariant(const class XVariant& Process);

protected:

	void UpdateHandles(const class XVariant& Handles);

	void UpdateSockets(const class XVariant& Sockets);

	quint64 m_Pid = 0;
	quint64 m_CreationTime = 0;
	quint64 m_ParentPid = 0;
	QString m_Name;
	CFilePath m_Path;
	QIcon m_Icon;
	QString m_CmdLine;
	//m_ImageHash;
	quint64 m_EnclaveId = 0;
	quint32 m_SecState = 0;
	quint32 m_Flags = 0;
	quint32 m_SecFlags = 0;
	SLibraryInfo::USign m_SignInfo = {0};
	quint32 m_NumberOfImageLoads = 0;
	quint32 m_NumberOfMicrosoftImageLoads = 0;
	quint32 m_NumberOfAntimalwareImageLoads = 0;
	quint32 m_NumberOfVerifiedImageLoads = 0;
	quint32 m_NumberOfSignedImageLoads = 0;
	quint32 m_NumberOfUntrustedImageLoads = 0;


	QList<QString> m_ServiceList;

	QString m_AppContainerSid;
	QString m_AppContainerName;
	QString m_PackageFullName;

	QMap<quint64, CHandlePtr> m_Handles;

	QMap<quint64, CSocketPtr> m_Sockets;
};

typedef QSharedPointer<CProcess> CProcessPtr;