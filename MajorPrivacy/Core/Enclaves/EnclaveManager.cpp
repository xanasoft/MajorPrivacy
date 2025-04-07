#include "pch.h"
#include "EnclaveManager.h"
#include "Core/PrivacyCore.h"
#include "Core/Programs/ProgramManager.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/API/DriverAPI.h"
#include "./Common/QtVariant.h"

CEnclaveManager::CEnclaveManager(QObject* parent)
 : QObject(parent)
{
	CEnclavePtr pEnclave = CEnclavePtr(new CEnclave());
	pEnclave->SetGuid(SYSTEM_ENCLAVE);
	pEnclave->SetName(tr("Major Privacy's Private Enclave"));
	pEnclave->SetSignatureLevel(KphDevAuthority);
	pEnclave->SetOnTrustedSpawn(EProgramOnSpawn::eAllow);
	pEnclave->SetOnSpawn(EProgramOnSpawn::eEject);
	pEnclave->SetImageLoadProtection(TRUE);
	pEnclave->SetIcon(QIcon(":/MajorPrivacy.png"));
	AddEnclave(pEnclave);
}

STATUS CEnclaveManager::Update()
{
    return OK;
}

bool CEnclaveManager::UpdateAllEnclaves()
{
	auto Ret = theCore->GetAllEnclaves();
	if (Ret.IsError())
		return false;

	QtVariant& Enclaves = Ret.GetValue();

	QMap<QFlexGuid, CEnclavePtr> OldEnclaves = m_Enclaves;
	OldEnclaves.take(SYSTEM_ENCLAVE);

	for (int i = 0; i < Enclaves.Count(); i++)
	{
		const QtVariant& Enclave = Enclaves[i];

		QFlexGuid Guid;
		Guid.FromVariant(Enclave[API_V_GUID]);

		CEnclavePtr pEnclave = OldEnclaves.take(Guid);

		bool bAdd = false;
		if (!pEnclave) {
			pEnclave = CEnclavePtr(new CEnclave());
			bAdd = true;
		} 

		pEnclave->FromVariant(Enclave);

		if(bAdd)
			AddEnclave(pEnclave);
	}

	foreach(const QFlexGuid& Guid, OldEnclaves.keys())
		RemoveEnclave(OldEnclaves.take(Guid));

	return true;
}

bool CEnclaveManager::UpdateEnclave(const QFlexGuid& Guid)
{
	auto Ret = theCore->GetEnclave(Guid);
	if (Ret.IsError())
		return false;

	QtVariant& Enclave = Ret.GetValue();

	CEnclavePtr pEnclave = m_Enclaves.value(Guid);

	bool bAdd = false;
	if (!pEnclave) {
		pEnclave = CEnclavePtr(new CEnclave());
		bAdd = true;
	} 

	pEnclave->FromVariant(Enclave);

	if(bAdd)
		AddEnclave(pEnclave);

	return true;
}

void CEnclaveManager::RemoveEnclave(const QFlexGuid& Guid)
{
	CEnclavePtr pEnclave = m_Enclaves.value(Guid);
	if (pEnclave)
		RemoveEnclave(pEnclave);
}

STATUS CEnclaveManager::SetEnclave(const CEnclavePtr& pEnclave)
{
	SVarWriteOpt Opts;
	Opts.Flags = SVarWriteOpt::eTextGuids;

    return theCore->SetEnclave(pEnclave->ToVariant(Opts));
}

CEnclavePtr CEnclaveManager::GetEnclave(const QFlexGuid& Guid, bool bCanAdd)
{
	if(Guid.IsNull())
		return NULL;
	CEnclavePtr pEnclave = m_Enclaves.value(Guid);
	if (!pEnclave && bCanAdd)
	{
		auto Ret = theCore->GetEnclave(Guid);
		if (Ret.IsError())
			return nullptr;

		QtVariant& Enclave = Ret.GetValue();

		pEnclave = CEnclavePtr(new CEnclave());
		pEnclave->FromVariant(Enclave);
		AddEnclave(pEnclave);
	}
    return pEnclave;
}

STATUS CEnclaveManager::DelEnclave(const CEnclavePtr& pEnclave)
{
    STATUS Status = theCore->DelEnclave(pEnclave->GetGuid());
    if(Status)
        RemoveEnclave(pEnclave);
    return Status;
}

void CEnclaveManager::AddEnclave(const CEnclavePtr& pEnclave)
{
	m_Enclaves.insert(pEnclave->GetGuid(), pEnclave);
}

void CEnclaveManager::RemoveEnclave(const CEnclavePtr& pEnclave)
{
	m_Enclaves.remove(pEnclave->GetGuid());
}