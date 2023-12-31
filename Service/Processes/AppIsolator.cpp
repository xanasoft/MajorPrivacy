#include "pch.h"
#include "AppIsolator.h"
#include "ProcessList.h"


CAppIsolator::CAppIsolator()
{
}

CAppIsolator::~CAppIsolator()
{
}


STATUS CAppIsolator::Init()
{

	return OK;
}

bool CAppIsolator::AllowProcessStart(const CProcessPtr& pProcess)
{
    bool bOk = true;
    
    //
    // TODO: make decision !!!!
    //

    return bOk;
}