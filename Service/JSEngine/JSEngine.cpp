#include "pch.h"
#include <cstdarg>
#include "JSEngine.h"
#include "../../quickjspp/quickjspp.hpp"
#include "../../quickjspp/quickjs/quickjs-libc.h"
#include "../ServiceCore.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../Processes/ProcessList.h"

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

    void HandleException()
    {
        auto exc = context.getException();
        auto str = (std::string)exc;
        if((bool)exc["stack"])
            str += "\n" + (std::string)exc["stack"];
        
        theCore->Log()->LogEvent(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_JSLOG_MSG, s2w(str));
    }

	qjs::Runtime runtime;
	qjs::Context context;
};

//
// Script API
// 
// Resource Access Callback TestAccess(event)
//  event.NtPath            - the NT path of the resource
//  event.DosPath           - the DOS path of the resource
//  event.ActorPid          - the PID of the process that is accessing the resource
//  event.ActorServiceTag   - the service tag of the process that is accessing the resource
//  event.AccessMask        - the access mask of the access request
//
// Global Objects:
//  processList             - the process list object
//          getProcess(pid) - returns a process object for the given PID
// 
// Objects:
//  process 		        - the process object
//          pid             - the PID of the process
//          filePath        - the file path of the process
//          serviceTag      - the service tag of the process
//          enclaveGuid     - the enclave GUID of the process
// 
//

/*struct STest
{
    STest() {}
    ~STest() {}

    int test = 0;
};*/

CJSEngine::CJSEngine(const std::string& Script)
{
	m = new SJSEngine();
    
    /*
    js_std_init_handlers(m->runtime.rt);
    
    JS_SetModuleLoaderFunc(m->runtime.rt, nullptr, js_module_loader, nullptr);
    js_std_add_helpers(m->context.ctx, 0, nullptr);

    js_init_module_std(m->context.ctx, "std");
    js_init_module_os(m->context.ctx, "os");

    m->context.eval(R"xxx(
        import * as std from 'std';
        import * as os from 'os';
        globalThis.std = std;
        globalThis.os = os;
    )xxx", "<input>", JS_EVAL_TYPE_MODULE);
    */

    auto processList = m->context.newObject();

    auto getProcess = [&](int PID) -> qjs::Value {

        auto ctx = m->context.ctx;

		CProcessPtr pProcess = theCore->ProcessList()->GetProcess(PID, true);
        if(!pProcess)
            return qjs::Value {ctx, JS_NULL};

        qjs::Value obj = m->context.newObject();

		/*std::shared_ptr<STest> pTest = std::make_shared<STest>();

        obj["test"] = [ctx, pProcess, pTest]()-> qjs::Value {

            std::wstring test = pProcess->GetNtFilePath();
            test.empty();

            return qjs::Value {ctx, JS_NULL};
        };*/

        auto path = pProcess->GetNtFilePath();

        obj["pid"]  = PID;
        obj["ntFilePath"] = ToUtf8(nullptr, path.c_str()).ConstData();
        obj["filePath"] = ToUtf8(nullptr, theCore->NormalizePath(path).c_str()).ConstData();

        //auto meta = m->context.newObject();
        //meta["createdAt"] = std::time(nullptr);
        //obj["meta"] = meta;

        return obj;
    };

    processList["getProcess"] = getProcess;

    m->context.global()["processList"] = processList;

    //m->context.global()["println"] = [](const std::string& s) 
    //{ 
    //    std::cout << s << std::endl; 
    //};

    m->context.global()["logLine"] = [](const std::string& s) { theCore->Log()->LogEvent(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_JSLOG_MSG, s2w(s)); };
    m->context.global()["logSuccess"] = [](const std::string& s) { theCore->Log()->LogEvent(EVENTLOG_SUCCESS, 0, SVC_EVENT_JSLOG_MSG, s2w(s)); };
    m->context.global()["logWarning"] = [](const std::string& s) { theCore->Log()->LogEvent(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_JSLOG_MSG, s2w(s)); };
    m->context.global()["logError"] = [](const std::string& s) { theCore->Log()->LogEvent(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_JSLOG_MSG, s2w(s)); };

	if(!Script.empty())
		SetScript(Script);
}

CJSEngine::~CJSEngine()
{
	delete m;
}

void CJSEngine::SetScript(const std::string& script)
{
	std::unique_lock Lock(m_Mutex);

	m_Script = script;
}

RESULT(StVariant) CJSEngine::RunScript()
{
    std::unique_lock Lock(m_Mutex);

    try {
        auto Val = m->context.eval(m_Script);

        if (Val.v.tag == JS_TAG_EXCEPTION) {
            m->HandleException();
            return STATUS_UNSUCCESSFUL;
        }

        return m->JS2Var(Val);
    }
    catch (qjs::exception & ex) {
        m->HandleException();
        return STATUS_UNSUCCESSFUL;
    }
}

RESULT(StVariant) CJSEngine::CallFunc(const std::string& Name, int arg_count, ...)
{
    std::unique_lock Lock(m_Mutex);

    try {
        qjs::Value fn = m->context.global()[Name.c_str()];

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
            m->HandleException();
            return STATUS_UNSUCCESSFUL;
        }

        return m->JS2Var(retVal);
    }
    catch (qjs::exception& ex) {
        m->HandleException();
        return STATUS_UNSUCCESSFUL;
    }
}