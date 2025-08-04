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
#include "../Programs/ProgramManager.h"

#include "../../Library/Helpers/Scoped.h"
#include "../../Library/Helpers/TokenUtil.h"
#include "../../Library/Helpers/SID.h"
#include "../../Library/Helpers/AppUtil.h"
#include "../../Library/Helpers/NtUtil.h"
#include "../../Library/Helpers/CertUtil.h"

//static inline uint16_t reverseBits16(uint16_t x)
//{
//	x = (uint16_t)(((x & 0x5555U) << 1) | ((x & 0xAAAAU) >> 1));
//	x = (uint16_t)(((x & 0x3333U) << 2) | ((x & 0xCCCCU) >> 2));
//	x = (uint16_t)(((x & 0x0F0FU) << 4) | ((x & 0xF0F0U) >> 4));
//	x = (uint16_t)((x << 8) | (x >> 8));
//	return x;
//}
//
//static inline uint32_t reverseBits32(uint32_t x)
//{
//	x = ((x & 0x55555555UL) << 1)  | ((x & 0xAAAAAAAAUL) >> 1);
//	x = ((x & 0x33333333UL) << 2)  | ((x & 0xCCCCCCCCUL) >> 2);
//	x = ((x & 0x0F0F0F0FUL) << 4)  | ((x & 0xF0F0F0F0UL) >> 4);
//	x = ((x & 0x00FF00FFUL) << 8)  | ((x & 0xFF00FF00UL) >> 8);
//	x = (x << 16) | (x >> 16);
//	return x;
//}
//
//static inline uint64_t reverseBits64(uint64_t x)
//{
//	x = ((x & 0x5555555555555555ULL) << 1)  | ((x & 0xAAAAAAAAAAAAAAAAULL) >> 1); // Swap odd and even bits
//	x = ((x & 0x3333333333333333ULL) << 2)  | ((x & 0xCCCCCCCCCCCCCCCCULL) >> 2); // Swap consecutive pairs (2-bit groups)
//	x = ((x & 0x0F0F0F0F0F0F0F0FULL) << 4)  | ((x & 0xF0F0F0F0F0F0F0F0ULL) >> 4); // Swap nibbles (4-bit groups)
//	x = ((x & 0x00FF00FF00FF00FFULL) << 8)  | ((x & 0xFF00FF00FF00FF00ULL) >> 8); // Swap bytes
//	x = ((x & 0x0000FFFF0000FFFFULL) << 16) | ((x & 0xFFFF0000FFFF0000ULL) >> 16); // Swap 16-bit halves
//	x = (x << 32) | (x >> 32); 	// Swap 32-bit halves
//	return x;
//}

SProcessUID::SProcessUID(uint64 uPid, uint64 msTime)
{
	//
	// Note: On Windows PID's are 32-bit and word aligned i.e. the least significant 2 bits are always 0 hence we can drop them
	//

	// Variant A
	//
	// 11111111 11111111 1111 1111 111111 00                                     - PID
	// 00000000 00000000 0000(0000 000000)00 00000000 00000000 00000000 00000000 - uint64 - PID (shared msb bits) timestamp in miliseconds
	//					      1111 111111 11 11111111 11111111 11111111 11111111 - unix timestamp (Jun 2527)
	//                           1 100101 00 00100110 01000000 01010010 01111101 - unix timestamp (Jan 2025)
	//
	// rev_PID_LSB       (rev_PID_MSB Time_MSB)                         Time_LSB
	//
	//PUID = reverseBits64(((uPid & 0xFFFFFFFC) >> 2)) ^ (msTime & 0x00000FFFFFFFFFFF);

	// Variant B
	//
	// 11111111 11111111 11111111 11111100                                     - PID
	// 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 - uint64 - 30 bit Pid, 34 bit timestamp in seconds
	//                                  11 11111111 11111111 11111111 11111111 - unix timestamp (May 2514)
	//										1100111 01110110 01010110 00011001 - unix timestamp (Jan 2025)
	//
	//PUID = ((uPid & 0xFFFFFFFC) << 32) | ((msTime/1000llu) & 0x00000003FFFFFFFF);

	// Variant C
	//
	// 11111111 11111111 11111100                                              - PID
	// 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 - uint64 - 22 bit Pid, 42 bit timestamp in seconds
	//                         11 11111111 11111111 11111111 11111111 11111111 - unix timestamp (May 2109) - 3FFFFFFFFFF - 4398046511103
	//                         01 10010100 00100110 01000000 01010010 01111101 - unix timestamp (Jan 2025)
	//
	PUID = (((uPid) << 40) & 0xFFFFFC0000000000) ^ (msTime & 0x000003FFFFFFFFFF);

	// Variant C seams best, 2109 is far enough in the future and we have no slow integer division, even though the PID gets truncated a bit
}

const wchar_t* CProcess::NtOsKernel_exe = L"\\SystemRoot\\System32\\ntoskrnl.exe";
//const wchar_t* CProcess::NtOsKernel_exe = L"%SystemRoot%\\System32\\ntoskrnl.exe";

CProcess::CProcess(uint64 Pid)
{
	m_Pid = Pid;

	m_CreateTimeStamp = GetCurTime() * 1000; // todo
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
		m_CreatorPid = Data->CreatorPid;
		m_Name = Data->ImageName;
		m_NtFilePath = Data->FileName;
		if(m_NtFilePath.empty() && (m_Pid == NT_OS_KERNEL_PID || m_Name == L"Secure System"))
			m_NtFilePath = std::wstring(NtOsKernel_exe);
		ASSERT(!m_Name.empty());
		ASSERT(!m_NtFilePath.empty());
		if (m_NtFilePath.empty())
			m_NtFilePath = L"UNKNOWN_FILENAME";

		// driver specific fields
		m_EnclaveGuid = Data->Enclave;
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
		m_NtFilePath = GetProcessImageFileNameByProcessId((HANDLE)m_Pid);
		if (m_NtFilePath.empty()) {
			m_Name = m_NtFilePath = L"UNKNOWN_FILENAME";
			return false;
		}
		m_Name = GetFileNameFromPath(m_NtFilePath);
	}

	if(m_NtFilePath == L"MemCompression")
		m_NtFilePath = std::wstring(NtOsKernel_exe) + L"\\MemCompression";
	else if(m_NtFilePath == L"Registry")
		m_NtFilePath = std::wstring(NtOsKernel_exe) + L"\\Registry";

	return InitOther();
}

bool CProcess::Init(PSYSTEM_PROCESS_INFORMATION process, bool bFullProcessInfo)
{
	// init once before enlisting -> no lock needed

	//m_ServiceList = theCore->ProcessList()->Services()->GetServicesByPid(m_Pid);

	m_ParentPid = (uint64)process->InheritedFromUniqueProcessId;
	if (bFullProcessInfo)
	{
		m_NtFilePath = std::wstring(process->ImageName.Buffer, process->ImageName.Length / sizeof(wchar_t));

		m_Name = GetFileNameFromPath(m_NtFilePath);
	}
	else
	{
		m_NtFilePath = GetProcessImageFileNameByProcessId((HANDLE)m_Pid);

		m_Name = std::wstring(process->ImageName.Buffer, process->ImageName.Length / sizeof(wchar_t));
	}

	/*PSYSTEM_PROCESS_INFORMATION_EXTENSION processExtension = NULL;
	if (WindowsVersion >= WINDOWS_10_RS3 && !IsExecutingInWow64() && IS_REAL_PROCESS_ID(m->UniqueProcessId))
	{
		processExtension = bFullProcessInfo ? PH_EXTENDED_PROCESS_EXTENSION(Process) : PH_PROCESS_EXTENSION(Process);
	}
	
	m_SequenceNr = processExtension->ProcessSequenceNumber;*/

	SetRawCreationTime(process->CreateTime.QuadPart);

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
		m_NtFilePath = std::wstring(NtOsKernel_exe);

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
		SE_TOKEN_USER tokenUser;
		ULONG returnLength;
		if (NT_SUCCESS(NtQueryInformationToken(ProcessToken, TokenUser, &tokenUser, sizeof(SE_TOKEN_USER), &returnLength)))
		{
			SSid UserSid = &tokenUser.Sid;
			m_UserSid = UserSid.ToWString();
		}

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
					m_AppPackageName = std::wstring(attribute->Values.pString->Buffer, attribute->Values.pString->Length / sizeof(wchar_t));
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

bool CProcess::InitLibs()
{
	CScopedHandle hProcess = CScopedHandle(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, (DWORD)m_Pid), CloseHandle);
	if (!hProcess)
		return false;

	std::wstring ProgramPath = theCore->NormalizePath(GetNtFilePath());

	for (PVOID baseAddress = (PVOID)0;baseAddress != (PVOID)-1;)
	{
		MEMORY_BASIC_INFORMATION basicInfo;
		if (!NT_SUCCESS(NtQueryVirtualMemory(hProcess, baseAddress, MemoryBasicInformation, &basicInfo, sizeof(MEMORY_BASIC_INFORMATION), NULL)))
			break;

		if (/*basicInfo.Type != MEM_MAPPED &&*/ basicInfo.Type != MEM_IMAGE) {
			baseAddress = PTR_ADD_OFFSET(baseAddress, basicInfo.RegionSize);
			continue;
		}

		PVOID allocationBase = basicInfo.AllocationBase;
		SIZE_T allocationSize = 0;

		do {
			baseAddress = PTR_ADD_OFFSET(baseAddress, basicInfo.RegionSize);
			allocationSize += basicInfo.RegionSize;
			if (!NT_SUCCESS(NtQueryVirtualMemory(hProcess, baseAddress, MemoryBasicInformation, &basicInfo, sizeof(MEMORY_BASIC_INFORMATION), NULL))) {
				baseAddress = (PVOID)-1;
				break;
			}
		} while (basicInfo.AllocationBase == allocationBase);


		SIZE_T returnLength = 0;
		std::vector<BYTE> buffer(0x1000);
		NTSTATUS status = NtQueryVirtualMemory(hProcess, allocationBase, MemoryMappedFilenameInformation, buffer.data(), buffer.size(), &returnLength);
		if (status == STATUS_BUFFER_OVERFLOW && returnLength > 0) {
			buffer.resize(returnLength);
			status = NtQueryVirtualMemory(hProcess, allocationBase, MemoryMappedFilenameInformation, buffer.data(), buffer.size(), &returnLength);
		}

		if (NT_SUCCESS(status))
		{
			auto pUnicodeString = (PUNICODE_STRING)buffer.data();
			std::wstring ModulePath = theCore->NormalizePath(std::wstring(pUnicodeString->Buffer, pUnicodeString->Length / sizeof(wchar_t)), false);
		
			CProgramFilePtr pProgram = GetProgram();
			bool bIsProcess = ProgramPath == MkLower(ModulePath);
			if (!bIsProcess && pProgram) 
			{
				CProgramLibraryPtr pLibrary = theCore->ProgramManager()->GetLibrary(ModulePath, true);

				AddLibrary(pLibrary);
				pProgram->AddLibrary(m_EnclaveGuid, pLibrary, GetCurrentTimeAsFileTime());
				//if (pProgram->HashInfoUnknown(pLibrary))
				//{
				//	theCore->ThreadPool()->enqueueTask([pProgram, pLibrary](const std::wstring& ModulePath) {
				//		SVerifierInfo VerifyInfo;
				//		MakeVerifyInfo(ModulePath, VerifyInfo);
				//		pProgram->AddLibrary(pLibrary, 0, &VerifyInfo);
				//	}, ModulePath);
				//}
			}
		}
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

void CProcess::UpdateSignInfo(const struct SVerifierInfo* pVerifyInfo)
{
	std::unique_lock lock(m_Mutex);

	m_SignInfo.Update(pVerifyInfo);
}

bool CProcess::HashInfoUnknown() const
{
	std::shared_lock lock(m_Mutex);

	return m_SignInfo.GetHashStatus() == EHashStatus::eHashUnknown;
}

bool CProcess::MakeVerifyInfo(const std::wstring& ModulePath, SVerifierInfo& VerifyInfo)
{
	/*SVerifierInfo VerifyInfo;
	SEmbeddedCIInfoPtr pEmbeddedInfo = GetEmbeddedCIInfo(ModulePath);
	if (pEmbeddedInfo && pEmbeddedInfo->EmbeddedSignatureValid && !pEmbeddedInfo->EmbeddedSigners.empty()) 
	{
		VerifyInfo.VerificationFlags |= KPH_VERIFY_FLAG_CI_SIG_DUMMY;
		if (!pEmbeddedInfo->EmbeddedSigners[0]->SHA256.empty())
			VerifyInfo.SignerHash = pEmbeddedInfo->EmbeddedSigners[0]->SHA256;
		else
			VerifyInfo.SignerHash = pEmbeddedInfo->EmbeddedSigners[0]->SHA1;
		VerifyInfo.SignerName = std::string(pEmbeddedInfo->EmbeddedSigners[0]->Subject.begin(), pEmbeddedInfo->EmbeddedSigners[0]->Subject.end());
	}
	else 
	{
		SCatalogCIInfoPtr pCatalogInfo = GetCatalogCIInfo(ModulePath);
		if (pCatalogInfo && !pCatalogInfo->CatalogSigners.empty()) 
		{
			auto pCatalog = pCatalogInfo->CatalogSigners.begin()->second;
			if (!pCatalog.empty()) 
			{
				VerifyInfo.VerificationFlags |= KPH_VERIFY_FLAG_CI_SIG_DUMMY;
				if (!pCatalog[0]->SHA256.empty())
					VerifyInfo.SignerHash = pCatalog[0]->SHA256;
				else
					VerifyInfo.SignerHash = pCatalog[0]->SHA1;
				VerifyInfo.SignerName = std::string(pCatalog[0]->Subject.begin(), pCatalog[0]->Subject.end());
			}
		}
	}*/

	SCICertificatePtr pCIInfo = GetCIInfo(ModulePath);
	VerifyInfo.VerificationFlags |= KPH_VERIFY_FLAG_CI_SIG_DUMMY;

	// todo hash file

	if (!pCIInfo) 
		return false;

	if (!pCIInfo->SHA256.empty())
		VerifyInfo.SignerHash = pCIInfo->SHA256;
	else
		VerifyInfo.SignerHash = pCIInfo->SHA1;
#pragma warning(push)
#pragma warning(disable : 4244)
	VerifyInfo.SignerName = std::string(pCIInfo->Subject.begin(), pCIInfo->Subject.end());
#pragma warning(pop)

	return true;
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
		if (m_EnclaveGuid.IsNull() && !Data->Enclave.empty()) {

			//
			// With the new driver behavioure this should no logner happen
			//

			DbgPrint(L"Enclave GUID selayed set for process %s\n", m_Name.c_str());

			m_EnclaveGuid = Data->Enclave;
		}
		m_SecState = Data->SecState;

		m_Flags = Data->Flags;
		m_SecFlags = Data->SecFlags;

		m_NumberOfImageLoads = Data->NumberOfImageLoads;
		m_NumberOfMicrosoftImageLoads = Data->NumberOfMicrosoftImageLoads;
		m_NumberOfAntimalwareImageLoads = Data->NumberOfAntimalwareImageLoads;
		m_NumberOfVerifiedImageLoads = Data->NumberOfVerifiedImageLoads;
		m_NumberOfSignedImageLoads = Data->NumberOfSignedImageLoads;
		m_NumberOfUntrustedImageLoads = Data->NumberOfUntrustedImageLoads;

		if (!m_SignInfo.GetRawInfo()) {
			KPH_PROCESS_SFLAGS kSFlags;
			kSFlags.SecFlags = m_SecFlags;
			m_SignInfo.SetAuthority(kSFlags.SignatureAuthority);
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
	Lock.unlock();

	bool bSave = theCore->Config()->GetBool("Service", "NetTrace", true);
	if (pProg) {
		ETracePreset ePreset = pProg->GetResTrace();
		if (!pProg->TrafficLog()->RemoveSocket(pSocket, bNoCommit || ePreset == ETracePreset::eNoTrace || (ePreset == ETracePreset::eDefault && !bSave)) && pSvc) {
			ePreset = pSvc->GetResTrace();
			pSvc->TrafficLog()->RemoveSocket(pSocket, bNoCommit|| ePreset == ETracePreset::eNoTrace || (ePreset == ETracePreset::eDefault && !bSave));
		}
	}
}

void CProcess::AddNetworkIO(int Type, uint32 TransferSize)
{
	std::shared_lock StatsLock(m_StatsMutex);

	m_LastNetActivity = GetCurTime() * 1000ULL;

	switch ((CSocketList::EEventType)Type)
	{
	case CSocketList::EEventType::Receive:	m_Stats.Net.AddReceive(TransferSize); break;
	case CSocketList::EEventType::Send:		m_Stats.Net.AddSend(TransferSize); break;
	}
}

StVariant CProcess::ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool) const
{
	std::shared_lock Lock(m_Mutex);

	StVariantWriter Process(pMemPool);
	Process.BeginIndex();

	Process.Write(API_V_PID, m_Pid);
	Process.Write(API_V_CREATE_TIME, m_CreationTime);
	Process.Write(API_V_PARENT_PID, m_ParentPid);
	Process.Write(API_V_CREATOR_PID, m_CreatorPid);
	
	Process.WriteEx(API_V_NAME, m_Name);
	Process.WriteEx(API_V_FILE_NT_PATH, m_NtFilePath);
	Process.WriteEx(API_V_CMD_LINE, m_CommandLine);

	Process.WriteVariant(API_V_ENCLAVE, m_EnclaveGuid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids, pMemPool));
	Process.Write(API_V_KPP_STATE, m_SecState);

	Process.Write(API_V_FLAGS, m_Flags);
	Process.Write(API_V_SFLAGS, m_SecFlags);
	
	Process.WriteVariant(API_V_SIGN_INFO, m_SignInfo.ToVariant(Opts, pMemPool));
	Process.Write(API_V_NUM_IMG, m_NumberOfImageLoads);
	Process.Write(API_V_NUM_MS_IMG, m_NumberOfMicrosoftImageLoads);
	Process.Write(API_V_NUM_AV_IMG, m_NumberOfAntimalwareImageLoads);
	Process.Write(API_V_NUM_V_IMG, m_NumberOfVerifiedImageLoads);
	Process.Write(API_V_NUM_S_IMG, m_NumberOfSignedImageLoads);
	Process.Write(API_V_NUM_U_IMG, m_NumberOfUntrustedImageLoads);

	Process.WriteVariant(API_V_SERVICES, StVariant(pMemPool, m_ServiceList));

	Process.WriteEx(API_V_APP_SID, m_AppContainerSid);
	Process.WriteEx(API_V_APP_NAME, m_AppContainerName);
	Process.WriteEx(API_V_PACK_NAME, m_AppPackageName);

	Process.WriteEx(API_V_USER_SID, m_UserSid);

	Process.Write(API_V_SOCK_LAST_NET_ACT, m_LastNetActivity);
	
	Lock.unlock();

	std::shared_lock StatsLock(m_StatsMutex);

	Process.Write(API_V_SOCK_UPLOAD, m_Stats.Net.SendRate.Get());
	Process.Write(API_V_SOCK_DOWNLOAD, m_Stats.Net.ReceiveRate.Get());
	Process.Write(API_V_SOCK_UPLOADED, m_Stats.Net.SendRaw);
	Process.Write(API_V_SOCK_DOWNLOADED, m_Stats.Net.ReceiveRaw);

	StatsLock.unlock();

	std::shared_lock HandleLock(m_HandleMutex);

	StVariantWriter Handles(pMemPool);
	Handles.BeginList();
	for (auto I : m_HandleList) {
		if(I->GetFileName().empty())
			continue; // skip unnamed handles
		Handles.WriteVariant(I->ToVariant(pMemPool));
	}
	Process.WriteVariant(API_V_HANDLES, Handles.Finish());

	HandleLock.unlock();

	std::shared_lock SocketLock(m_SocketMutex);

	StVariantWriter Sockets(pMemPool);
	Sockets.BeginList();
	for (auto I : m_SocketList)
		Sockets.WriteVariant(I->ToVariant(pMemPool));
	Process.WriteVariant(API_V_SOCKETS, Sockets.Finish());

	SocketLock.unlock();

	return Process.Finish();
}
