#pragma once
#include "ProgramGroup.h"
#include "../../Helpers/FilePath.h"

class CProgramPattern: public CProgramList
{
	Q_OBJECT
public:
	CProgramPattern(QObject* parent = nullptr);

	virtual EProgramType GetType() const		{ return EProgramType::eFilePattern; }

	virtual QIcon DefaultIcon() const;

	void SetPattern(const QString& Pattern, EPathType Type);
	QString GetPattern() const							{ return m_Pattern.Get(EPathType::eWin32); }
	//bool MatchFileName(const QString& FileName);

	virtual QString GetPath(EPathType Type) const		{ return m_Pattern.Get(Type); }
	
protected:

	void WriteIVariant(XVariant& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(XVariant& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const XVariant& Data) override;
	void ReadMValue(const SVarName& Name, const XVariant& Data) override;

	CFilePath m_Pattern;
	//QRegularExpression m_RegExp;
};


typedef QSharedPointer<CProgramPattern> CProgramPatternPtr;