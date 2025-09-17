#pragma once
#include "HashEntry.h"
#include "../Library/Status.h"

class CHashDB : public QObject
{
	Q_OBJECT
public:
	CHashDB(QObject* parent);

	STATUS					Update();

	bool					UpdateAllHashes();
	bool					UpdateHash(const QByteArray& HashValue);
	void					RemoveHash(const QByteArray& HashValue);

	QHash<QByteArray, CHashPtr> GetAllHashes() { return m_Hashes; }
	STATUS					SetHash(const CHashPtr& pHash);
	CHashPtr				GetHash(const QByteArray& HashValue, bool bCanAdd = false);
	STATUS					DelHash(const CHashPtr& pHash);

	STATUS					AllowFile(QString FilePath, const QFlexGuid& EnclaveId = QFlexGuid(), const QString& Collection = QString());
	STATUS					AllowCert(const QByteArray& HashValue, const QString& Subject, const QFlexGuid& EnclaveId = QFlexGuid(), const QString& Collection = QString());

	STATUS					ClearFile(QString FilePath, const QFlexGuid& EnclaveId = QFlexGuid());
	STATUS					ClearCert(const QByteArray& HashValue, const QFlexGuid& EnclaveId = QFlexGuid());

	QSet<QString>			GetCollections() const { return m_Collections; }
	void					AddCollection(const QString& Collection) { m_Collections.insert(Collection); }

protected:

	void					AddHash(const CHashPtr& pHash);
	void					RemoveHash(const CHashPtr& pHash);

	QHash<QByteArray, CHashPtr> m_Hashes;

	QSet<QString>			m_Collections;
};