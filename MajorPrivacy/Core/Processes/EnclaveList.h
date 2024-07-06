#pragma once
#include "Enclave.h"
#include "../Library/Status.h"

class CEnclaveList : public QObject
{
	Q_OBJECT
public:
	CEnclaveList(QObject* parent);

	STATUS Update();

	QMap<quint64, CEnclavePtr> List() { return m_List; }
	CEnclavePtr GetEnclave(quint64 Eid, bool CanUpdate = false);

	int GetCount() const { return m_List.count(); }

protected:
	QMap<quint64, CEnclavePtr> m_List;
};