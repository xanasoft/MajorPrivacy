#include "pch.h"
#include "Handle.h"
#include "HandleList.h"
#include "../ServiceCore.h"
#include "../Library/Helpers/MiscHelpers.h"
#include "../Library/Helpers/Service.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Processes/Process.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../Library/Common/Strings.h"

CHandle::CHandle(const std::wstring& FileName)
{
	m_FileName= FileName;
}

CHandle::~CHandle()
{
}

void CHandle::LinkProcess(const CProcessPtr& pProcess)
{
	//m_ProcessName = pProcess->GetName();
	m_ProcessId = pProcess->GetProcessId();
	m_pProcess = pProcess; // relember m_pProcess is a week pointer

	//ProcessSetNetworkFlag();
}

CVariant CHandle::ToVariant() const
{
	std::shared_lock Lock(m_Mutex);

	CVariant Handle;

	Handle.BeginIMap();

	Handle.Write(API_V_ACCESS_REF, (uint64)this);

	Handle.Write(API_V_ACCESS_PATH, m_FileName);

	Handle.Write(API_V_ACCESS_PID, m_ProcessId);

	Handle.Finish();

	return Handle;
}
