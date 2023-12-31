#include "pch.h"
#include "SchdUtil.h"

#include <windows.h>
#include <taskschd.h>
#include <comdef.h>
#include <vector>
#include <string>
#include <iostream>

#pragma comment(lib, "taskschd.lib")

std::vector<std::wstring> EnumTasks(const std::wstring& path)
{
    std::vector<std::wstring> list;
    HRESULT hr;

    ITaskService* pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)&pService);
    if (SUCCEEDED(hr))
    {
        hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
        if (SUCCEEDED(hr))
        {
            ITaskFolder* pRootFolder = NULL;
            hr = pService->GetFolder(_bstr_t(path.c_str()), &pRootFolder);
            if (SUCCEEDED(hr))
            {
                IRegisteredTaskCollection* pTaskCollection = NULL;
                hr = pRootFolder->GetTasks(TASK_ENUM_HIDDEN, &pTaskCollection);
                if (SUCCEEDED(hr))
                {
                    LONG numTasks = 0;
                    hr = pTaskCollection->get_Count(&numTasks);
                    if (SUCCEEDED(hr))
                    {
                        for (LONG i = 0; i < numTasks; i++)
                        {
                            IRegisteredTask* pRegisteredTask = NULL;
                            hr = pTaskCollection->get_Item(_variant_t(i + 1), &pRegisteredTask);
                            if (SUCCEEDED(hr))
                            {
                                BSTR bstrTaskName;
                                hr = pRegisteredTask->get_Name(&bstrTaskName);
                                if (SUCCEEDED(hr))
                                {
                                    list.push_back(std::wstring(bstrTaskName, SysStringLen(bstrTaskName)));
                                    SysFreeString(bstrTaskName);
                                }
                                pRegisteredTask->Release();
                            }
                        }
                    }
                    pTaskCollection->Release();
                }
                pRootFolder->Release();
            }
        }
        pService->Release();
    }

    return list;
}

bool IsTaskEnabled(const std::wstring& path, const std::wstring& taskName)
{
    VARIANT_BOOL isEnabled = VARIANT_FALSE;
    HRESULT hr;

    ITaskService* pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)&pService);
    if (SUCCEEDED(hr))
    {
        hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
        if (SUCCEEDED(hr))
        {
            ITaskFolder* pFolder = NULL;
            hr = pService->GetFolder(_bstr_t(path.c_str()), &pFolder);
            if (SUCCEEDED(hr))
            {
                IRegisteredTask* pTask = NULL;
                hr = pFolder->GetTask(_bstr_t(taskName.c_str()), &pTask);
                if (SUCCEEDED(hr))
                {
                    hr = pTask->get_Enabled(&isEnabled);

                    pTask->Release();
                }
                pFolder->Release();
            }
        }
        pService->Release();
    }

    return isEnabled != VARIANT_FALSE;
}

bool SetTaskEnabled(const std::wstring& path, const std::wstring& taskName, bool setValue)
{
    HRESULT hr;

    ITaskService* pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)&pService);
    if (SUCCEEDED(hr))
    {
        hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
        if (SUCCEEDED(hr))
        {
            ITaskFolder* pFolder = NULL;
            hr = pService->GetFolder(_bstr_t(path.c_str()), &pFolder);
            if (SUCCEEDED(hr))
            {
                IRegisteredTask* pTask = NULL;
                hr = pFolder->GetTask(_bstr_t(taskName.c_str()), &pTask);
                if (SUCCEEDED(hr))
                {
                    hr = pTask->put_Enabled(setValue ? VARIANT_TRUE : VARIANT_FALSE);
                    pTask->Release();
                }
                pFolder->Release();
            }
        }
        pService->Release();
    }
    
    return SUCCEEDED(hr);
}

DWORD GetTaskStateKnown(const std::wstring& path, const std::wstring& taskName)
{
    TASK_STATE taskState = TASK_STATE_UNKNOWN;
    HRESULT hr;

    ITaskService* pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)&pService);
    if (SUCCEEDED(hr))
    {
        hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
        if (SUCCEEDED(hr))
        {
            ITaskFolder* pFolder = NULL;
            hr = pService->GetFolder(_bstr_t(path.c_str()), &pFolder);
            if (SUCCEEDED(hr))
            {
                IRegisteredTask* pTask = NULL;
                hr = pFolder->GetTask(_bstr_t(taskName.c_str()), &pTask);
                if (SUCCEEDED(hr))
                {
                    hr = pTask->get_State(&taskState);
                    pTask->Release();
                }
                pFolder->Release();
            }
        }
        pService->Release();
    }

    return taskState;
}