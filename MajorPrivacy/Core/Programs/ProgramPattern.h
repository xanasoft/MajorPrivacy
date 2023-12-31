#pragma once
#include "ProgramGroup.h"

class CProgramPattern: public CProgramList
{
	Q_OBJECT
public:
	CProgramPattern(QObject* parent = nullptr);

	virtual QIcon DefaultIcon() const;

	void SetPattern(const QString& Pattern);
	QString GetPattern() const						{ return m_Pattern; }
	bool MatchFileName(const QString& FileName);

	virtual QString GetPath() const					{ return GetPattern(); }
	
protected:

	virtual void ReadValue(const SVarName& Name, const XVariant& Data);

	QString m_Pattern;
	QRegularExpression m_RegExp;
};


typedef QSharedPointer<CProgramPattern> CProgramPatternPtr;