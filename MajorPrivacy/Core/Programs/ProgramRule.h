#pragma once
#include "../GenericRule.h"

#include "../Library/API/PrivacyDefs.h"

class CProgramRule : public CGenericRule
{
	Q_OBJECT
public:
	CProgramRule(QObject* parent = NULL);
    CProgramRule(const CProgramID& ID, QObject* parent = NULL);

	QString GetProgramPath() const					{return m_ProgramPath;}
	QString GetProgramNtPath() const				{return m_ProgramNtPath;}

	EExecRuleType GetType() const					{return m_Type;}
	QString GetTypeStr() const;
	KPH_VERIFY_AUTHORITY GetSignatureLevel() const	{return m_SignatureLevel;}
	static QString GetSignatureLevelStr(KPH_VERIFY_AUTHORITY SignAuthority);
	EProgramOnSpawn GetOnTrustedSpawn() const		{return m_OnTrustedSpawn;}
	EProgramOnSpawn GetOnSpawn() const				{return m_OnSpawn;}
	static QString GetOnSpawnStr(EProgramOnSpawn OnSpawn);
	bool GetImageLoadProtection() const				{return m_ImageLoadProtection;}

	CProgramRule* Clone() const;

protected:
	friend class CProgramRuleWnd;

	void WriteIVariant(XVariant& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(XVariant& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const XVariant& Data) override;
	void ReadMValue(const SVarName& Name, const XVariant& Data) override;

	EExecRuleType m_Type = EExecRuleType::eUnknown;
	QString m_ProgramPath;
	QString m_ProgramNtPath;
	KPH_VERIFY_AUTHORITY m_SignatureLevel = KPH_VERIFY_AUTHORITY::KphUntestedAuthority;
	EProgramOnSpawn m_OnTrustedSpawn = EProgramOnSpawn::eAllow;
	EProgramOnSpawn m_OnSpawn = EProgramOnSpawn::eEject;
	bool m_ImageLoadProtection = true;
};

typedef QSharedPointer<CProgramRule> CProgramRulePtr;