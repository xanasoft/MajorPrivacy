#pragma once
#include "../Library/Common/FlexGuid.h"

#include "QtVariant.h"

class QFlexGuid : public CFlexGuid
{
public:
	QFlexGuid() {}
	QFlexGuid(const char* pStr) { FromString(pStr); }
	QFlexGuid(const QString& Str) { FromQS(Str); }
	QFlexGuid(const QVariant& Var) { FromQV(Var); }

	void FromQS(const QString& Str) { FromWString((wchar_t*)Str.utf16(), Str.length()); }
	QString ToQS() const {
		wchar_t GuidStr[39];
		if (ToWString(GuidStr))
			return QString::fromWCharArray(GuidStr, 38);
		if (m_Type == Unicode)
			return QString::fromWCharArray((wchar_t*)m_String, m_Length);
		if (m_Type == Ascii)
			return QString::fromLatin1((char*)m_String, m_Length);
		return QString(); // IsNull
	}

	bool FromQV(const QVariant& Var)  { 
		if (Var.type() == QVariant::String)  {
			QString Str = Var.toString();
			FromWString((wchar_t*)Str.utf16(), Str.length());
		} else if (Var.type() == QVariant::ByteArray && (Var.toByteArray().size() == 16 || Var.toByteArray().size() == 20)) {
			Clear();
			m_Type = Regular;
			//
			// WARNING: This is a HACK which relays on the order of the fields in the header!!!
			//
			QByteArray Arr = Var.toByteArray();
			memcpy(m_Data, Arr.data(), Arr.size());
		} else 
			return false;
		return true;
	}
	QVariant ToQV() const {
		if (IsNull())
			return QVariant();
		if (m_Type == Unicode)
			return QString::fromWCharArray((wchar_t*)m_String, m_Length);
		if (m_Type == Ascii)
			return QString::fromLatin1((char*)m_String, m_Length);
		//
		// WARNING: This is a HACK which relays on the order of the fields in the header!!!
		//
		return QByteArray((const char*)m_Data, m_LowerCaseMask ? 20 : 16);
	}

	static QSet<QFlexGuid> ReadVariantSet(const QtVariant& Var)
	{
		QSet<QFlexGuid> Set;
		QtVariantReader(Var).ReadRawList([&Set](const QtVariant& var) {
			QFlexGuid Guid;
			Guid.FromVariant(var);
			Set.insert(Guid);
			});
		return Set;
	}
};

__inline uint qHash(const QFlexGuid& Guid)
{
	return qHash(Guid.ToQS());
}