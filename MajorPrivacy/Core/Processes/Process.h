#pragma once

#include "../Programs/ProgramId.h"
#include "../Access/Handle.h"
#include "../Network/Socket.h"
#include "../Programs/ImageSignInfo.h"
#include "./Common/QtFlexGuid.h"

class CProcess : public QObject
{
	Q_OBJECT
	TRACK_OBJECT(CProcess)
public:
	CProcess(quint32 Pid, QObject* parent = NULL);

	static QIcon DefaultIcon();

	quint64 GetProcessId() const { return m_Pid; }
	quint64 GetCreationTime() const { return m_CreationTime; }
	quint64 GetParentId() const { return m_ParentPid; }
	QString GetName() const { QReadLocker Lock(&m_Mutex); return m_Name; }
	QString GetNtPath() const { QReadLocker Lock(&m_Mutex); return m_NtFileName; }
	QString GetCmdLine() const { QReadLocker Lock(&m_Mutex); return m_CmdLine; }

	quint32 GetStatus() const { return m_Status; }
	quint32 GetSecState() const { return m_SecState; }
	quint32 GetFlags() const { return m_Flags; }
	quint32 GetSecFlags() const { return m_SecFlags; }

	QIcon GetIcon() const { QReadLocker Lock(&m_Mutex); return m_Icon; }
	bool IsProtected(bool bAlsoLite = false) const;
	QFlexGuid GetEnclaveGuid() const { QReadLocker Lock(&m_Mutex); return m_EnclaveGuid; }
	QSharedPointer<class CEnclave> GetEnclave() const { QReadLocker Lock(&m_Mutex); return m_pEnclave; }
	void SetEnclave(const QSharedPointer<class CEnclave>& pEnclave) { QWriteLocker Lock(&m_Mutex); m_pEnclave = pEnclave; }

	const CImageSignInfo& GetSignInfo() const { QReadLocker Lock(&m_Mutex); return m_SignInfo; }

	static QString GetSecStateStr(uint32 SecState);

	QString GetStatusStr() const;
	QString GetImgStats() const;

	QList<QString> GetServices() const { QReadLocker Lock(&m_Mutex); return m_ServiceList; }
	QString GetAppContainerSid() const { QReadLocker Lock(&m_Mutex); return m_AppContainerSid; }

	QString GetUserSid() const { QReadLocker Lock(&m_Mutex); return m_UserSid; }
	QString GetUserName() const { QReadLocker Lock(&m_Mutex); return m_UserName; }

	QList<CHandlePtr> GetHandles() const { QReadLocker Lock(&m_Mutex); return m_Handles.values(); }
	int GetHandleCount() const { QReadLocker Lock(&m_Mutex); return m_Handles.count(); }

	QList<CSocketPtr> GetSockets() const { QReadLocker Lock(&m_Mutex); return m_Sockets.values(); }
	int GetSocketCount() const { QReadLocker Lock(&m_Mutex); return m_Sockets.count(); }

	void FromVariant(const class QtVariant& Process);

private slots:
	void OnSidResolved(const QByteArray& SID, const QString& Name);

protected:

	void UpdateHandlesUnsafe(const class QtVariant& Handles);

	void UpdateSocketsUnsafe(const class QtVariant& Sockets);

	mutable QReadWriteLock m_Mutex;

	quint64 m_Pid = 0;
	quint64 m_CreationTime = 0;
	quint64 m_ParentPid = 0;
	QString m_Name;
	QString m_NtFileName;
	QIcon m_Icon;
	QString m_CmdLine;
	quint32 m_Status = 0;
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