#include "pch.h"
#include "ProgramPattern.h"
#include "../Library/Common/XVariant.h"
#include "../Library/API/PrivacyAPI.h"

CProgramPattern::CProgramPattern( QObject* parent)
	: CProgramList(parent)
{
}

QIcon CProgramPattern::DefaultIcon() const
{
	return QIcon(":/Icons/Filter.png");
}

void CProgramPattern::SetPattern(const QString& Pattern, EPathType Type)
{ 
	m_Pattern.Set(Pattern, Type); 

	//QString regex = QRegularExpression::escape(Pattern.toLower());
	//regex.replace(QRegularExpression::escape("*"), ".*");
	//regex.replace(QRegularExpression::escape("?"), ".");
	//m_RegExp = QRegularExpression("^" + regex + "$");
}

//bool CProgramPattern::MatchFileName(const QString& FileName)
//{
//	return m_RegExp.match(FileName).hasMatch();
//}