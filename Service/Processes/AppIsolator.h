#pragma once
#include "../Library/Status.h"
#include "Process.h"

class CAppIsolator
{
public:
	CAppIsolator();
	~CAppIsolator();

	STATUS Init();

	bool AllowProcessStart(const CProcessPtr& pProcess);

protected:
};