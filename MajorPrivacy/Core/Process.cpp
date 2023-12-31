#include "pch.h"
#include "Process.h"
#include "PrivacyCore.h"
#include "../Library/Common/XVariant.h"
#include "../Service/ServiceAPI.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Helpers/WinHelper.h"


CProcess::CProcess(quint32 Pid, QObject* parent)
 : QObject(parent)
{
	m_Pid = Pid;
}

QIcon CProcess::DefaultIcon() const
{
	QIcon ExeIcon(":/exe16.png");
	ExeIcon.addFile(":/exe32.png");
	ExeIcon.addFile(":/exe48.png");
	ExeIcon.addFile(":/exe64.png");
	//return QIcon(":/Icons/Process.png");
	return ExeIcon;
}

void CProcess::FromVariant(const class XVariant& Process)
{
	Process.ReadRawMap([&](const SVarName& Name, const CVariant& vData) {
		const XVariant& Data = *(XVariant*)&vData;

			 if (VAR_TEST_NAME(Name, SVC_API_PROC_CREATED))		m_CreationTime = Data;
		else if (VAR_TEST_NAME(Name, SVC_API_PROC_PARENT))		m_ParentPid = Data;

		else if (VAR_TEST_NAME(Name, SVC_API_PROC_NAME))		m_Name = Data.AsQStr();
		else if (VAR_TEST_NAME(Name, SVC_API_PROC_PATH)) 
		{		 
			bool bInitIcon = m_FileName.isEmpty();
			m_FileName = Data.AsQStr();

			if (bInitIcon && !m_FileName.isEmpty())
			{
				QString Path = QString::fromStdWString(NtPathToDosPath(m_FileName.toStdWString()));
				m_Icon = LoadWindowsIconEx(Path, 0);
				if (m_Icon.availableSizes().isEmpty())
					m_Icon = DefaultIcon();
			}
		}

		//else if (VAR_TEST_NAME(Name, SVC_API_PROC_HASH))		m_ImageHash = Data.AsQBytes();

		else if (VAR_TEST_NAME(Name, SVC_API_PROC_APP_SID))		m_AppContainerSid = Data.AsQStr();
		else if (VAR_TEST_NAME(Name, SVC_API_PROC_APP_NAME))	m_AppContainerName = Data.AsQStr();
		//else if (VAR_TEST_NAME(Name, SVC_API_PROC_PACK_NAME))	m_PackageFullName = Data.AsQStr();

		else if (VAR_TEST_NAME(Name, SVC_API_PROC_EID))			m_EnclaveId = Data;

		else if (VAR_TEST_NAME(Name, SVC_API_PROC_SVCS))		m_ServiceList = Data.AsQList();

		else if (VAR_TEST_NAME(Name, SVC_API_PROC_SOCKETS))		UpdateSockets(Data);

		// else unknown tag

	});
}

void CProcess::UpdateSockets(const class XVariant& Sockets)
{
	QMap<quint64, CSocketPtr> OldSockets = m_Sockets;

	Sockets.ReadRawList([&](const CVariant& vData) {
		const XVariant& Socket = *(XVariant*)&vData;

		quint64 SockRef = Socket.Find(SVC_API_SOCK_REF);

		CSocketPtr pSocket = OldSockets.take(SockRef);
		if (!pSocket) {
			pSocket = CSocketPtr(new CSocket());
			m_Sockets.insert(SockRef, pSocket);
		} 
		
		pSocket->FromVariant(Socket);
	});

	foreach(quint64 SockRef, OldSockets.keys())
		m_Sockets.remove(OldSockets.take(SockRef)->GetSocketRef());
}