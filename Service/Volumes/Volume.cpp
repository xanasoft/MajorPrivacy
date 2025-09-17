#include "pch.h"
#include "Volume.h"
#include "../Library/API/PrivacyAPI.h"
#include "../../Library/Crypto/HashFunction.h"

CVolume::CVolume()
{
}

CVolume::~CVolume()
{
}

void CVolume::Update(const std::shared_ptr<CVolume>& pVolume)
{
    std::unique_lock Lock(m_Mutex);
    std::shared_lock Lock2(pVolume->m_Mutex);
    
    m_bUseScript = pVolume->m_bUseScript;
    m_Script = pVolume->m_Script;
    UpdateScript_NoLock();
    
    m_Data = pVolume->m_Data;
	
    m_bDataDirty = true;
}

void CVolume::SetImageDosPath(const std::wstring& ImageDosPath) 
{ 
	std::unique_lock Lock(m_Mutex); 

	m_ImageDosPath = ImageDosPath; 

    m_Guid = GetGuidFromPath(ImageDosPath);
}

CFlexGuid CVolume::GetGuidFromPath(std::wstring Path)
{
    CFlexGuid FlexGuid;
    Path = MkLower(Path);

    CHashFunction hasher;
    if (NT_SUCCESS(hasher.InitHash())) 
    {
        CBuffer chunk((uchar*)Path.c_str(), (ULONG)(Path.length() * sizeof(wchar_t)), true);
        hasher.UpdateHash(chunk);

        CBuffer Hash;
        if (NT_SUCCESS(hasher.FinalizeHash(Hash))) {
            SGuid Guid = {0};
            ((__int64*)&Guid)[0] = ((__int64*)Hash.GetBuffer())[0] ^ ((__int64*)Hash.GetBuffer())[2];
            ((__int64*)&Guid)[1] = ((__int64*)Hash.GetBuffer())[1] ^ ((__int64*)Hash.GetBuffer())[3];
            FlexGuid.SetRegularGuid(&Guid);
        }
    }

    return FlexGuid;
}

void CVolume::UpdateScript_NoLock()
{
    m_pScript.reset();

    if (!m_Script.empty()) 
    {
        m_pScript = std::make_shared<CJSEngine>(m_Script, m_Guid, EScriptTypes::eVolume);
        m_pScript->RunScript();
    }
}

void CVolume::SetScript(const std::string& Script) 
{
    std::unique_lock Lock(m_Mutex); 
    
    m_Script = Script; 

    UpdateScript_NoLock();
}

bool CVolume::HasScript() const
{
    std::shared_lock Lock(m_Mutex);

    return m_bUseScript && !!m_pScript.get();
}

CJSEnginePtr CVolume::GetScriptEngine() const 
{ 
    std::shared_lock Lock(m_Mutex); 

    return m_pScript; 
}


StVariant CVolume::ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool) const
{
	std::shared_lock Lock(m_Mutex);

	StVariantWriter Volume(pMemPool);
	Volume.BeginIndex();

    Volume.WriteVariant(API_V_GUID, m_Guid.ToVariant(false));
	Volume.Write(API_V_VOL_REF, (uint64)this);
	Volume.WriteEx(API_V_VOL_PATH, m_ImageDosPath);
	Volume.WriteEx(API_V_VOL_DEVICE_PATH, m_DevicePath);
	Volume.WriteEx(API_V_VOL_MOUNT_POINT, m_MountPoint);
	Volume.Write(API_V_VOL_SIZE, m_VolumeSize);
	Volume.Write(API_V_USE_SCRIPT, m_bUseScript);
	Volume.WriteEx(API_V_SCRIPT, m_Script);

	return Volume.Finish();
}

NTSTATUS CVolume::FromVariant(const StVariant& Rule)
{
    std::unique_lock Lock(m_Mutex);

    return StVariantReader(Rule).ReadRawIndex([&](uint32 Index, const StVariant& Data) {
        switch (Index)
        {
        case API_V_GUID:			m_Guid.FromVariant(Data); break;
        case API_V_VOL_PATH:		SetImageDosPath(Data.AsStr()); break;
        case API_V_VOL_DEVICE_PATH:	m_DevicePath = Data.AsStr(); break;
        case API_V_VOL_MOUNT_POINT:	m_MountPoint = Data.AsStr(); break;
        case API_V_VOL_SIZE:		m_VolumeSize = Data; break;
        case API_V_USE_SCRIPT:		m_bUseScript = Data; break;
        case API_V_SCRIPT:			m_Script = Data; break;
        }
        return true;
	});
}