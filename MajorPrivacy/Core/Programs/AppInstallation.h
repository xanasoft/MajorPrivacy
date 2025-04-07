#pragma once
#include "ProgramPattern.h"

class CAppInstallation: public CProgramList
{
	Q_OBJECT
public:
	CAppInstallation(QObject* parent = nullptr);
	
	virtual EProgramType GetType() const			{ return EProgramType::eAppInstallation; }

	virtual QString GetRegKey() const				{ return m_RegKey; }
	virtual QString GetPath() const					{ return m_Path; }

protected:

	void WriteIVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const QtVariant& Data) override;
	void ReadMValue(const SVarName& Name, const QtVariant& Data) override;

	QString m_RegKey;
	QString m_Path;
};

typedef QSharedPointer<CAppInstallation> CAppInstallationPtr;