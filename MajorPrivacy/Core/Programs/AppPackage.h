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

	void WriteIVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const QtVariant& Data) override;
	void ReadMValue(const SVarName& Name, const QtVariant& Data) override;

	QString m_AppContainerSid;
	QString m_AppContainerName;
	QString m_Path;
};

typedef QSharedPointer<CAppPackage> CAppPackagePtr;