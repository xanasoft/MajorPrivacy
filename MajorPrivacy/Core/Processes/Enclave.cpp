#include "pch.h"
#include "Enclave.h"
#include "../PrivacyCore.h"
#include "../Library/Common/XVariant.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NtUtil.h"
#include "../../Helpers/WinHelper.h"
#include "../Driver/KSI/include/kphapi.h"
#include "../Library/API/PrivacyAPI.h"

CEnclave::CEnclave(quint64 Eid, QObject* parent)
	: QObject(parent)
{
	m_Eid = Eid;
}

void CEnclave::Update()
{
	if (m_Eid == SYSTEM_ENCLAVE_ID) { // the system enclave does nto have an actual instance in the driver
		m_SignatureLevel = KPH_VERIFY_AUTHORITY::KphDevAuthority;
		return;
	}

	CVariant ReqVar;
	ReqVar[API_V_EID] = m_Eid;

	auto Ret = theCore->Driver()->Call(API_GET_ENCLAVE_INFO, ReqVar);
	if (Ret.IsError())
		return;

	XVariant& ResVar = (XVariant&)Ret.GetValue();
	FromVariant(ResVar);
}

QString CEnclave::GetNameEx() const
{ 
	if(m_Eid == SYSTEM_ENCLAVE_ID)
		return tr("Major Privacy Enclave");
	if(m_Name.isEmpty())
		return tr("Unnamed Enclave");
	return m_Name; 
}

void CEnclave::FromVariant(const class XVariant& Enclave)
{
	Enclave.ReadRawIMap([&](int Index, const CVariant& vData) {
		const XVariant& Data = *(XVariant*)&vData;

		switch(Index)
		{
		case API_V_ENCLAVE_ID:						break;
		case API_V_ENCLAVE_NAME:					m_Name = Data.AsQStr(); break;
		case API_V_ENCLAVE_SIGN_LEVEL:				m_SignatureLevel = (KPH_VERIFY_AUTHORITY)Data.To<uint32>(); break;
		case API_V_ENCLAVE_ON_TRUSTED_SPAWN:		m_OnTrustedSpawn = (EProgramOnSpawn)Data.To<uint32>(); break;
		case API_V_ENCLAVE_ON_SPAWN:				m_OnSpawn = (EProgramOnSpawn)Data.To<uint32>(); break;
		case API_V_ENCLAVE_IMAGE_LOAD_PROTECTION:	m_ImageLoadProtection = Data.To<bool>(); break;
		}
	});
}
