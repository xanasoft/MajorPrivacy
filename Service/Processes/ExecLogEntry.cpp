#include "pch.h"
#include "ExecLogEntry.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../../Library/API/DriverAPI.h"

#ifdef DEF_USE_POOL
CExecLogEntry::CExecLogEntry(FW::AbstractMemPool* pMem, const CFlexGuid& EnclaveGuid, EExecLogRole Role, EExecLogType Type, EEventStatus Status, uint64 MiscID, const CFlexGuid& OtherEnclave, std::wstring ActorServiceTag, uint64 TimeStamp, uint64 PID, const struct SVerifierInfo* pInfo, uint32 AccessMask)
	: CTraceLogEntry(pMem, PID)
#else
CExecLogEntry::CExecLogEntry(const CFlexGuid& EnclaveGuid, EExecLogRole Role, EExecLogType Type, EEventStatus Status, uint64 MiscID, const CFlexGuid& OtherEnclave, std::wstring ActorServiceTag, uint64 TimeStamp, uint64 PID, const struct SVerifierInfo* pInfo, uint32 AccessMask)
	: CTraceLogEntry(PID)
#endif
{
	m_Role = Role;
	m_Type = Type;
	m_Status = Status;
	m_MiscID = MiscID;
	m_OtherEnclave = OtherEnclave;
	if (pInfo) {
		m_StatusFlags = pInfo->StatusFlags;

		m_PrivateAuthority = pInfo->PrivateAuthority;
		m_SignLevel = pInfo->SignLevel;
		m_SignPolicyBits = pInfo->SignPolicyBits;

		m_FoundSignatures = pInfo->FoundSignatures;
		m_AllowedSignatures = pInfo->AllowedSignatures;
	}
	if(AccessMask)
		m_AccessMask = AccessMask;

	m_TimeStamp = TimeStamp;
#ifdef DEF_USE_POOL
	m_ServiceTag.Assign(ActorServiceTag.c_str(), ActorServiceTag.length());
#else
	m_ServiceTag = ActorServiceTag;
#endif
	m_EnclaveGuid = EnclaveGuid;
}



void CExecLogEntry::WriteVariant(StVariantWriter& Entry) const
{
	CTraceLogEntry::WriteVariant(Entry);

	Entry.Write(API_V_EVENT_ROLE, (uint32)m_Role);

	Entry.Write(API_V_EVENT_TYPE, (uint32)m_Type);

	Entry.Write(API_V_EVENT_STATUS, (uint32)m_Status);

	Entry.Write(API_V_PROC_MISC_ID, m_MiscID);
	Entry.WriteVariant(API_V_PROC_MISC_ENCLAVE, m_OtherEnclave.ToVariant(false, Entry.Allocator()));
		
	if (m_StatusFlags) {
		Entry.Write(API_V_SIGN_FLAGS, m_StatusFlags);

		Entry.Write(API_V_IMG_SIGN_AUTH, m_PrivateAuthority);
		Entry.Write(API_V_IMG_SIGN_LEVEL, m_SignLevel);
		Entry.Write(API_V_IMG_SIGN_POLICY, m_SignPolicyBits);

		Entry.Write(API_V_IMG_SIGN_BITS, m_FoundSignatures.Value);
		Entry.Write(API_V_EXEC_ALLOWED_SIGNERS, m_AllowedSignatures.Value);
	}

	if (m_AccessMask)
		Entry.Write(API_V_ACCESS_MASK, m_AccessMask);
}