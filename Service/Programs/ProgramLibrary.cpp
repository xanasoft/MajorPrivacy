#include "pch.h"
#include "ProgramLibrary.h"
#include "../../Library/API/PrivacyAPI.h"

CProgramLibrary::CProgramLibrary(const std::wstring& Path)
{
	static volatile LONG64 VolatileIdCounter = 0;

	m_UID = InterlockedIncrement64(&VolatileIdCounter);

	m_Path = Path;	
}

CVariant CProgramLibrary::ToVariant(const SVarWriteOpt& Opts) const
{
    std::unique_lock Lock(m_Mutex);

    CVariant Data;
    if (Opts.Format == SVarWriteOpt::eIndex) {
        Data.BeginIMap();
        WriteIVariant(Data, Opts);
    } else {  
        Data.BeginMap();
        WriteMVariant(Data, Opts);
    }
    Data.Finish();
    return Data;
}

NTSTATUS CProgramLibrary::FromVariant(const class CVariant& Data)
{
    std::unique_lock Lock(m_Mutex);

    if (Data.GetType() == VAR_TYPE_MAP)         Data.ReadRawMap([&](const SVarName& Name, const CVariant& Data) { ReadMValue(Name, Data); });
    else if (Data.GetType() == VAR_TYPE_INDEX)  Data.ReadRawIMap([&](uint32 Index, const CVariant& Data)        { ReadIValue(Index, Data); });
    else
        return STATUS_UNKNOWN_REVISION;
    return STATUS_SUCCESS;
}
