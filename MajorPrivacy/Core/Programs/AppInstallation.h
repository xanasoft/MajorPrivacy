#pragma once
#include "ProgramPattern.h"

class CAppInstallation: public CProgramPattern
{
	Q_OBJECT
public:
	CAppInstallation(QObject* parent = nullptr);
	
	virtual EProgramType GetType() const					{ return EProgramType::eAppInstallation; }

	virtual QString GetRegKey() const				{ return m_RegKey; }
	virtual QString GetPath(EPathType Type) const	{ return m_Path.Get(Type); }

protected:

	void WriteIVariant(XVariant& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(XVariant& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const XVariant& Data) override;
	void ReadMValue(const SVarName& Name, const XVariant& Data) override;

	QString m_RegKey;
	CFilePath m_Path;
};

typedef QSharedPointer<CAppInstallation> CAppInstallationPtr;