#pragma once
#include "Enclave.h"
#include "../Library/Status.h"

class CEnclaveManager : public QObject
{
	Q_OBJECT
public:
	CEnclaveManager(QObject* parent);

	STATUS					Update();

	bool					UpdateAllEnclaves();
	bool					UpdateEnclave(const QFlexGuid& Guid);
	void					RemoveEnclave(const QFlexGuid& Guid);

	QMap<QFlexGuid, CEnclavePtr> GetAllEnclaves() { return m_Enclaves; }
	STATUS					SetEnclave(const CEnclavePtr& pEnclave);
	CEnclavePtr				GetEnclave(const QFlexGuid& Guid, bool bCanAdd = false);
	STATUS					DelEnclave(const CEnclavePtr& pEnclave);

protected:

	void					AddEnclave(const CEnclavePtr& pEnclave);
	void					RemoveEnclave(const CEnclavePtr& pEnclave);

	QMap<QFlexGuid, CEnclavePtr> m_Enclaves;
};