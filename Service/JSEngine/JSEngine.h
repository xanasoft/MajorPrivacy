#pragma once
#include "../Library/Status.h"
#include "../../Library/Common/StVariant.h"
#include "../../Library/API/PrivacyDefs.h"
#include "../Library/Common/FlexGuid.h"

class CJSEngine
{
public:
	CJSEngine(const std::string& Script, const CFlexGuid& Guid = CFlexGuid(), EScriptTypes Type = EScriptTypes::eNone);
	~CJSEngine();

	RESULT(StVariant) RunScript();

	RESULT(StVariant) CallFunc(const char* FuncName, int arg_count, ...);

	EProgramOnSpawn RunStartScript(const std::wstring& NtPath, const std::wstring& CommandLine, uint64 ActorPid, const std::wstring& ActorServiceTag, const std::wstring& EnclaveId, bool bTrusted, const struct SVerifierInfo* pVerifierInfo);
	
	EImageOnLoad RunLoadScript(uint64 Pid, const std::wstring& NtPath, const std::wstring& EnclaveId, bool bTrusted, const struct SVerifierInfo* pVerifierInfo);

	EAccessRuleType RunAccessScript(const std::wstring& NtPath, uint64 ActorPid, const std::wstring& ActorServiceTag, const std::wstring& EnclaveId, uint32 AccessMask);

	bool RunMountScript(const std::wstring& ImagePath, const std::wstring& DevicePath, const std::wstring& MountPoint);
	void RunDismountScript(const std::wstring& ImagePath, const std::wstring& DevicePath, const std::wstring& MountPoint);

	void ClearLog();
	StVariant DumpLog(uint32 LastID = 0, FW::AbstractMemPool* pMemPool = nullptr);

protected:
	mutable std::recursive_mutex m_Mutex;
	std::string m_Script;
	CFlexGuid m_Guid; 
	EScriptTypes m_Type = EScriptTypes::eNone;

	void HandleException();

	void EmitEvent(const std::string& Message, ELogLevels Level = ELogLevels::eNone);

	//void Dump(const qjs::Value& Value);

	void Log(const std::string& Message, ELogLevels Level = ELogLevels::eNone);

	struct SJSEngine* m = nullptr;

	uint32 m_BaseID = 0;
	struct SLogEntry
	{
		uint64 TimeStamp;
		ELogLevels Level = ELogLevels::eNone;
		std::string Message;
	};
	FW::Array<SLogEntry> m_Log;
};

typedef std::shared_ptr<CJSEngine> CJSEnginePtr;
typedef std::weak_ptr<CJSEngine> CJSEngineRef;