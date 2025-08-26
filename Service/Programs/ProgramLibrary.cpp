#include "pch.h"
#include "ProgramLibrary.h"
#include "../../Library/API/PrivacyAPI.h"

CProgramLibrary::CProgramLibrary(const std::wstring& Path)
{
	static volatile LONG64 VolatileIdCounter = 0;

	m_UID = InterlockedIncrement64(&VolatileIdCounter);

	m_Path = Path;	
}

void CProgramLibrary::UpdateSignInfo(const struct SVerifierInfo* pVerifyInfo)
{
    std::unique_lock lock(m_Mutex);

    m_SignInfo.Update(pVerifyInfo);
}

void CProgramLibrary::UpdateSignInfo(const CImageSignInfo& Info)
{
    std::unique_lock lock(m_Mutex);

    if(Info.GetTimeStamp() > m_SignInfo.GetTimeStamp())
        m_SignInfo = Info;
}

StVariant CProgramLibrary::ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool) const
{
    std::unique_lock Lock(m_Mutex);

    StVariantWriter Data(pMemPool);
    if (Opts.Format == SVarWriteOpt::eIndex) {
        Data.BeginIndex();
        WriteIVariant(Data, Opts);
    } else {  
        Data.BeginMap();
        WriteMVariant(Data, Opts);
    }
    return Data.Finish();
}

NTSTATUS CProgramLibrary::FromVariant(const class StVariant& Data)
{
    std::unique_lock Lock(m_Mutex);

    if (Data.GetType() == VAR_TYPE_MAP)         StVariantReader(Data).ReadRawMap([&](const SVarName& Name, const StVariant& Data) { ReadMValue(Name, Data); });
    else if (Data.GetType() == VAR_TYPE_INDEX)  StVariantReader(Data).ReadRawIndex([&](uint32 Index, const StVariant& Data) { ReadIValue(Index, Data); });
    else
        return STATUS_UNKNOWN_REVISION;
    return STATUS_SUCCESS;
}
