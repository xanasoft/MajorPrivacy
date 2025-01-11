#pragma once
#include "../Library/Common/Variant.h"
#include "../Library/API/PrivacyDefs.h"
#include "../Library/Common/FlexGuid.h"
#include "../Processes/Process.h"

class CAccessLog
{
	TRACK_OBJECT(CAccessLog)
public:
	CAccessLog();
	virtual ~CAccessLog();

	struct SExecInfo
	{
		TRACK_OBJECT(SExecInfo)
		uint64						LastExecTime = 0;
		CFlexGuid					EnclaveGuid;
		bool						bBlocked = false;
		std::wstring				CommandLine;
	};

	struct SExecLog
	{
		TRACK_OBJECT(SExecLog)
		std::map<uint64, SExecInfo> Infos; // key: Process UID
	};

	void AddExecParent(uint64 ProgramUID, const CFlexGuid& EnclaveGuid, const SProcessUID& ProcessUID, const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked);
	void StoreAllExecParents(CVariant& List, const CFlexGuid& EnclaveGuid, const SVarWriteOpt& Opts) const;
	bool LoadExecParent(const CVariant& Data);

	void AddExecChild(uint64 ProgramUID, const CFlexGuid& EnclaveGuid, const SProcessUID& ProcessUID, const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked);
	void StoreAllExecChildren(CVariant& List, const CFlexGuid& EnclaveGuid, const SVarWriteOpt& Opts, const std::wstring& SvcTag = L"") const;
	bool LoadExecChild(const CVariant& Data);


	struct SAccessInfo
	{
		TRACK_OBJECT(SAccessInfo)
		uint64						LastAccessTime = 0;
		CFlexGuid					EnclaveGuid;
		uint32						ProcessAccessMask = 0;
		uint32						ThreadAccessMask = 0;
		bool						bBlocked = false;
	};

	struct SIngressLog
	{
		TRACK_OBJECT(SIngressLog)
		std::map<uint64, SAccessInfo> Infos; // key: Process UID
	};

	void AddIngressActor(uint64 ProgramUID, const CFlexGuid& EnclaveGuid, const SProcessUID& ProcessUID, bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked);
	void StoreAllIngressActors(CVariant& List, const CFlexGuid& EnclaveGuid, const SVarWriteOpt& Opts) const;
	bool LoadIngressActor(const CVariant& Data);

	void AddIngressTarget(uint64 ProgramUID, const CFlexGuid& EnclaveGuid, const SProcessUID& ProcessUID, bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked);
	void StoreAllIngressTargets(CVariant& List, const CFlexGuid& EnclaveGuid, const SVarWriteOpt& Opts, const std::wstring& SvcTag = L"") const;
	bool LoadIngressTarget(const CVariant& Data);

	void Clear();

	void Truncate();

protected:
	mutable std::recursive_mutex	m_Mutex;

	std::map<uint64, SExecLog>		m_ExecParents; // key: Program UID
	std::map<uint64, SExecLog>		m_ExecChildren; // key: Program UID

	std::map<uint64, SIngressLog>	m_IngressActors; // key: Program UID
	std::map<uint64, SIngressLog>	m_IngressTargets; // key: Program UID
};