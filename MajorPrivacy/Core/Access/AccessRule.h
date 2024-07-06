#pragma once
#include "../GenericRule.h"

#include "../../Library/API/PrivacyDefs.h"

class CAccessRule : public CGenericRule
{
	Q_OBJECT
public:
	CAccessRule(QObject* parent = NULL);
    CAccessRule(const CProgramID& ID, QObject* parent = NULL);

	void SetPath(const QString& Path)				{m_AccessPath = Path;}
	QString GetPath() const							{return m_AccessPath;}
	void SetProgramPath(const QString& Path)		{m_ProgramPath = Path;}
	QString GetProgramPath() const					{return m_ProgramPath;}

	void SetType(EAccessRuleType Type)				{m_Type = Type;}
	EAccessRuleType GetType() const					{return m_Type;}
	QString GetTypeStr() const;

	CAccessRule* Clone() const;

protected:
	friend class CAccessRuleWnd;

	void WriteIVariant(XVariant& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(XVariant& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const XVariant& Data) override;
	void ReadMValue(const SVarName& Name, const XVariant& Data) override;

	EAccessRuleType m_Type = EAccessRuleType::eNone;

	QString m_AccessPath;
	QString m_ProgramPath; // can be pattern
};

typedef QSharedPointer<CAccessRule> CAccessRulePtr;