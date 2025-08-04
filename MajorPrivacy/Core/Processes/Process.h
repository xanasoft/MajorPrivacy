#pragma once

#include "../Programs/ProgramId.h"
#include "../Access/Handle.h"
#include "../Network/Socket.h"
#include "../Programs/ImageSignInfo.h"
#include "./Common/QtFlexGuid.h"

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
	QString GetNtPath() const { return m_NtFileName; }
	QString GetCmdLine() const { return m_CmdLine; }

	quint32 GetSecState() const { return m_SecState; }
	quint32 GetFlags() const { return m_Flags; }
	quint32 GetSecFlags() const { return m_SecFlags; }

	QIcon GetIcon() const { return m_Icon; }
	bool IsProtected(bool bAlsoLite = false) const;
	QFlexGuid GetEnclaveGuid() const { return m_EnclaveGuid; }
	QSharedPointer<class CEnclave> GetEnclave() const { return m_pEnclave; }
	void SetEnclave(const QSharedPointer<class CEnclave>& pEnclave) { m_pEnclave = pEnclave; }

	const CImageSignInfo& GetSignInfo() const { return m_SignInfo; }

	static QString GetSecStateStr(uint32 SecState);

	QString GetStatus() const;
	QString GetImgStats() const;

	QList<QString> GetServices() const { return m_ServiceList; }
	QString GetAppContainerSid() const { return m_AppContainerSid; }

	QString GetUserSid() const { return m_UserSid; }
	QString GetUserName() const { return m_UserName; }

	QList<CHandlePtr> GetHandles() const { return m_Handles.values(); }
	int GetHandleCount() const { return m_Handles.count(); }

	QList<CSocketPtr> GetSockets() const { return m_Sockets.values(); }
	int GetSocketCount() const { return m_Sockets.count(); }

	void FromVariant(const class QtVariant& Process);

private slots:
	void OnSidResolved(const QByteArray& SID, const QString& Name);

protected:

	void UpdateHandles(const class QtVariant& Handles);

	void UpdateSockets(const class QtVariant& Sockets);

	quint64 m_Pid = 0;
	quint64 m_CreationTime = 0;
	quint64 m_ParentPid = 0;
	QString m_Name;
	QString m_NtFileName;
	QIcon m_Icon;
	QString m_CmdLine;
	quint32 m_SecState = 0;
	quint32 m_Flags = 0;
	quint32 m_SecFlags = 0;
	QFlexGuid m_EnclaveGuid;
	QSharedPointer<class CEnclave> m_pEnclave;
	CImageSignInfo m_SignInfo;
	quint32 m_NumberOfImageLoads = 0;
	quint32 m_NumberOfMicrosoftImageLoads = 0;
	quint32 m_NumberOfAntimalwareImageLoads = 0;
	quint32 m_NumberOfVerifiedImageLoads = 0;
	quint32 m_NumberOfSignedImageLoads = 0;
	quint32 m_NumberOfUntrustedImageLoads = 0;


	QList<QString> m_ServiceList;

	QString m_AppContainerSid;
	QString m_AppContainerName;
	QString m_AppPackageName;

	QString m_UserSid;
	QString m_UserName;

	QHash<quint64, CHandlePtr> m_Handles;

	QHash<quint64, CSocketPtr> m_Sockets;
};

typedef QSharedPointer<CProcess> CProcessPtr;