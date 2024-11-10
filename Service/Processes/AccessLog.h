#pragma once
#include "../Library/Common/Variant.h"
#include "../Library/API/PrivacyDefs.h"

class CAccessLog
{
public:
	CAccessLog();
	virtual ~CAccessLog();

	struct SExecInfo
	{
		TRACK_OBJECT(SExecInfo)
		uint64						LastExecTime = 0;
		bool						bBlocked = false;
		std::wstring				CommandLine;
	};

	void AddExecActor(uint64 UID, const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked);
	CVariant StoreExecActors(const SVarWriteOpt& Opts) const;
	void LoadExecActors(const CVariant& Data);
	void DumpExecActors(CVariant& Actors, const SVarWriteOpt& Opts) const;

	void AddExecTarget(uint64 UID, const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked);
	//std::map<uint64, CAccessLog::SExecInfo> GetExecTargets() const { return m_ExecTargets; }
	CVariant StoreExecTargets(const SVarWriteOpt& Opts) const;
	void LoadExecTargets(const CVariant& Data);
	void DumpExecTarget(CVariant& Targets, const SVarWriteOpt& Opts, const std::wstring& SvcTag = L"") const;


	struct SAccessInfo
	{
		TRACK_OBJECT(SAccessInfo)
		uint64						LastAccessTime = 0;
		bool						bBlocked = false;
		uint32						ProcessAccessMask = 0;
		uint32						ThreadAccessMask = 0;
	};

	void AddIngressActor(uint64 UID, bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked);
	CVariant StoreIngressActors(const SVarWriteOpt& Opts) const;
	void LoadIngressActors(const CVariant& Data);
	void DumpIngressActors(CVariant& Actors, const SVarWriteOpt& Opts) const;

	void AddIngressTarget(uint64 UID, bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked);
	//std::map<uint64, CAccessLog::SAccessInfo> GetIngressTargets() const { return m_IngressTargets; }
	CVariant StoreIngressTargets(const SVarWriteOpt& Opts) const;
	void LoadIngressTargets(const CVariant& Data);
	void DumpIngressTargets(CVariant& Targets, const SVarWriteOpt& Opts, const std::wstring& SvcTag = L"") const;


	void Clear();

protected:
	std::map<uint64, SExecInfo>		m_ExecActors; // key: Program UUID
	std::map<uint64, SExecInfo>		m_ExecTargets; // key: Program UUID

	std::map<uint64, SAccessInfo>	m_IngressActors; // key: Program UUID
	std::map<uint64, SAccessInfo>	m_IngressTargets; // key: Program UUID
};