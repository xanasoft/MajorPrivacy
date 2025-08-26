#pragma once
#include "ProgramGroup.h"

class CProgramPattern: public CProgramList
{
	Q_OBJECT
public:
	CProgramPattern(QObject* parent = nullptr);

	virtual EProgramType GetType() const		{ return EProgramType::eFilePattern; }

	virtual QIcon DefaultIcon() const;

	void SetPattern(const QString& Pattern);
	QString GetPattern() const					{ QReadLocker Lock(&m_Mutex); return m_Pattern; }
	//bool MatchFileName(const QString& FileName);

	virtual QString GetPath() const				{ QReadLocker Lock(&m_Mutex); return m_Pattern; }
	
protected:

	void WriteIVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const QtVariant& Data) override;
	void ReadMValue(const SVarName& Name, const QtVariant& Data) override;

	QString m_Pattern;
	//QRegularExpression m_RegExp;
};


typedef QSharedPointer<CProgramPattern> CProgramPatternPtr;