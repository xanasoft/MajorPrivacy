#pragma once

#include "../Programs/ProgramId.h"
#include "../Access/Handle.h"
#include "Process.h"
#include "../Programs/ProgramRule.h"

class CEnclave : public QObject
{
	Q_OBJECT
public:
	CEnclave(quint64 Eid, QObject* parent = NULL);

	void Update();

	quint64 GetEnclaveID() const { return m_Eid; }
	QString GetName() const { return m_Name; }
	QString GetNameEx() const;

	KPH_VERIFY_AUTHORITY GetSignatureLevel() const { return m_SignatureLevel; }
	EProgramOnSpawn GetOnTrustedSpawn() const { return m_OnTrustedSpawn; }
	EProgramOnSpawn GetOnSpawn() const { return m_OnSpawn; }
	bool GetImageLoadProtection() const { return m_ImageLoadProtection; }

	void AddProcess(CProcessPtr Process)			{ m_Processes.insert(Process->GetProcessId(), Process); }
	void RemoveProcess(CProcessPtr Process)			{ m_Processes.remove(Process->GetProcessId()); }
	QMap<quint64, CProcessPtr> GetProcesses()		{ return m_Processes; }

	void FromVariant(const class XVariant& Enclave);

protected:

	quint64 m_Eid = 0;
	QString m_Name;

	KPH_VERIFY_AUTHORITY m_SignatureLevel = KPH_VERIFY_AUTHORITY::KphUntestedAuthority;
	EProgramOnSpawn m_OnTrustedSpawn = EProgramOnSpawn::eAllow;
	EProgramOnSpawn m_OnSpawn = EProgramOnSpawn::eEject;
	bool m_ImageLoadProtection = true;

	QMap<quint64, CProcessPtr> m_Processes;
};

typedef QSharedPointer<CEnclave> CEnclavePtr;