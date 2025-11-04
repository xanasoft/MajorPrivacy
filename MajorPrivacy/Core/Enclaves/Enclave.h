#pragma once

#include "../Programs/ProgramId.h"
#include "../Access/Handle.h"
#include "../Processes/Process.h"
#include "../Programs/ProgramRule.h"
#include "./Common/QtFlexGuid.h"

class CEnclave : public QObject
{
	Q_OBJECT
	TRACK_OBJECT(CEnclave)
public:
	CEnclave(QObject* parent = NULL);

	CEnclave* Clone() const;

	QFlexGuid GetGuid() const						{ return m_Guid; }
	void SetGuid(const QFlexGuid& Guid)				{ m_Guid = Guid; }
	bool IsSystem() const							{ return m_Guid == QFlexGuid(SYSTEM_ENCLAVE); }

	QFlexGuid GetVolumeGuid() const					{ return m_VolumeGuid; }
	void SetVolumeGuid(const QFlexGuid& Guid)		{ m_VolumeGuid = Guid; }

	bool IsEnabled() const							{ return m_bEnabled; }
	void SetEnabled(bool bEnabled)					{ m_bEnabled = bEnabled; }
	
	//bool IsLockdown() const							{ return m_bLockdown; }
	//void SetLockdown(bool bLockdown)				{ m_bLockdown = bLockdown; }

	QString GetName() const							{ return m_Name; }
	void SetName(const QString& Name)				{ m_Name = Name; }

	virtual void SetIconFile(const QString& IconFile);
	virtual QString GetIconFile() const;
	virtual QIcon GetIcon() const					{ return m_Icon; }
	virtual void SetIcon(const QIcon& Icon)			{ m_Icon = Icon; }

	//QString GetGrouping() const					{ return m_Grouping; }
	QString GetDescription() const					{ return m_Description; }

	USignatures GetAllowedSignatures() const {return m_AllowedSignatures;}
	void SetAllowedSignatures(const USignatures& Signatures) { m_AllowedSignatures = Signatures; }
	QList<QString> GetAllowedCollections() const		{ return m_AllowedCollections; }
	static QStringList GetAllowedSigners(USignatures Signers, const QList<QString>& Collections);

	EProgramOnSpawn GetOnTrustedSpawn() const		{ return m_OnTrustedSpawn; }
	void SetOnTrustedSpawn(EProgramOnSpawn OnSpawn) { m_OnTrustedSpawn = OnSpawn; }
	EProgramOnSpawn GetOnSpawn() const				{ return m_OnSpawn; }
	void SetOnSpawn(EProgramOnSpawn OnSpawn)		{ m_OnSpawn = OnSpawn; }
	bool GetImageLoadProtection() const				{ return m_ImageLoadProtection; }
	void SetImageLoadProtection(bool bProtect)		{ m_ImageLoadProtection = bProtect; }
	bool GetImageCoherencyChecking() const			{ return m_ImageCoherencyChecking; }
	void SetImageCoherencyChecking(bool bCheck)		{ m_ImageCoherencyChecking = bCheck; }

	bool GetAllowDebugging() const					{ return m_AllowDebugging; }
	void SetAllowDebugging(bool bAllow)				{ m_AllowDebugging = bAllow; }

	bool GetKeepAlive() const						{ return m_KeepAlive; }
	void SetKeepAlive(bool bKeepAlive)				{ m_KeepAlive = bKeepAlive; }

	void AddProcess(CProcessPtr Process)			{ m_Processes.insert(Process->GetProcessId(), Process); }
	void RemoveProcess(CProcessPtr Process)			{ m_Processes.remove(Process->GetProcessId()); }
	QMap<quint64, CProcessPtr> GetProcesses()		{ return m_Processes; }

	//static QString GetSignatureLevelStr(KPH_VERIFY_AUTHORITY SignAuthority);
	static QString GetOnSpawnStr(EProgramOnSpawn OnSpawn);

	void SetData(const char* pKey, const QtVariant& Value) { if (Value.IsValid()) m_Data[pKey] = Value; else m_Data.Remove(pKey); }
	QtVariant GetData(const char* pKey) const { return m_Data.Has(pKey) ? QtVariant(m_Data[pKey]) : QtVariant(); }

	QtVariant ToVariant(const SVarWriteOpt& Opts) const;
	NTSTATUS FromVariant(const class QtVariant& Enclave);

protected:
	friend class CEnclaveWnd;

	void WriteIVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	void WriteMVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	void ReadIValue(uint32 Index, const QtVariant& Data);
	void ReadMValue(const SVarName& Name, const QtVariant& Data);

	void SetIconFile();
	void UpdateIconFile();

	QFlexGuid m_Guid;
	bool m_bEnabled = true;

	//bool m_bLockdown = true;

	QString					m_Name;
	QIcon					m_Icon;
	//QString				m_Grouping;
	QString					m_Description;

	QFlexGuid				m_VolumeGuid;

	bool					m_bUseScript = false;
	QString					m_Script;

	EExecDllMode			m_DllMode = EExecDllMode::eDisabled;

	USignatures				m_AllowedSignatures = { 0 };
	QList<QString>			m_AllowedCollections;
	bool					m_ImageLoadProtection = true;
	bool					m_ImageCoherencyChecking = true;

	EProgramOnSpawn			m_OnTrustedSpawn = EProgramOnSpawn::eAllow;
	EProgramOnSpawn			m_OnSpawn = EProgramOnSpawn::eEject;

	EIntegrityLevel			m_IntegrityLevel = EIntegrityLevel::eUnknown;

	bool					m_AllowDebugging = false;
	bool					m_KeepAlive = false;

	QtVariant				m_Data;

	QMap<quint64, CProcessPtr> m_Processes;
};

typedef QSharedPointer<CEnclave> CEnclavePtr;