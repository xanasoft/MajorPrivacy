#include "pch.h"
#include "Process.h"
#include "ProcessList.h"
#include "ServiceList.h"
#include "ServiceCore.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../../Library/API/DriverAPI.h"
#include "../../Library/Common/Strings.h"
#include "../Network/SocketList.h"
#include "../Programs/ProgramFile.h"
#include "../Programs/WindowsService.h"

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

	//m_ServiceList = theCore->ProcessList()->Services()->GetServicesByPid(m_Pid);

	auto Result = theCore->Driver()->GetProcessInfo(m_Pid);
	if (!Result.IsError())
	{
		auto Data = Result.GetValue();
		SetRawCreationTime(Data->CreateTime);

		m_ParentPid = Data->ParentPid;
		m_Name = Data->ImageName;
		m_FileName = Data->FileName;
		if(m_FileName.empty() && m_Pid == NT_OS_KERNEL_PID)
			m_FileName = NormalizeFilePath(NtOsKernel_exe);
		ASSERT(!m_Name.empty());
		ASSERT(!m_FileName.empty());
		if (m_FileName.empty())
			m_FileName = L"UNKNOWN_FILENAME";

		// driver specific fields
		//m_ImageHash = Data->FileHash;
		m_EnclaveId = Data->EnclaveId;
		m_SecState = Data->SecState;

		m_Flags = Data->Flags;
		m_SecFlags = Data->SecFlags;

		m_NumberOfImageLoads = Data->NumberOfImageLoads;
		m_NumberOfMicrosoftImageLoads = Data->NumberOfMicrosoftImageLoads;
		m_NumberOfAntimalwareImageLoads = Data->NumberOfAntimalwareImageLoads;
		m_NumberOfVerifiedImageLoads = Data->NumberOfVerifiedImageLoads;
		m_NumberOfSignedImageLoads = Data->NumberOfSignedImageLoads;
		m_NumberOfUntrustedImageLoads = Data->NumberOfUntrustedImageLoads;
	}
	else
	{
		m_FileName = GetProcessImageFileNameByProcessId((HANDLE)m_Pid);
		if (m_FileName.empty()) {
			m_Name = m_FileName = L"UNKNOWN_FILENAME";
			return false;
		}
		m_Name = GetFileNameFromPath(m_FileName);
	}

	return InitOther();
}

bool CProcess::Init(PSYSTEM_PROCESS_INFORMATION process, bool bFullProcessInfo)
{
	// init once before enlisting -> no lock needed

	//m_ServiceList = theCore->ProcessList()->Services()->GetServicesByPid(m_Pid);

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
		m_FileName = NormalizeFilePath(NtOsKernel_exe);

	CScopedHandle hProcess = CScopedHandle(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, (DWORD)m_Pid), CloseHandle);
	if (!hProcess) // try with less rights
		hProcess.Set(OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, (DWORD)m_Pid));
	if (!hProcess) // try with even less rights
		hProcess.Set(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)m_Pid));
	if (!hProcess)
		return false;

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


		Buffer.Clear();

		if (!QueryTokenVariable(ProcessToken, TokenSecurityAttributes, Buffer).IsError())
		{
			PTOKEN_SECURITY_ATTRIBUTES_INFORMATION Attributes = (PTOKEN_SECURITY_ATTRIBUTES_INFORMATION)Buffer.GetBuffer();
			for (ULONG i = 0; i < Attributes->AttributeCount; i++)
			{
				PTOKEN_SECURITY_ATTRIBUTE_V1 attribute = &Attributes->Attribute.pAttributeV1[i];

				if (attribute->ValueType != TOKEN_SECURITY_ATTRIBUTE_TYPE_STRING)
					continue;
				if (_wcsnicmp(attribute->Name.Buffer, L"WIN://SYSAPPID", attribute->Name.Length / sizeof(wchar_t)) == 0)
				{
					m_PackageFullName = std::wstring(attribute->Values.pString->Buffer, attribute->Values.pString->Length / sizeof(wchar_t));
					if (m_AppContainerName.empty() && attribute->ValueCount >= 3)
					{
						m_AppContainerName = std::wstring(attribute->Values.pString[2].Buffer, attribute->Values.pString[2].Length / sizeof(wchar_t));
						m_AppContainerSid = ::GetAppContainerSidFromName(m_AppContainerName);
					}
					break;
				}
			}
		}
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

void CProcess::UpdateSignInfo(KPH_VERIFY_AUTHORITY SignAuthority, uint32 SignLevel, uint32 SignPolicy)
{
	std::unique_lock lock(m_Mutex);

	m_SignInfo.Authority = (uint8)SignAuthority;
	if(SignLevel) m_SignInfo.Level = (uint8)SignLevel;
	if(SignPolicy) m_SignInfo.Policy = SignPolicy;
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
	auto Result = theCore->Driver()->GetProcessInfo(m_Pid);
	if (!Result.IsError())
	{
		auto Data = Result.GetValue();

		// driver specific fields
		//m_ImageHash = Data->FileHash;
		m_EnclaveId = Data->EnclaveId;
		m_SecState = Data->SecState;

		m_Flags = Data->Flags;
		m_SecFlags = Data->SecFlags;

		m_NumberOfImageLoads = Data->NumberOfImageLoads;
		m_NumberOfMicrosoftImageLoads = Data->NumberOfMicrosoftImageLoads;
		m_NumberOfAntimalwareImageLoads = Data->NumberOfAntimalwareImageLoads;
		m_NumberOfVerifiedImageLoads = Data->NumberOfVerifiedImageLoads;
		m_NumberOfSignedImageLoads = Data->NumberOfSignedImageLoads;
		m_NumberOfUntrustedImageLoads = Data->NumberOfUntrustedImageLoads;

		if (!m_SignInfo.Data) {
			KPH_PROCESS_SFLAGS kSFlags;
			kSFlags.SecFlags = m_SecFlags;
			m_SignInfo.Authority = kSFlags.SignatureAuthority;
		}
	}

	m_DnsLog.Update();

	std::shared_lock StatsLock(m_StatsMutex);
	m_Stats.UpdateStats();
}

void CProcess::AddHandle(const CHandlePtr& pHandle)
{
	std::unique_lock Lock(m_HandleMutex);
	m_HandleList.insert(pHandle);
}

void CProcess::RemoveHandle(const CHandlePtr& pHandle)
{
	std::unique_lock Lock(m_HandleMutex);
	m_HandleList.erase(pHandle);
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

	Process.BeginIMap();

	Process.Write(API_V_PID, m_Pid);
	Process.Write(API_V_CREATE_TIME, m_CreationTime);
	Process.Write(API_V_PARENT_PID, m_ParentPid);
	
	Process.Write(API_V_NAME, m_Name);
	Process.Write(API_V_FILE_PATH, m_FileName);
	Process.Write(API_V_CMD_LINE, m_CommandLine);
	//Process.Write(API_V_HASH, m_ImageHash);

	Process.Write(API_V_EID, m_EnclaveId);
	Process.Write(API_V_SEC, m_SecState);

	Process.Write(API_V_FLAGS, m_Flags);
	Process.Write(API_V_SFLAGS, m_SecFlags);
	
	Process.Write(API_V_SIGN_INFO, m_SignInfo.Data);
	Process.Write(API_V_N_IMG, m_NumberOfImageLoads);
	Process.Write(API_V_N_MS_IMG, m_NumberOfMicrosoftImageLoads);
	Process.Write(API_V_N_AV_IMG, m_NumberOfAntimalwareImageLoads);
	Process.Write(API_V_N_V_IMG, m_NumberOfVerifiedImageLoads);
	Process.Write(API_V_N_S_IMG, m_NumberOfSignedImageLoads);
	Process.Write(API_V_N_U_IMG, m_NumberOfUntrustedImageLoads);

	Process.Write(API_V_SVCS, m_ServiceList);

	Process.Write(API_V_APP_SID, m_AppContainerSid);
	Process.Write(API_V_APP_NAME, m_AppContainerName);
	Process.Write(API_V_PACK_NAME, m_PackageFullName);

	Process.Write(API_V_SOCK_LAST_ACT, m_LastActivity);
	
	Lock.unlock();

	std::shared_lock StatsLock(m_StatsMutex);

	Process.Write(API_V_SOCK_UPLOAD, m_Stats.Net.SendRate.Get());
	Process.Write(API_V_SOCK_DOWNLOAD, m_Stats.Net.ReceiveRate.Get());
	Process.Write(API_V_SOCK_UPLOADED, m_Stats.Net.SendRaw);
	Process.Write(API_V_SOCK_DOWNLOADED, m_Stats.Net.ReceiveRaw);

	StatsLock.unlock();

	std::shared_lock HandleLock(m_HandleMutex);

	CVariant Handles;
	Handles.BeginList();
	for (auto I : m_HandleList) {
		if(I->GetFileName().empty())
			continue; // skip unnamed handles
		Handles.WriteVariant(I->ToVariant());
	}
	Handles.Finish();
	Process.WriteVariant(API_V_HANDLES, Handles);

	HandleLock.unlock();

	std::shared_lock SocketLock(m_SocketMutex);

	CVariant Sockets;
	Sockets.BeginList();
	for (auto I : m_SocketList)
		Sockets.WriteVariant(I->ToVariant());
	Sockets.Finish();
	Process.WriteVariant(API_V_SOCKETS, Sockets);

	SocketLock.unlock();

	Process.Finish();

	return Process;
}
