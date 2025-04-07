#include "pch.h"
#include "ProgramPattern.h"
#include "./Common/QtVariant.h"
#include "../Library/API/PrivacyAPI.h"

CProgramPattern::CProgramPattern( QObject* parent)
	: CProgramList(parent)
{
}

QIcon CProgramPattern::DefaultIcon() const
{
	return QIcon(":/Icons/Filter.png");
}

void CProgramPattern::SetPattern(const QString& Pattern)
{ 
	m_Pattern = Pattern; 

	//QString regex = QRegularExpression::escape(Pattern.toLower());
	//regex.replace(QRegularExpression::escape("*"), ".*");
	//regex.replace(QRegularExpression::escape("?"), ".");
	//m_RegExp = QRegularExpression("^" + regex + "$");
}

//bool CProgramPattern::MatchFileName(const QString& FileName)
//{
//	return m_RegExp.match(FileName).hasMatch();
//}