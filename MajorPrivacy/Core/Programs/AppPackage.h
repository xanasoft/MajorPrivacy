#pragma once
#include "ProgramGroup.h"

class CAppPackage: public CProgramList
{
	Q_OBJECT
public:
	CAppPackage(QObject* parent = nullptr);

	virtual EProgramType GetType() const			{ return EProgramType::eAppPackage; }

	virtual QIcon DefaultIcon() const;

	virtual QString GetNameEx() const;
	virtual QString GetAppSid() const				{ return m_AppContainerSid; }
	virtual QString GetContainerName() const		{ return m_AppContainerName; }
	virtual QString GetPath() const					{ return m_Path; }

protected:

	void WriteIVariant(XVariant& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(XVariant& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const XVariant& Data) override;
	void ReadMValue(const SVarName& Name, const XVariant& Data) override;

	QString m_AppContainerSid;
	QString m_AppContainerName;
	QString m_Path;
};

typedef QSharedPointer<CAppPackage> CAppPackagePtr;