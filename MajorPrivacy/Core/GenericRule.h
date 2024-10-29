#pragma once
#include "./Programs/ProgramID.h"

class CGenericRule : public QObject
{
	Q_OBJECT
public:
	CGenericRule(QObject* parent = NULL);
	CGenericRule(const CProgramID& ID, QObject* parent = NULL);

	QString GetGuid() const							{ return m_Guid; }
	void SetGuid(const QString& Guid)				{ m_Guid = Guid; }

	const CProgramID& GetProgramID() const			{ return m_ProgramID; }

	bool IsEnabled() const							{ return m_bEnabled; }
	void SetEnabled(bool bEnabled)					{ m_bEnabled = bEnabled; }
	bool IsTemporary() const						{ return m_bTemporary; }
	void SetTemporary(bool bTemporary)				{ m_bTemporary = bTemporary; }

	QString GetName() const							{ return m_Name; }
	void SetName(const QString& Name)				{ m_Name = Name; }
	//QString GetGrouping() const						{ return m_Grouping; }
	QString GetDescription() const					{ return m_Description; }

	virtual XVariant ToVariant(const SVarWriteOpt& Opts) const;
	virtual void FromVariant(const class XVariant& Rule);

protected:
	void CopyTo(CGenericRule* pRule, bool CloneGuid = false) const;

	virtual void WriteIVariant(XVariant& Rule, const SVarWriteOpt& Opts) const;
	virtual void WriteMVariant(XVariant& Rule, const SVarWriteOpt& Opts) const;
	virtual void ReadIValue(uint32 Index, const XVariant& Data);
	virtual void ReadMValue(const SVarName& Name, const XVariant& Data);

	QString m_Guid;

	bool m_bEnabled = true;
	bool m_bTemporary = false;

	QString m_Name;
	//QString m_Grouping;
	QString m_Description;

	CProgramID m_ProgramID;

	XVariant m_Data;
};

typedef QSharedPointer<CGenericRule> CGenericRulePtr;