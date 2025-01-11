#pragma once

#include "../Programs/ProgramId.h"
#include "../Access/Handle.h"
#include "../Processes/Process.h"
#include "../Programs/ProgramRule.h"
#include "../../Library/Common/FlexGuid.h"

class CEnclave : public QObject
{
	Q_OBJECT
public:
	CEnclave(QObject* parent = NULL);

	CEnclave* Clone() const;

	QFlexGuid GetGuid() const						{ return m_Guid; }
	void SetGuid(const QFlexGuid& Guid)				{ m_Guid = Guid; }
	bool IsSystem() const							{ return m_Guid == QFlexGuid(SYSTEM_ENCLAVE); }

	bool IsEnabled() const							{ return m_bEnabled; }
	void SetEnabled(bool bEnabled)					{ m_bEnabled = bEnabled; }

	QString GetName() const							{ return m_Name; }
	void SetName(const QString& Name)				{ m_Name = Name; }

	virtual void SetIconFile(const QString& IconFile);
	virtual QString GetIconFile() const;
	virtual QIcon GetIcon() const					{ return m_Icon; }
	virtual void SetIcon(const QIcon& Icon)			{ m_Icon = Icon; }

	//QString GetGrouping() const					{ return m_Grouping; }
	QString GetDescription() const					{ return m_Description; }

	KPH_VERIFY_AUTHORITY GetSignatureLevel() const	{ return m_SignatureLevel; }
	void SetSignatureLevel(KPH_VERIFY_AUTHORITY SignAuthority) { m_SignatureLevel = SignAuthority; }
	EProgramOnSpawn GetOnTrustedSpawn() const		{ return m_OnTrustedSpawn; }
	void SetOnTrustedSpawn(EProgramOnSpawn OnSpawn) { m_OnTrustedSpawn = OnSpawn; }
	EProgramOnSpawn GetOnSpawn() const				{ return m_OnSpawn; }
	void SetOnSpawn(EProgramOnSpawn OnSpawn)		{ m_OnSpawn = OnSpawn; }
	bool GetImageLoadProtection() const				{ return m_ImageLoadProtection; }
	void SetImageLoadProtection(bool bProtect)		{ m_ImageLoadProtection = bProtect; }

	void AddProcess(CProcessPtr Process)			{ m_Processes.insert(Process->GetProcessId(), Process); }
	void RemoveProcess(CProcessPtr Process)			{ m_Processes.remove(Process->GetProcessId()); }
	QMap<quint64, CProcessPtr> GetProcesses()		{ return m_Processes; }

	static QString CEnclave::GetSignatureLevelStr(KPH_VERIFY_AUTHORITY SignAuthority);
	static QString CEnclave::GetOnSpawnStr(EProgramOnSpawn OnSpawn);

	void SetData(const char* pKey, const XVariant& Value) { if (Value.IsValid()) m_Data[pKey] = Value; else m_Data.Remove(pKey); }
	XVariant GetData(const char* pKey) const { return m_Data.Has(pKey) ? XVariant(m_Data[pKey]) : XVariant(); }

	XVariant ToVariant(const SVarWriteOpt& Opts) const;
	NTSTATUS FromVariant(const class XVariant& Enclave);

protected:
	friend class CEnclaveWnd;

	void WriteIVariant(XVariant& Rule, const SVarWriteOpt& Opts) const;
	void WriteMVariant(XVariant& Rule, const SVarWriteOpt& Opts) const;
	void ReadIValue(uint32 Index, const XVariant& Data);
	void ReadMValue(const SVarName& Name, const XVariant& Data);

	void SetIconFile();
	void UpdateIconFile();

	QFlexGuid m_Guid;
	bool m_bEnabled = true;

	QString m_Name;
	QIcon m_Icon;
	//QString m_Grouping;
	QString m_Description;

	KPH_VERIFY_AUTHORITY m_SignatureLevel = KPH_VERIFY_AUTHORITY::KphUntestedAuthority;
	EProgramOnSpawn m_OnTrustedSpawn = EProgramOnSpawn::eAllow;
	EProgramOnSpawn m_OnSpawn = EProgramOnSpawn::eEject;
	bool m_ImageLoadProtection = true;

	XVariant m_Data;

	QMap<quint64, CProcessPtr> m_Processes;
};

typedef QSharedPointer<CEnclave> CEnclavePtr;