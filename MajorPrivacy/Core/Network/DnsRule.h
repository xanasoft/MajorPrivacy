#pragma once
#include "./Common/QtVariant.h"
#include "./Common/QtFlexGuid.h"

#include "../../Library/API/PrivacyDefs.h"

class CDnsRule : public QObject
{
	Q_OBJECT
	TRACK_OBJECT(CDnsRule)
public:
	enum EAction { eUndefined, eBlock, eAllow };

	CDnsRule(QObject* parent = NULL);

	QFlexGuid GetGuid() const					{ return m_Guid; }

	bool IsEnabled() const						{ return m_bEnabled; }
	void SetEnabled(bool bEnabled)				{ m_bEnabled = bEnabled; }

	bool IsTemporary() const					{ return m_bTemporary; }

	QString GetHostName() const					{ return m_HostName; }
	void SetHostName(const QString& HostName)	{ m_HostName = HostName; }

	EAction GetAction() const					{ return m_Action; }
	void SetAction(EAction Action)				{ m_Action = Action; }
	QString GetActionStr() const;

	int GetHitCount() const						{ return m_HitCount; }

	CDnsRule* Clone() const;

	virtual QtVariant ToVariant(const SVarWriteOpt& Opts) const;
	virtual void FromVariant(const class QtVariant& Rule);

protected:
	virtual void WriteIVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	virtual void WriteMVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	virtual void ReadIValue(uint32 Index, const QtVariant& Data);
	virtual void ReadMValue(const SVarName& Name, const QtVariant& Data);

	QFlexGuid		m_Guid;
	bool			m_bEnabled = true;

	bool 			m_bTemporary = false;

	QString			m_HostName;
	EAction			m_Action = eUndefined;

	int				m_HitCount = 0;
};

typedef QSharedPointer<CDnsRule> CDnsRulePtr;