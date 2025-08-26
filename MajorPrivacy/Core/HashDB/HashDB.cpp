#include "pch.h"
#include "HashDB.h"
#include "Core/PrivacyCore.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/API/DriverAPI.h"
#include "./Common/QtVariant.h"

CHashDB::CHashDB(QObject* parent)
 : QObject(parent)
{
}

STATUS CHashDB::Update()
{
	//////////////////////////////////////////////////////////
	// WARING This is called from a differnt thread
	//////////////////////////////////////////////////////////


    return OK;
}

bool CHashDB::UpdateAllHashes()
{
	auto Ret = theCore->GetAllHashes();
	if (Ret.IsError())
		return false;

	QtVariant& Hashes = Ret.GetValue();

	QHash<QByteArray, CHashPtr> OldHashes = m_Hashes;

	for (int i = 0; i < Hashes.Count(); i++)
	{
		const QtVariant& Hash = Hashes[i];

		QByteArray HashValue = Hash[API_V_HASH].AsQBytes();

		CHashPtr pHash = OldHashes.take(HashValue);

		bool bAdd = false;
		if (!pHash) {
			pHash = CHashPtr(new CHash());
			bAdd = true;
		} 

		pHash->FromVariant(Hash);

		if (bAdd) {
			foreach(const QString& Collection, pHash->GetCollections())
				m_Collections.insert(Collection);
			AddHash(pHash);
		}
	}

	foreach(const QByteArray& HashValue, OldHashes.keys())
		RemoveHash(OldHashes.take(HashValue));

	return true;
}

bool CHashDB::UpdateHash(const QByteArray& HashValue)
{
	auto Ret = theCore->GetHashEntry(HashValue);
	if (Ret.IsError())
		return false;

	QtVariant& Hash = Ret.GetValue();

	CHashPtr pHash = m_Hashes.value(HashValue);

	bool bAdd = false;
	if (!pHash) {
		pHash = CHashPtr(new CHash());
		bAdd = true;
	} 

	pHash->FromVariant(Hash);

	if(bAdd)
		AddHash(pHash);

	return true;
}

void CHashDB::RemoveHash(const QByteArray& HashValue)
{
	CHashPtr pHash = m_Hashes.value(HashValue);
	if (pHash)
		RemoveHash(pHash);
}

STATUS CHashDB::SetHash(const CHashPtr& pHash)
{
	SVarWriteOpt Opts;
	Opts.Flags = SVarWriteOpt::eTextGuids;

    return theCore->SetHashEntry(pHash->ToVariant(Opts));
}

CHashPtr CHashDB::GetHash(const QByteArray& HashValue, bool bCanAdd)
{
	if(HashValue.isEmpty())
		return NULL;
	CHashPtr pHash = m_Hashes.value(HashValue);
	if (!pHash && bCanAdd)
	{
		auto Ret = theCore->GetHashEntry(HashValue);
		if (Ret.IsError())
			return nullptr;

		QtVariant& Hash = Ret.GetValue();

		pHash = CHashPtr(new CHash());
		pHash->FromVariant(Hash);
		AddHash(pHash);
	}
    return pHash;
}

STATUS CHashDB::DelHash(const CHashPtr& pHash)
{
    STATUS Status = theCore->DelHashEntry(pHash->GetHash());
    if(Status)
        RemoveHash(pHash);
    return Status;
}

void CHashDB::AddHash(const CHashPtr& pHash)
{
	m_Hashes.insert(pHash->GetHash(), pHash);
}

void CHashDB::RemoveHash(const CHashPtr& pHash)
{
	m_Hashes.remove(pHash->GetHash());
}

STATUS CHashDB::AllowFile(QString FilePath, const QFlexGuid& EnclaveId, const QString& Collection)
{
	FilePath.replace("/", "\\");

	CBuffer FileHash;
	STATUS Status = theCore->HashFile(FilePath, FileHash);
	if(!Status) return Status;
	QByteArray HashValue((char*)FileHash.GetBuffer(), FileHash.GetSize());

	CHashPtr pHash = GetHash(HashValue, true);
	if(!pHash)
	{
		pHash = CHashPtr(new CHash(EHashType::eFileHash, HashValue, FilePath));
		AddHash(pHash);
	}

	pHash->AddEnclave(EnclaveId);
	pHash->AddCollection(Collection);

	return theCore->SetHashEntry(pHash->ToVariant(SVarWriteOpt()));
}

STATUS CHashDB::AllowCert(const QByteArray& HashValue, const QString& Subject, const QFlexGuid& EnclaveId, const QString& Collection)
{
	CHashPtr pHash = GetHash(HashValue, true);
	if(!pHash)
	{
		pHash = CHashPtr(new CHash(EHashType::eCertHash, HashValue, Subject));
		AddHash(pHash);
	}

	pHash->AddEnclave(EnclaveId);
	pHash->AddCollection(Collection);

	return theCore->SetHashEntry(pHash->ToVariant(SVarWriteOpt()));
}

STATUS CHashDB::ClearFile(QString FilePath)
{
	FilePath.replace("/", "\\");

	CBuffer FileHash;
	STATUS Status = theCore->HashFile(FilePath, FileHash);
	if(!Status) return Status;
	QByteArray HashValue((char*)FileHash.GetBuffer(), FileHash.GetSize());

	return theCore->DelHashEntry(HashValue);
}

STATUS CHashDB::ClearCert(const QByteArray& HashValue)
{
	return theCore->DelHashEntry(HashValue);
}