#pragma once
#include "../Library/Status.h"
#include "../../Library/Common/StVariant.h"
#include "../../Library/API/PrivacyDefs.h"
#include "../Library/Common/FlexGuid.h"

class CJSEngine
{
public:
	CJSEngine(const std::string& Script, const CFlexGuid& Guid = CFlexGuid(), EItemType Type = EItemType::eUnknown);
	~CJSEngine();

	RESULT(StVariant) RunScript();

	RESULT(StVariant) CallFunction(const char* FuncName, const CVariant& Param, uint32 CallerPID = 0);

	EProgramOnSpawn RunStartScript(const std::wstring& NtPath, const std::wstring& CommandLine, uint64 ActorPid, const std::wstring& ActorServiceTag, const std::wstring& EnclaveId, bool bTrusted, const struct SVerifierInfo* pVerifierInfo);
	
	EImageOnLoad RunLoadScript(uint64 Pid, const std::wstring& NtPath, const std::wstring& EnclaveId, bool bTrusted, const struct SVerifierInfo* pVerifierInfo);

	EAccessRuleType RunAccessScript(const std::wstring& NtPath, uint64 ActorPid, const std::wstring& ActorServiceTag, const std::wstring& EnclaveId, uint32 AccessMask);

	bool RunMountScript(const std::wstring& ImagePath, const std::wstring& DevicePath, const std::wstring& MountPoint);
	void RunDismountScript(const std::wstring& ImagePath, const std::wstring& DevicePath, const std::wstring& MountPoint);

	void RunActivationScript(uint32 CallerPID);
	void RunDeactivationScript(uint32 CallerPID);

	void ClearLog();
	StVariant DumpLog(uint32 LastID = 0, FW::AbstractMemPool* pMemPool = nullptr);

protected:
	mutable std::recursive_mutex m_Mutex;
	std::string m_Script;
	CFlexGuid m_Guid; 
	EItemType m_Type = EItemType::eUnknown;

	RESULT(StVariant) CallFunc(const char* FuncName, int arg_count, ...);

	void HandleException();

	void EmitEvent(const std::string& Message, ELogLevels Level = ELogLevels::eNone);

	//void Dump(const qjs::Value& Value);

	void Log(const std::string& Message, ELogLevels Level = ELogLevels::eNone);

	struct SJSEngine* m = nullptr;

	uint32 m_CallerPID = 0;

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