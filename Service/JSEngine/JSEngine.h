#pragma once
#include "../Library/Status.h"
#include "../../Library/Common/StVariant.h"

class CJSEngine
{
public:
	CJSEngine(const std::string& Script = {});
	~CJSEngine();

	void SetScript(const std::string &Script);
	std::string GetScript() {std::unique_lock Lock(m_Mutex); return m_Script;}

	RESULT(StVariant) RunScript();

	RESULT(StVariant) CallFunc(const std::string &Name, int arg_count, ...);

protected:
	std::recursive_mutex m_Mutex;
	std::string m_Script;

	struct SJSEngine* m = nullptr;
};

typedef std::shared_ptr<CJSEngine> CJSEnginePtr;
typedef std::weak_ptr<CJSEngine> CJSEngineRef;