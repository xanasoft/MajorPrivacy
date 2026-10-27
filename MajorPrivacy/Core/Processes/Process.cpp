#include "pch.h"
#include "Process.h"
#include "../PrivacyCore.h"
#include "./Common/QtVariant.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NtUtil.h"
#include "../../Helpers/WinHelper.h"
#include "../../Library/Helpers/SID.h"
#include "../Enclaves/EnclaveManager.h"


CProcess::CProcess(quint32 Pid, QObject* parent)
 : QObject(parent)
{
	m_Pid = Pid;
}

QIcon CProcess::DefaultIcon()
{
	static QIcon ExeIcon;
	if(ExeIcon.isNull())
	{
		ExeIcon.addFile(":/exe16.png");
		ExeIcon.addFile(":/exe32.png");
		ExeIcon.addFile(":/exe48.png");
		ExeIcon.addFile(":/exe64.png");
	}
	//return QIcon(":/Icons/Process.png");
	return ExeIcon;
}

bool CProcess::IsProtected(bool bAlsoLite) const 
{ 
	KPH_PROCESS_FLAGS kFlags;
	kFlags.Flags = m_Flags;

	KPH_PROCESS_SFLAGS kSFlags;
	kSFlags.SecFlags = m_SecFlags;

	return kFlags.Protected && (bAlsoLite || kSFlags.ProtectionLevel == KphFullProtection);
}

void CProcess::FromVariant(const class QtVariant& Process, bool bFullSend)
{
	QWriteLocker Lock(&m_Mutex);


	QtVariantReader(Process).ReadRawIndex([&](uint32 Index, const FW::CVariant& vData) {
		const QtVariant& Data = *(QtVariant*)&vData;

		switch (Index)
		{
		case API_V_PUID:			m_Puid = Data; break;
		case API_V_CREATE_TIME:		m_CreationTime = Data; break;
		case API_V_PARENT_PID:		m_ParentPid = Data; break;

		case API_V_NAME:			m_Name = Data.AsQStr(); break;
		case API_V_FILE_NT_PATH:
		{
			QString NtFileName = Data.AsQStr();
			if (m_NtFileName != NtFileName)
			{
				if (m_NtFileName.isEmpty() && !NtFileName.isEmpty()) {
					m_Icon = LoadWindowsIconEx(theCore->NormalizePath(NtFileName), 0);
					if (m_Icon.availableSizes().isEmpty())
						m_Icon = DefaultIcon();
				}
				m_NtFileName = NtFileName;
			}
			break;
		}

		case API_V_CMD_LINE:	m_CmdLine = Data.AsQStr(); break;

		case API_V_APP_SID:		m_AppContainerSid = Data.AsQStr(); break;
		case API_V_APP_NAME:	m_AppContainerName = Data.AsQStr(); break;
		case API_V_PACK_NAME:	m_AppPackageName = Data.AsQStr(); break;

		case API_V_STATUS:		m_Status = Data; break;

		case API_V_ENCLAVE:		m_EnclaveGuid.FromVariant(Data); break;
		case API_V_KPP_STATE:	m_SecState = Data; break;

		case API_V_USER_SID: {
			m_UserSid = Data.AsQStr(); 
			if (!m_UserSid.isEmpty() && m_UserName.isEmpty()) {
				SSid UserSid(m_UserSid.toStdString());
				m_UserName = theCore->GetSidResolver()->GetSidFullName(QByteArray((char*)UserSid.Value.data(), UserSid.Value.size()), this, SLOT(OnSidResolved(const QByteArray&, const QString&)));
			}
			break;
		}

		case API_V_FLAGS:		m_Flags = Data; break;
		case API_V_SFLAGS:		m_SecFlags = Data; break;

		case API_V_SIGN_INFO:	m_SignInfo.FromVariant(Data); break;
		
		case API_V_NUM_IMG:		m_NumberOfImageLoads = Data; break;
		case API_V_NUM_MS_IMG:	m_NumberOfMicrosoftImageLoads = Data; break;
		case API_V_NUM_AV_IMG:	m_NumberOfAntimalwareImageLoads = Data; break;
		case API_V_NUM_V_IMG:	m_NumberOfVerifiedImageLoads = Data; break;
		case API_V_NUM_S_IMG:	m_NumberOfSignedImageLoads = Data; break;
		case API_V_NUM_U_IMG:	m_NumberOfUntrustedImageLoads = Data; break;

		case API_V_SERVICES:	m_ServiceList = Data.AsQList(); break;

		case API_V_HANDLES:		UpdateHandlesUnsafe(Data, bFullSend); break;
		case API_V_OLD_HANDLES:	RemoveHandlesUnsafe(Data); break;

		case API_V_SOCKETS:		UpdateSocketsUnsafe(Data, bFullSend); break;
		case API_V_OLD_SOCKETS:	RemoveSocketsUnsafe(Data); break;

		}

	});
}

void CProcess::OnSidResolved(const QByteArray& SID, const QString& Name)
{
	QWriteLocker Lock(&m_Mutex);

	m_UserName = Name;
}

//
// On a complete list anything not mentioned is gone, and an already enlisted
// handle carries nothing new - its path and owner are fixed when it is opened -
// so there is no point reading it again.
//
// On a delta nothing is swept, because a handle that is absent is one that did
// not change; what went away is in API_V_OLD_HANDLES. And here an entry only
// arrived because something about it did change, so it is always read.
//
void CProcess::UpdateHandlesUnsafe(const class QtVariant& Handles, bool bFullSend)
{
	QHash<quint64, CHandlePtr> OldHandles;
	if (bFullSend)
		OldHandles = m_Handles;

	QtVariantReader(Handles).ReadRawList([&](const FW::CVariant& vData) {
		const QtVariant& Handle = *(QtVariant*)&vData;

		QtVariant vHandleRef(Handle.Allocator());
		FW::CVariantReader::Find(Handle, API_V_ACCESS_REF, vHandleRef);
		quint64 HandleRef = vHandleRef;

		CHandlePtr pHandle = m_Handles.value(HandleRef);
		if (pHandle) {
			OldHandles.remove(HandleRef);
			if (bFullSend)
				return; // already enlisted, and it cannot have changed
		}
		else {
			pHandle = CHandlePtr(new CHandle());
			m_Handles.insert(HandleRef, pHandle);
		} 

		pHandle->FromVariant(Handle);
	});

	foreach(quint64 HandleRef, OldHandles.keys())
		m_Handles.remove(HandleRef);
}

void CProcess::RemoveHandlesUnsafe(const class QtVariant& Handles)
{
	QtVariantReader(Handles).ReadRawList([&](const FW::CVariant& vData) {
		m_Handles.remove(((QtVariant*)&vData)->To<quint64>(0));
	});
}

//
// As above, except that a socket does carry live data - its state and its
// transfer rates - so an enlisted one is read again either way.
//
void CProcess::UpdateSocketsUnsafe(const class QtVariant& Sockets, bool bFullSend)
{
	QHash<quint64, CSocketPtr> OldSockets;
	if (bFullSend)
		OldSockets = m_Sockets;

	QtVariantReader(Sockets).ReadRawList([&](const FW::CVariant& vData) {
		const QtVariant& Socket = *(QtVariant*)&vData;

		QtVariant vSockRef(Socket.Allocator());
		FW::CVariantReader::Find(Socket, API_V_SOCK_REF, vSockRef);
		quint64 SockRef = vSockRef;

		CSocketPtr pSocket = m_Sockets.value(SockRef);
		if (pSocket)
			OldSockets.remove(SockRef);
		else {
			pSocket = CSocketPtr(new CSocket());
			m_Sockets.insert(SockRef, pSocket);
		} 
		
		pSocket->FromVariant(Socket);
	});

	foreach(quint64 SockRef, OldSockets.keys())
		m_Sockets.remove(SockRef);
}

void CProcess::RemoveSocketsUnsafe(const class QtVariant& Sockets)
{
	QtVariantReader(Sockets).ReadRawList([&](const FW::CVariant& vData) {
		m_Sockets.remove(((QtVariant*)&vData)->To<quint64>(0));
	});
}

QString CProcess::GetSecStateStr(uint32 SecState)
{
	if ((SecState & KPH_PROCESS_STATE_MAXIMUM) == KPH_PROCESS_STATE_MAXIMUM)	return "Maximum";
	if ((SecState & KPH_PROCESS_STATE_HIGH) == KPH_PROCESS_STATE_HIGH)			return "High";
	if ((SecState & KPH_PROCESS_STATE_MEDIUM) == KPH_PROCESS_STATE_MEDIUM)		return "Medium";
	if ((SecState & KPH_PROCESS_STATE_LOW) == KPH_PROCESS_STATE_LOW)			return "Low";
	if ((SecState & KPH_PROCESS_STATE_MINIMUM) == KPH_PROCESS_STATE_MINIMUM)	return "Minimum";
	return "Other";
}

QString CProcess::GetStatusStr() const
{
	QStringList Infos;

	KPH_PROCESS_FLAGS kFlags;
	kFlags.Flags = m_Flags;

	//KPH_PROCESS_SFLAGS kSFlags;
	//kSFlags.SecFlags = m_SecFlags;

	QStringList Flags;
	//if (kFlags.VerifiedProcess)		Flags.append("VerifiedProcess");
	//if (kFlags.SecurelyCreated)		Flags.append("SecurelyCreated");
	//if (kFlags.Protected)			Flags.append("Protected");
	//if (kFlags.IsWow64)				Flags.append("IsWow64");
	//if (kFlags.IsSubsystemProcess)	Flags.append("IsSubsystemProcess");
	if(!Flags.isEmpty())	Infos.append(Flags.join(", "));

	if (kFlags.Protected && m_SecState)
	{
		Infos.append(tr("Sec: %1").arg(GetSecStateStr(m_SecState)));

		QStringList Sec;
		if (m_SecState & KPH_PROCESS_SECURELY_CREATED)				Sec.append("Created");
		if (m_SecState & KPH_PROCESS_VERIFIED_PROCESS)				Sec.append("Verified");
		if (m_SecState & KPH_PROCESS_PROTECTED_PROCESS)				Sec.append("Protected");
		if (m_SecState & KPH_PROCESS_NO_UNTRUSTED_IMAGES)			Sec.append("NoUntrusted");
		if (m_SecState & KPH_PROCESS_HAS_FILE_OBJECT)				Sec.append("HasFile");
		if (m_SecState & KPH_PROCESS_HAS_SECTION_OBJECT_POINTERS)	Sec.append("HasSection");
		if (m_SecState & KPH_PROCESS_NO_USER_WRITABLE_REFERENCES)	Sec.append("NoWritableRefs");
		if (m_SecState & KPH_PROCESS_NO_WRITABLE_FILE_OBJECT)		Sec.append("NoWritableFile");
		if (m_SecState & KPH_PROCESS_NO_FILE_TRANSACTION)			Sec.append("NoFileTransaction");
		if (m_SecState & KPH_PROCESS_NOT_BEING_DEBUGGED)			Sec.append("NotBeingDebugged");
		Infos.append(Sec.join(", "));

	}

	return Infos.join("; ");
}

QString CProcess::GetImgStats() const
{
	QString Infos = tr("%1 (").arg(m_NumberOfImageLoads);
	if (!m_NumberOfAntimalwareImageLoads) Infos.append(tr("%1").arg(m_NumberOfMicrosoftImageLoads));
	else Infos.append(tr("%1+%2").arg(m_NumberOfMicrosoftImageLoads).arg(m_NumberOfAntimalwareImageLoads));
	Infos.append(tr("/%1/%2/%3)").arg(m_NumberOfVerifiedImageLoads).arg(m_NumberOfSignedImageLoads).arg(m_NumberOfUntrustedImageLoads));
	return Infos;
}