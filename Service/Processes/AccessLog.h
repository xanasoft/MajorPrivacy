#pragma once
#include "../Library/Common/StVariant.h"
#include "../Library/API/PrivacyDefs.h"
#include "../Library/Common/FlexGuid.h"
#include "../Processes/Process.h"

#include "../Framework/Core/MemoryPool.h"
#include "../Framework/Core/Object.h"
#include "../Framework/Core/Map.h"

class CAccessLog
{
	TRACK_OBJECT(CAccessLog)
public:
	CAccessLog();
	~CAccessLog();

#ifdef DEF_USE_POOL
	void SetPool(FW::AbstractMemPool* pMem) { m_pMem = pMem; }
#endif

	struct SExecInfo
	{
		//TRACK_OBJECT(SExecInfo)
		uint64						LastExecTime = 0;
		CFlexGuid					EnclaveGuid;
		bool						bBlocked = false;
#ifdef DEF_USE_POOL
		FW::StringW					CommandLine;
#else
		std::wstring				CommandLine;
#endif
	};

	struct SExecLog
	{
		//TRACK_OBJECT(SExecLog)
#ifdef DEF_USE_POOL
		FW::Map<uint64, SExecInfo> Infos; // key: Process UID
#else
		std::map<uint64, SExecInfo> Infos; // key: Process UID
#endif
	};

	void AddExecParent(uint64 ProgramUID, const CFlexGuid& EnclaveGuid, const SProcessUID& ProcessUID, const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked);
	void StoreAllExecParents(StVariantWriter& List, const CFlexGuid& EnclaveGuid, const SVarWriteOpt& Opts) const;
	bool LoadExecParent(const StVariant& Data);

	void AddExecChild(uint64 ProgramUID, const CFlexGuid& EnclaveGuid, const SProcessUID& ProcessUID, const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked);
	void StoreAllExecChildren(StVariantWriter& List, const CFlexGuid& EnclaveGuid, const SVarWriteOpt& Opts, const std::wstring& SvcTag = L"") const;
	bool LoadExecChild(const StVariant& Data);


	struct SAccessInfo
	{
		//TRACK_OBJECT(SAccessInfo)
		uint64						LastAccessTime = 0;
		CFlexGuid					EnclaveGuid;
		uint32						ProcessAccessMask = 0;
		uint32						ThreadAccessMask = 0;
		bool						bBlocked = false;
	};

	struct SIngressLog
	{
		//TRACK_OBJECT(SIngressLog)
#ifdef DEF_USE_POOL
		FW::Map<uint64, SAccessInfo> Infos; // key: Process UID
#else
		std::map<uint64, SAccessInfo> Infos; // key: Process UID
#endif
	};

	void AddIngressActor(uint64 ProgramUID, const CFlexGuid& EnclaveGuid, const SProcessUID& ProcessUID, bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked);
	void StoreAllIngressActors(StVariantWriter& List, const CFlexGuid& EnclaveGuid, const SVarWriteOpt& Opts) const;
	bool LoadIngressActor(const StVariant& Data);

	void AddIngressTarget(uint64 ProgramUID, const CFlexGuid& EnclaveGuid, const SProcessUID& ProcessUID, bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked);
	void StoreAllIngressTargets(StVariantWriter& List, const CFlexGuid& EnclaveGuid, const SVarWriteOpt& Opts, const std::wstring& SvcTag = L"") const;
	bool LoadIngressTarget(const StVariant& Data);

	void Clear();

	void Truncate();

protected:
	mutable std::recursive_mutex	m_Mutex;

#ifdef DEF_USE_POOL
	struct SAccessLog : FW::Object
#else
	struct SAccessLog
#endif
	{
#ifdef DEF_USE_POOL
		SAccessLog(FW::AbstractMemPool* pMem) : FW::Object(pMem), ExecParents(pMem), ExecChildren(pMem), IngressActors(pMem), IngressTargets(pMem) {}

		FW::Map<uint64, SExecLog>		ExecParents; // key: Program UID
		FW::Map<uint64, SExecLog>		ExecChildren; // key: Program UID

		FW::Map<uint64, SIngressLog>	IngressActors; // key: Program UID
		FW::Map<uint64, SIngressLog>	IngressTargets; // key: Program UID
#else
		std::map<uint64, SExecLog>		ExecParents; // key: Program UID
		std::map<uint64, SExecLog>		ExecChildren; // key: Program UID

		std::map<uint64, SIngressLog>	IngressActors; // key: Program UID
		std::map<uint64, SIngressLog>	IngressTargets; // key: Program UID
#endif
	};

#ifdef DEF_USE_POOL
	typedef FW::SharedPtr<SAccessLog> SAccessLogPtr;
#else
	typedef std::shared_ptr<SAccessLog> SAccessLogPtr;
#endif

	SAccessLogPtr					m_Data;

#ifdef DEF_USE_POOL
	FW::AbstractMemPool*			m_pMem = nullptr;
#endif
};