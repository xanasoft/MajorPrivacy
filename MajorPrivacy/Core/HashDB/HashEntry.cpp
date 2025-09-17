#include "pch.h"
#include "HashEntry.h"
#include "../PrivacyCore.h"
#include "./Common/QtVariant.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NtUtil.h"
#include "../../Helpers/WinHelper.h"
#include "../Library/API/PrivacyAPI.h"
#include "../MiscHelpers/Common/Common.h"

CHash::CHash(QObject* parent)
	: QObject(parent)
{

}

CHash::CHash(EHashType Type, const QByteArray& Hash, const QString& Name, QObject* parent)
	: QObject(parent), m_Type(Type), m_Hash(Hash), m_Name(Name)
{
}

CHash* CHash::Clone() const
{
	CHash* pHash = new CHash();

	pHash->m_Name = m_Name;
	pHash->m_bEnabled = m_bEnabled;
	//pHash->m_bTemporary = m_bTemporary;
	pHash->m_Type = m_Type;
	pHash->m_Hash = m_Hash;

	pHash->m_Enclaves = m_Enclaves;

	pHash->m_Collections = m_Collections;

	pHash->m_Data = m_Data;

	return pHash;
}

void CHash::AddEnclave(const QFlexGuid& EnclaveId)
{
	if(EnclaveId.IsNull())
		return;
	if(m_Enclaves.contains(EnclaveId))
		return;
	m_Enclaves.append(EnclaveId);
}

void CHash::RemoveEnclave(const QFlexGuid& EnclaveId)
{
	if(EnclaveId.IsNull())
		return;
	m_Enclaves.removeOne(EnclaveId);
}

bool CHash::HasEnclave(const QFlexGuid& EnclaveId) const
{
	if(EnclaveId.IsNull())
		return false;
	return m_Enclaves.contains(EnclaveId);
}

void CHash::AddCollection(const QString& Collection)
{
	if(Collection.isEmpty())
		return;
	if(m_Collections.contains(Collection))
		return;
	m_Collections.append(Collection);
}

QtVariant CHash::ToVariant(const SVarWriteOpt& Opts) const
{
	QtVariantWriter Hash;
	if (Opts.Format == SVarWriteOpt::eIndex) {
		Hash.BeginIndex();
		WriteIVariant(Hash, Opts);
	} else {  
		Hash.BeginMap();
		WriteMVariant(Hash, Opts);
	}
	return Hash.Finish();
}

NTSTATUS CHash::FromVariant(const class QtVariant& Hash)
{
	if (Hash.GetType() == VAR_TYPE_MAP)			QtVariantReader(Hash).ReadRawMap([&](const SVarName& Name, const QtVariant& Data)	{ ReadMValue(Name, Data); });
	else if (Hash.GetType() == VAR_TYPE_INDEX)	QtVariantReader(Hash).ReadRawIndex([&](uint32 Index, const QtVariant& Data) { ReadIValue(Index, Data); });
	else
		return STATUS_UNKNOWN_REVISION;
	return STATUS_SUCCESS;
}
