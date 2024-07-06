#include "pch.h"
#include "EnclaveList.h"
#include "Core/PrivacyCore.h"
#include "Core/Programs/ProgramManager.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/API/DriverAPI.h"
#include "../Library/Common/XVariant.h"

CEnclaveList::CEnclaveList(QObject* parent)
 : QObject(parent)
{
}

STATUS CEnclaveList::Update()
{
    auto Res = theCore->Driver()->EnumEnclaves();
    if(Res.IsError())
        return Res.GetStatus(); // todo log error

    auto Eids = *Res.GetValue();

    QMap<quint64, CEnclavePtr> OldMap = m_List;

    for (uint64 Eid : Eids)
    {
        CEnclavePtr pEnclave = OldMap.take(Eid);
        if (pEnclave.isNull())
        {
            pEnclave = CEnclavePtr(new CEnclave(Eid));
            m_List.insert(Eid, pEnclave);

            pEnclave->Update();
        }
    }

    OldMap.remove(SYSTEM_ENCLAVE_ID); // keep system enclave
    foreach(const quint64 & Eid, OldMap.keys())
        m_List.remove(Eid);

    return OK;
}

CEnclavePtr CEnclaveList::GetEnclave(quint64 Eid, bool CanUpdate)
{ 
	CEnclavePtr pEnclave = m_List.value(Eid); 

    if (pEnclave.isNull() && CanUpdate) 
    {
        pEnclave = CEnclavePtr(new CEnclave(Eid));
        m_List.insert(Eid, pEnclave);

        pEnclave->Update();
    }

	return pEnclave;
}