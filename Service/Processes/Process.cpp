#include "pch.h"
#include "Process.h"
#include "ProcessList.h"
#include "ServiceList.h"
#include "ServiceCore.h"
#include "ServiceAPI.h"
#include "../../Library/API/DriverAPI.h"
#include "../../Library/Common/Strings.h"
#include "../Network/SocketList.h"
#include "../Programs/ProgramFile.h"

#include "../../Library/Helpers/Scoped.h"
#include "../../Library/Helpers/TokenUtil.h"
#include "../../Library/Helpers/SID.h"
#include "../../Library/Helpers/AppUtil.h"
#include "../../Library/Helpers/NtUtil.h"

const wchar_t* CProcess::NtOsKernel_exe = L"\\SystemRoot\\System32\\ntoskrnl.exe";

CProcess::CProcess(uint64 Pid)
{
	m_Pid = Pid;

	m_CreateTimeStamp = GetTime() * 1000; // todo
}

CProcess::~CProcess()
{
	//ASSERT(m_SocketList.empty()); // todo
}

bool CProcess::Init()
{
	// init once before enlisting -> no lock needed

	//m_ServiceList = svcCore->ProcessList()->Services()->GetServicesByPid(m_Pid);

	auto Result = svcCore->Driver()->GetProcessInfo(m_Pid);
	if (!Result.IsError())
	{
		auto Data = Result.GetValue();
		SetRawCreationTime(Data->CreateTime);

		m_ParentPid = Data->ParentPid;
		m_Name = Data->ImageName;
		m_FileName = Data->FileName;
		ASSERT(!m_FileName.empty() || m_Pid == 4); // todo: test

		// driver specific fields
		m_ImageHash = Data->FileHash;
		m_EnclaveId = Data->EnclaveId;
	}
	else
	{
		// todo PID

		m_FileName = GetProcessImageFileNameByProcessId((HANDLE)m_Pid);
		//ASSERT(!m_FileName.empty());
		if (m_FileName.empty())
			m_FileName = L"UNKNOWN_FILENAME";

		m_Name = GetFileNameFromPath(m_FileName);
	}

	return InitOther();
}

bool CProcess::Init(PSYSTEM_PROCESS_INFORMATION process, bool bFullProcessInfo)
{
	// init once before enlisting -> no lock needed

	//m_ServiceList = svcCore->ProcessList()->Services()->GetServicesByPid(m_Pid);

	m_ParentPid = (uint64)process->InheritedFromUniqueProcessId;
	if (bFullProcessInfo)
	{
		m_FileName = std::wstring(process->ImageName.Buffer, process->ImageName.Length / sizeof(wchar_t));

		m_Name = GetFileNameFromPath(m_FileName);
	}
	else
	{
		m_FileName = GetProcessImageFileNameByProcessId((HANDLE)m_Pid);

		m_Name = std::wstring(process->ImageName.Buffer, process->ImageName.Length / sizeof(wchar_t));
	}

	/*PSYSTEM_PROCESS_INFORMATION_EXTENSION processExtension = NULL;
	if (WindowsVersion >= WINDOWS_10_RS3 && !IsExecutingInWow64() && IS_REAL_PROCESS_ID(m->UniqueProcessId))
	{
		processExtension = bFullProcessInfo ? PH_EXTENDED_PROCESS_EXTENSION(Process) : PH_PROCESS_EXTENSION(Process);
	}
	
	m_SequenceNr = processExtension->ProcessSequenceNumber;*/

	SetRawCreationTime(process->CreateTime.QuadPart);

	//m_ImageHash = // compute ourselves?

	return InitOther();
}

void CProcess::SetRawCreationTime(uint64 TimeStamp)
{
	m_CreationTime = TimeStamp;
	m_CreateTimeStamp = FILETIME2ms(TimeStamp);
}

bool CProcess::InitOther()
{
	if (m_Pid == NT_OS_KERNEL_PID)
		m_FileName = NtOsKernel_exe;

	CScopedHandle hProcess = CScopedHandle(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, (DWORD)m_Pid), CloseHandle);
	if (!hProcess) // try with less rights
		hProcess.Set(OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, (DWORD)m_Pid));
	if (!hProcess) // try with even less rights
		hProcess.Set(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)m_Pid));
	if (!hProcess)
		return true;

	if (m_CreationTime == 0) {
		FILETIME time, time1, time2, time3;
		if (GetProcessTimes(hProcess, &time, &time1, &time2, &time3))
			SetRawCreationTime(((PLARGE_INTEGER)&time)->QuadPart);
	}
	
	CScopedHandle ProcessToken = CScopedHandle((HANDLE)0, CloseHandle);
	if (OpenProcessToken(hProcess, TOKEN_QUERY, &ProcessToken))
	{
		CBuffer Buffer;

		if (!QueryTokenVariable(ProcessToken, TokenAppContainerSid, Buffer).IsError())
		{
			PTOKEN_APPCONTAINER_INFORMATION pAppContainerInfo = (PTOKEN_APPCONTAINER_INFORMATION)Buffer.GetBuffer();
			if (pAppContainerInfo->TokenAppContainer)
			{
				SSid AppContainerSid = (struct _SID*)pAppContainerInfo->TokenAppContainer;
				m_AppContainerSid = AppContainerSid.ToWString();
				m_AppContainerName = ::GetAppContainerNameBySid(AppContainerSid);
			}
		}
		

		//Buffer.Clear();

		//if (!QueryTokenVariable(ProcessToken, TokenSecurityAttributes, Buffer).IsError())
		//{
		//	PTOKEN_SECURITY_ATTRIBUTES_INFORMATION Attributes = (PTOKEN_SECURITY_ATTRIBUTES_INFORMATION)Buffer.GetBuffer();
		//	for (ULONG i = 0; i < Attributes->AttributeCount; i++)
		//	{
		//		PTOKEN_SECURITY_ATTRIBUTE_V1 attribute = &Attributes->Attribute.pAttributeV1[i];

		//		if (attribute->ValueType != TOKEN_SECURITY_ATTRIBUTE_TYPE_STRING)
		//			continue;
		//		if (_wcsnicmp(attribute->Name.Buffer, L"WIN://SYSAPPID", attribute->Name.Length / sizeof(wchar_t)) == 0)
		//		{
		//			m_PackageFullName = std::wstring(attribute->Values.pString->Buffer, attribute->Values.pString->Length / sizeof(wchar_t));
		//			break;
		//		}
		//	}
		//}
	}
	
#define ProcessCommandLineInformation ((PROCESSINFOCLASS)60)
	ULONG returnLength = 0;
	NTSTATUS status = NtQueryInformationProcess(hProcess, ProcessCommandLineInformation, NULL, 0, &returnLength);
	if (!(status != STATUS_BUFFER_OVERFLOW && status != STATUS_BUFFER_TOO_SMALL && status != STATUS_INFO_LENGTH_MISMATCH))
	{
		PUNICODE_STRING commandLine = (PUNICODE_STRING)malloc(returnLength);
		status = NtQueryInformationProcess(hProcess, ProcessCommandLineInformation, commandLine, returnLength, &returnLength);
		if (NT_SUCCESS(status) && commandLine->Buffer != NULL)
			m_CommandLine = commandLine->Buffer;
		free(commandLine);
	}
#undef ProcessCommandLineInformation

	if (m_CommandLine.empty()) // fall back to the win 7 method - requirers PROCESS_VM_READ
	{
		m_CommandLine = GetPebString(hProcess, AppCommandLine);
	}

	return true;
}

std::wstring CProcess::GetWorkDir() const
{
	CScopedHandle hProcess = CScopedHandle(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, (DWORD)m_Pid), CloseHandle);
	if (!hProcess)
		return L"";

	return GetPebString(hProcess, AppCurrentDirectory);
}

bool CProcess::Update()
{
	if (m_ParentPid == -1) {
		std::unique_lock Lock(m_Mutex);
		Init();
	}

	UpdateMisc();

	return true;
}

bool CProcess::Update(PSYSTEM_PROCESS_INFORMATION process, bool bFullProcessInfo)
{
	if (m_ParentPid == -1) {
		std::unique_lock Lock(m_Mutex);
		Init(process, bFullProcessInfo);
	}

	UpdateMisc();

	return true;
}

void CProcess::UpdateMisc()
{
	m_DnsLog.Update();

	std::shared_lock StatsLock(m_StatsMutex);
	m_Stats.UpdateStats();
}

void CProcess::AddSocket(const CSocketPtr& pSocket)
{
	CProgramFilePtr pProg = GetProgram();

	CWindowsServicePtr pSvc;
	std::wstring SvcTag = pSocket->GetOwnerServiceName();
	if (!SvcTag.empty())
		pSvc = pProg->GetService(SvcTag);
	
	std::unique_lock Lock(m_SocketMutex);
	m_SocketList.insert(pSocket);

	if(pSvc) pSvc->TrafficLog()->AddSocket(pSocket);
	else if(pProg) pProg->TrafficLog()->AddSocket(pSocket);
}

void CProcess::RemoveSocket(const CSocketPtr& pSocket, bool bNoCommit)
{
	CProgramFilePtr pProg = GetProgram();

	CWindowsServicePtr pSvc;
	std::wstring SvcTag = pSocket->GetOwnerServiceName();
	if (!SvcTag.empty())
		pSvc = pProg->GetService(SvcTag);

	std::unique_lock Lock(m_SocketMutex);
	m_SocketList.erase(pSocket);

	if (pProg) {
		if (!pProg->TrafficLog()->RemoveSocket(pSocket, bNoCommit) && pSvc)
			pSvc->TrafficLog()->RemoveSocket(pSocket, bNoCommit);
	}
}

void CProcess::AddNetworkIO(int Type, uint32 TransferSize)
{
	std::shared_lock StatsLock(m_StatsMutex);

	m_LastActivity = GetTime() * 1000ULL;

	switch ((CSocketList::EEventType)Type)
	{
	case CSocketList::EEventType::Receive:	m_Stats.Net.AddReceive(TransferSize); break;
	case CSocketList::EEventType::Send:		m_Stats.Net.AddSend(TransferSize); break;
	}
}

CVariant CProcess::ToVariant() const
{
	std::shared_lock Lock(m_Mutex);

	CVariant Process;

	Process.BeginMap();

	Process.Write(SVC_API_PROC_PID, m_Pid);
	Process.Write(SVC_API_PROC_CREATED, m_CreationTime);
	Process.Write(SVC_API_PROC_PARENT, m_ParentPid);

	Process.Write(SVC_API_PROC_NAME, m_Name);
	Process.Write(SVC_API_PROC_PATH, m_FileName);
	Process.Write(SVC_API_PROC_HASH, m_ImageHash);

	Process.Write(SVC_API_PROC_EID, m_EnclaveId);

	Process.Write(SVC_API_PROC_SVCS, m_ServiceList);

	Process.Write(SVC_API_PROC_APP_SID, m_AppContainerSid);
	Process.Write(SVC_API_PROC_APP_NAME, m_AppContainerName);
	//Process.Write(SVC_API_PROC_PACK_NAME, m_PackageFullName);

	Process.Write(SVC_API_SOCK_LAST_ACT, m_LastActivity);

	Lock.unlock();

	std::shared_lock StatsLock(m_StatsMutex);

	Process.Write(SVC_API_SOCK_UPLOAD, m_Stats.Net.SendRate.Get());
	Process.Write(SVC_API_SOCK_DOWNLOAD, m_Stats.Net.ReceiveRate.Get());
	Process.Write(SVC_API_SOCK_UPLOADED, m_Stats.Net.SendRaw);
	Process.Write(SVC_API_SOCK_DOWNLOADED, m_Stats.Net.ReceiveRaw);

	StatsLock.unlock();

	std::shared_lock SocksLock(m_SocketMutex);

	CVariant Sockets;
	Sockets.BeginList();
	for (auto I : m_SocketList)
		Sockets.WriteVariant(I->ToVariant());
	Sockets.Finish();
	Process.WriteVariant(SVC_API_PROC_SOCKETS, Sockets);

	Process.Finish();

	return Process;
}
