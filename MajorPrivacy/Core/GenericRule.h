#pragma once
#include "./Programs/ProgramID.h"
#include "../../Library/Common/FlexGuid.h"

class CGenericRule : public QObject
{
	Q_OBJECT
public:
	CGenericRule(QObject* parent = NULL);
	CGenericRule(const CProgramID& ID, QObject* parent = NULL);

	QFlexGuid GetGuid() const						{ return m_Guid; }
	void SetGuid(const QFlexGuid& Guid)				{ m_Guid = Guid; }

	virtual CProgramID GetProgramID() const			{ return m_ProgramID; }

	virtual void SetEnclave(const QFlexGuid& Enclave) { m_Enclave = Enclave; }
	virtual QFlexGuid GetEnclave() const			{ return m_Enclave; }

	virtual bool IsEnabled() const					{ return m_bEnabled; }
	virtual void SetEnabled(bool bEnabled)			{ m_bEnabled = bEnabled; }
	virtual bool IsTemporary() const				{ return m_bTemporary; }
	virtual void SetTemporary(bool bTemporary)		{ m_bTemporary = bTemporary; }

	virtual void SetTimeOut(quint64 uTimeOutSec)	{ SetTemporary(true); m_uTimeOut = QDateTime::currentDateTime().toSecsSinceEpoch() + uTimeOutSec; }

	virtual QString GetName() const					{ return m_Name; }
	virtual void SetName(const QString& Name)		{ m_Name = Name; }
	//virtual QString GetGrouping() const				{ return m_Grouping; }
	virtual QString GetDescription() const			{ return m_Description; }

	virtual XVariant ToVariant(const SVarWriteOpt& Opts) const;
	virtual void FromVariant(const class XVariant& Rule);

protected:
	void CopyTo(CGenericRule* pRule, bool CloneGuid = false) const;

	virtual void WriteIVariant(XVariant& Rule, const SVarWriteOpt& Opts) const;
	virtual void WriteMVariant(XVariant& Rule, const SVarWriteOpt& Opts) const;
	virtual void ReadIValue(uint32 Index, const XVariant& Data);
	virtual void ReadMValue(const SVarName& Name, const XVariant& Data);

	QFlexGuid m_Guid;
	bool m_bEnabled = true;

	QFlexGuid m_Enclave;

	bool m_bTemporary = false;
	quint64 m_uTimeOut = -1;

	QString m_Name;
	//QString m_Grouping;
	QString m_Description;

	CProgramID m_ProgramID;

	XVariant m_Data;
};

typedef QSharedPointer<CGenericRule> CGenericRulePtr;