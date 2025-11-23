#pragma once
#include "../GenericRule.h"

#include "../Library/API/PrivacyDefs.h"

class CProgramRule : public CGenericRule
{
	Q_OBJECT
	TRACK_OBJECT(CProgramRule)
public:
	CProgramRule(QObject* parent = NULL);
    CProgramRule(const CProgramID& ID, QObject* parent = NULL);

	static bool IsUnsafePath(const QString& Path);

	QString GetProgramPath() const					{return m_ProgramPath;}
	QString GetProgramNtPath() const				{return m_ProgramNtPath;}

	EExecRuleType GetType() const					{return m_Type;}
	QString GetTypeStr() const;
	
	EExecDllMode GetDllMode() const					{return m_DllMode;}

	USignatures GetAllowedSignatures() const		{return m_AllowedSignatures;}
	QList<QString> GetAllowedCollections() const	{return m_AllowedCollections;}
	QList<QString> GetAllowedSignaturesEx() const;

	bool GetImageLoadProtection() const				{return m_ImageLoadProtection;}
	bool GetImageCoherencyChecking() const			{return m_ImageCoherencyChecking;}

	CProgramRule* Clone() const;

protected:
	friend class CProgramRuleWnd;

	void WriteIVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const QtVariant& Data) override;
	void ReadMValue(const SVarName& Name, const QtVariant& Data) override;

	EExecRuleType			m_Type = EExecRuleType::eUnknown;
	bool					m_bUseScript = false;
	QString					m_Script;

	EExecDllMode			m_DllMode = EExecDllMode::eDisabled;

	QString					m_ProgramPath;
	QString					m_ProgramNtPath;
	
	USignatures				m_AllowedSignatures = { 0 };
	QList<QString>			m_AllowedCollections;
	bool					m_ImageLoadProtection = true;
	bool					m_ImageCoherencyChecking = true; // not used yet, but will be in the future

	QList<QString>			m_AllowedChildren;
	QList<QString>			m_AllowedParents;
	QList<QString>			m_BlockedChildren;
	QList<QString>			m_BlockedParents;
};

typedef QSharedPointer<CProgramRule> CProgramRulePtr;