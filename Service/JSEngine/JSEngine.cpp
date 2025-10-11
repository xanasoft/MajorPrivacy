#include "pch.h"
#include <cstdarg>
#include "JSEngine.h"
#include "../../quickjspp/quickjspp.hpp"
#include "../../quickjspp/quickjs/quickjs-libc.h"
#include "../ServiceCore.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../Processes/ProcessList.h"
#include "../Programs/ProgramLibrary.h"
#include "../../Library/Helpers/NtUtil.h"
#include "../../Library/API/DriverAPI.h"
#include <regex>

struct SJSEngine
{
	SJSEngine() : context(runtime)
	{
	}

    StVariant JS2Var(const qjs::Value& Value)
    {
        auto C = Value.ctx;

        // 1) null/undefined -> empty variant
        if (Value.v.tag == JS_TAG_NULL
            || Value.v.tag == JS_TAG_UNDEFINED
            || Value.v.tag == JS_TAG_UNINITIALIZED)
        {
            return StVariant();
        }

        // 2) primitives
        switch (Value.v.tag) {
        case JS_TAG_INT:
            return StVariant(Value.as<int>());
        case JS_TAG_BOOL:
            return StVariant(Value.as<bool>());
        case JS_TAG_FLOAT64:
            return StVariant(Value.as<double>());
        case JS_TAG_STRING:
            return StVariant(Value.as<std::string>().c_str());
        default:
            break;
        }

        // 3) arrays
        if (JS_IsArray(C, Value.v)) {
            qjs::Value arr{C, JS_DupValue(C, Value.v)};
            uint32_t len = static_cast<uint32_t>(arr["length"].as<int>());  
            // create a list-variant
            CVariant listVar(nullptr, VAR_TYPE_LIST);
            for (uint32_t i = 0; i < len; ++i) {
                qjs::Value elem{C, JS_GetPropertyUint32(C, Value.v, i)};
                StVariant child = JS2Var(elem);
                listVar.Append(CVariant(child));
            }
            return StVariant(listVar);
        }

        // 4) plain objects -> map
        if (Value.v.tag == JS_TAG_OBJECT) {
            // 4a) get own property names
            JSPropertyEnum *ptab;
            uint32_t plen;
            int res = JS_GetOwnPropertyNames(C, &ptab, &plen, Value.v, 0);
            if (res < 0)
                return StVariant();  // on error, return empty

            // 4b) build a map-variant
            CVariant mapVar(nullptr, VAR_TYPE_MAP);
            for (uint32_t i = 0; i < plen; ++i) {
                // convert atom ? C-string
                const char *name = JS_AtomToCString(C, ptab[i].atom);
                if (!name) {
                    // skip this property on OOM
                    continue;
                }

                // read the property value
                JSValue propVal = JS_GetProperty(C, Value.v, ptab[i].atom);

                qjs::Value retVal(std::move(propVal));
                retVal.ctx = context.ctx;

                StVariant child = JS2Var(retVal);

                // insert into our map
                mapVar.Insert(name, CVariant(child));

                // cleanup
                JS_FreeCString(C, name);
            }

            // 4c) free the enumeration table
            js_free_rt(JS_GetRuntime(C), ptab);

            return StVariant(mapVar);
        }

        // 5) everything else
        return StVariant();
    }

    qjs::Value Var2JS(const StVariant& Variant)
    {
        auto C = context.ctx;

        switch (Variant.GetType()) {
            // strings
        case VAR_TYPE_ASCII:
        case VAR_TYPE_UTF8:
        case VAR_TYPE_UNICODE: {
            auto s = Variant.ToStringA();
            return qjs::Value{C, JS_NewString(C, s.IsEmpty() ? "" : s.ConstData())};
        }

            // unsigned int
        case VAR_TYPE_UINT: {
            uint64_t u = Variant.To<uint64_t>();
            return qjs::Value{C, JS_NewInt64(C, static_cast<int64_t>(u))};
        }

            // signed int
        case VAR_TYPE_SINT: {
            int64_t i = Variant.To<int64_t>();
            return qjs::Value{C, JS_NewInt64(C, i)};
        }

            // double
        case VAR_TYPE_DOUBLE: {
            double d = Variant.To<double>();
            return qjs::Value{C, JS_NewFloat64(C, d)};
        }

            // lists
        case VAR_TYPE_LIST: {
            CVariant const& list = static_cast<CVariant const&>(Variant);
            uint32_t cnt = list.Count();
            qjs::Value arr{C, JS_NewArray(C)};
            for (uint32_t i = 0; i < cnt; ++i) {
                CVariant child = list.Get(i);
                qjs::Value v = Var2JS(child);
                JS_SetPropertyUint32(C, arr.v, i, v.release());
            }
            return arr;
        }

            // maps
        case VAR_TYPE_MAP: {
            CVariant const& map = static_cast<CVariant const&>(Variant);
            uint32_t cnt = map.Count();
            qjs::Value obj{C, JS_NewObject(C)};
            for (uint32_t i = 0; i < cnt; ++i) {
                const char* key = map.Key(i);
                CVariant child = map.Get(key);
                qjs::Value v = Var2JS(child);
                JS_SetPropertyStr(C, obj.v, key, v.release());
            }
            return obj;
        }

            // fallback ? null
        default:
            return qjs::Value{C, JS_NULL};
        }
    }

	qjs::Runtime runtime;
	qjs::Context context;

    qjs::Value MakeString(const std::wstring& str) 
    { 
        auto utf8 = ToUtf8(nullptr, str.c_str());
        qjs::Value jsStr{ context.ctx, JS_NewString(context.ctx, utf8.IsEmpty() ? "" : utf8.ConstData()) };
        return jsStr;
	}

    qjs::Value MakeProcessList();
    qjs::Value MakeLibrary(const CProgramLibraryPtr& pLibrary);
    qjs::Value MakeProcess(const CProcessPtr& pProcess) ;
};

//
// Script API
// 
// Resource Access Callback onAccess(event)
//  event.ntPath            - the NT path of the resource
//  event.dosPath           - the DOS path of the resource
//  event.actorPid          - the PID of the process that is accessing the resource
//  event.actorServiceTag   - the service tag of the process that is accessing the resource
//  event.accessMask        - the access mask of the access request
//
// Global Objects:
//  processList             - the process list object
//          getProcess(pid) - returns a process object for the given PID
//  	    getList()       - returns an array of all processes PIDs
// 
// Objects:
//  process 		        - the process object
//          pid             - the PID of the process
//          filePath        - the file path of the process
//          enclaveGuid     - the enclave GUID of the process
// 
//

/*struct STest
{
    STest() {}
    ~STest() {}

    int test = 0;
};*/

CJSEngine::CJSEngine(const std::string& Script, const CFlexGuid& Guid, EScriptTypes Type)
{
    m_Script = Script;
    m_Guid = Guid;
    m_Type = Type;

	m = new SJSEngine();
    
    //auto ctx = context.ctx;
    // 
    //std::shared_ptr<STest> pTest = std::make_shared<STest>();
    //
    //obj["test"] = [ctx, pProcess, pTest]()-> qjs::Value {
    //
    //std::wstring test = pProcess->GetNtFilePath();
    //test.empty();
    //
    //return qjs::Value {ctx, JS_NULL};
    //};

    //js_std_init_handlers(m->runtime.rt);
    //
    //JS_SetModuleLoaderFunc(m->runtime.rt, nullptr, js_module_loader, nullptr);
    //js_std_add_helpers(m->context.ctx, 0, nullptr);
    //
    //js_init_module_std(m->context.ctx, "std");
    //js_init_module_os(m->context.ctx, "os");
    //
    //m->context.eval(R"xxx(
    //    import * as std from 'std';
    //    import * as os from 'os';
    //    globalThis.std = std;
    //    globalThis.os = os;
    //)xxx", "<input>", JS_EVAL_TYPE_MODULE);
    
    //m->context.global()["println"] = [](const std::string& s) 
    //{ 
    //    std::cout << s << std::endl; 
    //};

    m->context.global()["processList"] = m->MakeProcessList();

    qjs::Value eSpawn = m->context.newObject();
    eSpawn["Undefined"] = EProgramOnSpawn::eUnknown;
    eSpawn["Allow"]     = EProgramOnSpawn::eAllow;
    eSpawn["Block"]     = EProgramOnSpawn::eBlock;
    eSpawn["Eject"]     = EProgramOnSpawn::eEject;
    m->context.global()["Spawn"] = eSpawn;

    qjs::Value eLoad = m->context.newObject();
    eLoad["Undefined"] = EImageOnLoad::eUnknown;
    eLoad["Allow"]     = EImageOnLoad::eAllow;
    eLoad["Block"]     = EImageOnLoad::eBlock;
    eLoad["AllowUntrusted"] = EImageOnLoad::eAllowUntrusted;
    m->context.global()["Load"] = eLoad;

    qjs::Value eAccess = m->context.newObject();
    eAccess["Undefined"]= EAccessRuleType::eNone;
    eAccess["Allow"]    = EAccessRuleType::eAllow;
    eAccess["AllowRO"]  = EAccessRuleType::eAllowRO;
    eAccess["Enum"]     = EAccessRuleType::eEnum;
    eAccess["Protect"]  = EAccessRuleType::eProtect;
    eAccess["Block"]    = EAccessRuleType::eBlock;
    eAccess["Ignore"]   = EAccessRuleType::eIgnore;
    m->context.global()["Access"] = eAccess;

    m->context.global()["log"] = [&](const std::string& s)          { Log(s); };
    m->context.global()["logInfo"] = [&](const std::string& s)      { Log(s, ELogLevels::eInfo); };
    m->context.global()["logSuccess"] = [&](const std::string& s)   { Log(s, ELogLevels::eSuccess); };
    m->context.global()["logWarning"] = [&](const std::string& s)   { Log(s, ELogLevels::eWarning); };
    m->context.global()["logError"] = [&](const std::string& s)     { Log(s, ELogLevels::eError); };

    m->context.global()["slogInfo"] = [&](const std::string& s)     { Log(s, ELogLevels::eInfo);    theCore->Log()->LogEvent(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_JSLOG_MSG, s2w(s)); };
    m->context.global()["slogSuccess"] = [&](const std::string& s)  { Log(s, ELogLevels::eSuccess); theCore->Log()->LogEvent(EVENTLOG_SUCCESS, 0, SVC_EVENT_JSLOG_MSG, s2w(s)); };
    m->context.global()["slogWarning"] = [&](const std::string& s)  { Log(s, ELogLevels::eWarning); theCore->Log()->LogEvent(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_JSLOG_MSG, s2w(s)); };
    m->context.global()["slogError"] = [&](const std::string& s)    { Log(s, ELogLevels::eError);   theCore->Log()->LogEvent(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_JSLOG_MSG, s2w(s)); };

    m->context.global()["emitInfo"] = [&](const std::string& s)     { Log(s, ELogLevels::eInfo);    EmitEvent(s, ELogLevels::eInfo); };
    m->context.global()["emitSuccess"] = [&](const std::string& s)  { Log(s, ELogLevels::eSuccess); EmitEvent(s, ELogLevels::eSuccess); };
    m->context.global()["emitWarning"] = [&](const std::string& s)  { Log(s, ELogLevels::eWarning); EmitEvent(s, ELogLevels::eWarning); };
    m->context.global()["emitError"] = [&](const std::string& s)    { Log(s, ELogLevels::eError);   EmitEvent(s, ELogLevels::eError); };

    //m->context.global()["dump"] = [&](const qjs::Value& v)          { Dump(v); };

    ClearLog();
}

CJSEngine::~CJSEngine()
{
	delete m;
}

RESULT(StVariant) CJSEngine::RunScript()
{
    std::unique_lock Lock(m_Mutex);

    try {
        auto Val = m->context.eval(m_Script);

        if (Val.v.tag == JS_TAG_EXCEPTION) {
            HandleException();
            return STATUS_UNSUCCESSFUL;
        }

        return m->JS2Var(Val);
    }
    catch (qjs::exception & ex) {
        HandleException();
        return STATUS_UNSUCCESSFUL;
    }
}

void CJSEngine::HandleException()
{
    auto exc = m->context.getException();
    auto str = (std::string)exc;
    if((bool)exc["stack"])
        str += "\n" + (std::string)exc["stack"];

	Log(str, ELogLevels::eError);

    theCore->Log()->LogEvent(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_JSLOG_MSG, s2w(str));
}

RESULT(StVariant) CJSEngine::CallFunc(const char* FuncName, int arg_count, ...)
{
    std::unique_lock Lock(m_Mutex);

    try {
        qjs::Value fn = m->context.global()[FuncName];

        if (fn.v.tag == JS_TAG_UNDEFINED)
            return STATUS_NOT_FOUND;

        std::vector<JSValue> jsArgs;
        jsArgs.reserve(arg_count);
        std::list<qjs::Value> jsTemp;

        va_list ap;
        va_start(ap, arg_count);
        for (int i = 0; i < arg_count; ++i) {
            StVariant var = va_arg(ap, StVariant);
            jsTemp.push_back(m->Var2JS(var));
            //auto test = jsTemp.back().toJSON();
            jsArgs.push_back(jsTemp.back().v);
        }
        va_end(ap);

        JSValue rawRet = JS_Call(m->context.ctx, fn.v, JS_UNDEFINED, arg_count, jsArgs.data());
        //auto test = rawRet.back().toJSON();

        qjs::Value retVal(std::move(rawRet));
        retVal.ctx = m->context.ctx;

        if (retVal.v.tag == JS_TAG_EXCEPTION) {
            HandleException();
            return STATUS_UNSUCCESSFUL;
        }

        return m->JS2Var(retVal);
    }
    catch (qjs::exception& ex) {
        HandleException();
        return STATUS_UNSUCCESSFUL;
    }
}

void CJSEngine::EmitEvent(const std::string& Message, ELogLevels Level)
{
    StVariant Data;
	Data[API_V_GUID] = m_Guid.ToVariant(false);
    Data[API_V_TYPE] = (uint32)m_Type;
	Data[API_V_DATA] = Message;
    theCore->EmitEvent(Level, ELogEventType::eLogScriptEvent, Data);
}

//void CJSEngine::Dump(const qjs::Value& Value)
//{
//    
//}

void CJSEngine::Log(const std::string& Message, ELogLevels Level)
{
    std::unique_lock Lock(m_Mutex);

    while (m_Log.Count() >= 100) {
        m_BaseID++;
        m_Log.erase(m_Log.begin());
    }

    SLogEntry Entry;
    Entry.TimeStamp = GetCurrentTimeAsFileTime();
	Entry.Level = Level;
    Entry.Message = Message;
	m_Log.Append(Entry);
}

void CJSEngine::ClearLog()
{
    std::unique_lock Lock(m_Mutex);

    m_BaseID = rand();
    m_Log.Clear();
}

StVariant CJSEngine::DumpLog(uint32 LastID, FW::AbstractMemPool* pMemPool)
{
    std::unique_lock Lock(m_Mutex);

    int Index = 0;
    if(LastID > m_BaseID && LastID <= m_BaseID + m_Log.Count())
		Index = LastID - m_BaseID;
    
    StVariantWriter Log(pMemPool);
    Log.BeginList();
    for (; Index < m_Log.Count(); Index++)
    {
        StVariantWriter Entry(pMemPool);
        Entry.BeginIndex();
        Entry.Write(API_V_ID,  m_BaseID + Index + 1);
		Entry.Write(API_V_TYPE, (uint32)m_Log[Index].Level);
		Entry.Write(API_V_TIME_STAMP, m_Log[Index].TimeStamp);
		Entry.WriteEx(API_V_DATA, m_Log[Index].Message);
        Log.WriteVariant(Entry.Finish());
    }
	return Log.Finish();
}

FW::StringW CJSEngine__DumpHash(uint32 Algorithm, const std::vector<uint8>& Hash)
{
    FW::StringW Hex;
    ToHex(Hex, Hash.data(), Hash.size());

    switch (Algorithm)
    {
    case KphHashAlgorithmSha1:              return FW::StringW(nullptr, L"SHA1:") + Hex;
    case KphHashAlgorithmSha1Authenticode:  return FW::StringW(nullptr, L"SHA1ac:") + Hex;
    case KphHashAlgorithmSha256:            return FW::StringW(nullptr, L"SHA256:") + Hex;
    case KphHashAlgorithmSha256Authenticode:return FW::StringW(nullptr, L"SHA256ac:") + Hex;
    case KphHashAlgorithmSha384:            return FW::StringW(nullptr, L"SHA384:") + Hex;
    case KphHashAlgorithmSha512:            return FW::StringW(nullptr, L"SHA512:") + Hex;
    default: return Hex;
    }
}

StVariant CJSEngine__SumpSign(USignatures uSign)
{
    StVariant Sign;

    Sign["value"] = uSign.Value;
	Sign["windows"] = uSign.Windows != 0;
	Sign["microsoft"] = uSign.Microsoft != 0;
	Sign["antimalware"] = uSign.Antimalware != 0;
	Sign["authenticode"] = uSign.Authenticode != 0;
	Sign["ms_store"] = uSign.Store != 0;
	Sign["mp_dev_sign"] = uSign.Developer != 0;
    Sign["user_sign"] = uSign.UserSign != 0;
	Sign["user"] = uSign.UserDB != 0;
	Sign["enclave"] = uSign.EnclaveDB != 0;
    Sign["collection"] = uSign.Collection != 0;

	return Sign;
}

StVariant CJSEngine__FillVerifierInfo(const struct SVerifierInfo* pVerifyInfo)
{
    StVariant Verifier;

    if (!pVerifyInfo)
        return Verifier;

    StVariant Status(nullptr, VAR_TYPE_MAP);
    Status["signAuthority"] = (pVerifyInfo->StatusFlags & MP_VERIFY_FLAG_SA) != 0;
    Status["codeIntegrity"] = (pVerifyInfo->StatusFlags & MP_VERIFY_FLAG_CI) != 0;
    Status["signLevel"]     = (pVerifyInfo->StatusFlags & MP_VERIFY_FLAG_SL) != 0;

    Status["imageIncoherent"] = (pVerifyInfo->StatusFlags & MP_VERIFY_FLAG_INCOHERENT) != 0;

    Status["fileMismatch"]  = (pVerifyInfo->StatusFlags & MP_VERIFY_FLAG_FILE_MISMATCH) != 0;
	Status["hashFailed"]    = (pVerifyInfo->StatusFlags & MP_VERIFY_FLAG_HASH_FILED) != 0;
    Status["coherencyFail"] = (pVerifyInfo->StatusFlags & MP_VERIFY_FLAG_COHERENCY_FAIL) != 0;
    Status["signatureFail"] = (pVerifyInfo->StatusFlags & MP_VERIFY_FLAG_SIGNATURE_FAIL) != 0;

    Verifier["statusFlags"] = Status;

	Verifier["signAuthority"] = (uint32)pVerifyInfo->PrivateAuthority;
	Verifier["signLevel"] = (uint32)pVerifyInfo->SignLevel;
	Verifier["codeIntegrity"] = (uint32)pVerifyInfo->SignPolicyBits;

    if(pVerifyInfo->FileHashAlgorithm)
	    Verifier["fileHash"] = CJSEngine__DumpHash(pVerifyInfo->FileHashAlgorithm, pVerifyInfo->FileHash);

    if (pVerifyInfo->SignerHashAlgorithm) {
        Verifier["signerHash"] = CJSEngine__DumpHash(pVerifyInfo->SignerHashAlgorithm, pVerifyInfo->SignerHash);
        Verifier["signerName"] = pVerifyInfo->SignerName;
    }

    if (pVerifyInfo->IssuerHashAlgorithm) {
        Verifier["issuerHash"] = CJSEngine__DumpHash(pVerifyInfo->IssuerHashAlgorithm, pVerifyInfo->IssuerHash);
        Verifier["issuerName"] = pVerifyInfo->IssuerName;
    }

	Verifier["foundSignatures"] = CJSEngine__SumpSign(pVerifyInfo->FoundSignatures);
	Verifier["allowedSignatures"] = CJSEngine__SumpSign(pVerifyInfo->AllowedSignatures);

    return Verifier;
}

qjs::Value SJSEngine::MakeLibrary(const CProgramLibraryPtr& pLibrary)
{
    qjs::Value obj = context.newObject();

    auto path = pLibrary->GetPath();

    obj["ntPath"] = MakeString(path.c_str());
    obj["dosPath"] = MakeString(theCore->NormalizePath(path));

    SVarWriteOpt Opts;
    obj["signerInfo"] = Var2JS(pLibrary->GetSignInfo().ToVariant(Opts));

    return obj;
};

qjs::Value SJSEngine::MakeProcess(const CProcessPtr& pProcess) 
{
    qjs::Value obj = context.newObject();

    auto path = pProcess->GetNtFilePath();

    obj["pid"] = (int64_t)pProcess->GetProcessId();
    obj["parentPid"] = (int64_t)pProcess->GetParentId();
    obj["creatorPid"] = (int64_t)pProcess->GetCreatorId();
    obj["name"] = MakeString(pProcess->GetName());
    obj["ntPath"] = MakeString(path);
    obj["dosPath"] = MakeString(theCore->NormalizePath(path));
    obj["workDir"] = MakeString(pProcess->GetWorkDir());

    obj["enclaveGuid"] = pProcess->GetEnclave().ToString();

    obj["appContainerSid"] = MakeString(pProcess->GetAppContainerSid());
    obj["appContainerName"] = MakeString(pProcess->GetAppContainerName());
    obj["appPackageFullName"] = MakeString(pProcess->GetAppPackageName());

    auto Services = pProcess->GetServices();
    if (!Services.empty())
    {
        qjs::Value arr{ context.ctx, JS_NewArray(context.ctx) };
        uint32_t idx = 0;
        for (auto& I : Services)
        {
            JS_SetPropertyUint32(context.ctx, arr.v, idx++, MakeString(I));
        }
        obj["services"] = arr;
    }

#if 0
    auto Libraries = pProcess->GetLibraries();
    if (!Libraries.empty())
    {
        qjs::Value arr{ context.ctx, JS_NewArray(context.ctx) };
        uint32_t idx = 0;
        for (auto& I : Libraries)
        {
            qjs::Value lib = MakeLibrary(I);
            JS_SetPropertyUint32(context.ctx, arr.v, idx++, lib.release());
        }
        obj["libraries"] = arr;
    }
#else
    auto getLibraries = [&]() -> qjs::Value {

        auto Libraries = pProcess->GetLibraries();

        qjs::Value arr = context.newObject();
        uint32_t idx = 0;
        for (auto& I : Libraries)
        {
            qjs::Value lib = MakeLibrary(I);
            JS_SetPropertyUint32(context.ctx, arr.v, idx++, lib.release());
        }

        return arr;
    };
    obj["getLibraries"] = getLibraries;
#endif

    SVarWriteOpt Opts;
    obj["signerInfo"] = Var2JS(pProcess->GetSignInfo().ToVariant(Opts));

    //auto meta = context.newObject();
    //meta["createdAt"] = std::time(nullptr);
    //obj["meta"] = meta;

    return obj;
};

qjs::Value SJSEngine::MakeProcessList()
{
    auto processList = context.newObject();

    auto getList = [&]() -> qjs::Value {

        qjs::Value arr{ context.ctx, JS_NewArray(context.ctx) };
        uint32_t idx = 0;

        for (auto& I : theCore->ProcessList()->List())
        {
            JS_SetPropertyUint32(context.ctx, arr.v, idx++, JS_NewInt64(context.ctx, (int64_t)I.second->GetProcessId()));
        }

        return arr;
    };
    processList["getList"] = getList;

    auto getAll = [&]() -> qjs::Value {

        qjs::Value arr{ context.ctx, JS_NewArray(context.ctx) };
        uint32_t idx = 0;

        for (auto& I : theCore->ProcessList()->List())
        {
            qjs::Value obj = MakeProcess(I.second);

            JS_SetPropertyUint32(context.ctx, arr.v, idx++, obj.release());
        }

        return arr;
    };
    processList["getAll"] = getAll;

    auto findByName = [&](const std::string& NameUtf8) -> qjs::Value {

        qjs::Value arr{ context.ctx, JS_NewArray(context.ctx) };

        // Build regex from DOS pattern (escape then translate * and ?)
        std::wstring pattern = FromUtf8(nullptr, NameUtf8.c_str()).ConstData();

        try {

            std::wstring rx = pattern;
            static const std::wregex specialChars(L"[.^$|()\\[\\]{}*+?\\\\]");
            rx = std::regex_replace(rx, specialChars, L"\\$&");      // escape regex meta
            static const std::wregex asterisk(L"\\\\\\*");
            rx = std::regex_replace(rx, asterisk, L".*");            // * -> .*
            static const std::wregex questionMark(L"\\\\\\?");
            rx = std::regex_replace(rx, questionMark, L".");         // ? -> .

            const std::wregex re(L"^" + rx + L"$", std::regex_constants::icase);

            uint32_t idx = 0;
            for (auto& it : theCore->ProcessList()->List()) {
                const CProcessPtr& pProcess = it.second;
                std::wstring DosPath = theCore->NormalizePath(pProcess->GetNtFilePath());
                if (std::regex_search(DosPath, re)) {
                    qjs::Value obj = MakeProcess(pProcess);
                    JS_SetPropertyUint32(context.ctx, arr.v, idx++, obj.release());
                    //JS_SetPropertyUint32(ctx, arr.v, idx++, JS_NewInt64(ctx, (int64_t)pProcess->GetProcessId()));
                }
            }
        }
        catch (...) {
            // On any regex error, just return empty array
        }

        return arr;
    };
    processList["findByName"] = findByName;

    auto getProcess = [&](int PID) -> qjs::Value {

        CProcessPtr pProcess = theCore->ProcessList()->GetProcessEx(PID, CProcessList::eCanAdd);
        if (!pProcess)
            return qjs::Value{ context.ctx, JS_NULL };

        return MakeProcess(pProcess);
    };
    processList["getProcess"] = getProcess;

    return processList;
}

EProgramOnSpawn CJSEngine::RunStartScript(const std::wstring& NtPath, const std::wstring& CommandLine, uint64 ActorPid, const std::wstring& ActorServiceTag, const std::wstring& EnclaveId, bool bTrusted, const struct SVerifierInfo* pVerifierInfo)
{
    //std::unique_lock Lock(m_Mutex);

    StVariant Event;
    Event["ntPath"] = NtPath;
    Event["dosPath"] = theCore->NormalizePath(NtPath);
    Event["commandLine"] = CommandLine;
    Event["actorPid"] = ActorPid;
    Event["actorServiceTag"] = ActorServiceTag;
    Event["enclaveId"] = EnclaveId;
    Event["trusted"] = bTrusted;
    Event["ci"] = CJSEngine__FillVerifierInfo(pVerifierInfo);

    auto Ret = CallFunc("onStart", 1, Event);
    if(Ret.IsError())
        return EProgramOnSpawn::eUnknown;
    EProgramOnSpawn Action = (EProgramOnSpawn)Ret.GetValue().To<int>();
    return Action;
}

EImageOnLoad CJSEngine::RunLoadScript(uint64 Pid, const std::wstring& NtPath, const std::wstring& EnclaveId, bool bTrusted, const struct SVerifierInfo* pVerifierInfo)
{
    //std::unique_lock Lock(m_Mutex);

    StVariant Event;
    Event["pid"] = Pid;
    Event["ntPath"] = NtPath;
    Event["dosPath"] = theCore->NormalizePath(NtPath);
    Event["enclaveId"] = EnclaveId;
    Event["trusted"] = bTrusted;
    Event["ci"] = CJSEngine__FillVerifierInfo(pVerifierInfo);
    
    auto Ret = CallFunc("onLoad", 1, Event);
    if(Ret.IsError())
        return EImageOnLoad::eUnknown;
    EImageOnLoad Action = (EImageOnLoad)Ret.GetValue().To<int>();
    return Action;
}

EAccessRuleType CJSEngine::RunAccessScript(const std::wstring& NtPath, uint64 ActorPid, const std::wstring& ActorServiceTag, const std::wstring& EnclaveId, uint32 AccessMask)
{
    //std::unique_lock Lock(m_Mutex);

    StVariant Event;
    Event["ntPath"] = NtPath;
    Event["dosPath"] = theCore->NormalizePath(NtPath);
    Event["actorPid"] = ActorPid;
    Event["actorServiceTag"] = ActorServiceTag;
    Event["enclaveId"] = EnclaveId;
    Event["accessMask"] = AccessMask;

    auto Ret = CallFunc("onAccess", 1, Event);
    if(Ret.IsError())
        return EAccessRuleType::eNone;
    EAccessRuleType Action = (EAccessRuleType)Ret.GetValue().To<int>();
    return Action;
}

bool CJSEngine::RunMountScript(const std::wstring& ImagePath, const std::wstring& DevicePath, const std::wstring& MountPoint)
{
    StVariant Data;
	Data["imagePath"] = ImagePath;
	Data["devicePath"] = DevicePath;
    Data["mountPoint"] = MountPoint;
    auto Ret = CallFunc("onMount", 1, Data);
    if (Ret.IsError()) {
        if(Ret.GetStatus() == STATUS_NOT_FOUND)
			return true; // allow if no function
		return false; // block on error
    }
    if(!Ret.GetValue().IsValid())
		return true; // allow if no return value
	return Ret.GetValue().To<bool>(); // allow/block
}

void CJSEngine::RunDismountScript(const std::wstring& ImagePath, const std::wstring& DevicePath, const std::wstring& MountPoint)
{
    StVariant Data;
    Data["imagePath"] = ImagePath;
    Data["devicePath"] = DevicePath;
    Data["mountPoint"] = MountPoint;
	CallFunc("onDismount", 1, Data);
}