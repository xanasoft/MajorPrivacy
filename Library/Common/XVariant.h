#pragma once
#include "Variant.h"

//
// We need to extend CVariant with a bunch of helper functions,
// we do that by deriving it and wher needed hard casting CVariant to XVariant
// hence booth must have the same representation in memory
// that is XVariant must not have any members
//

class XVariant : public CVariant
{
public:

	class XRef : public Ref
	{
	public:

		operator XVariant() const		{ if (m_ptr) return *((XVariant*)m_ptr); return XVariant(); }
		operator XVariant& () const		{ Q_ASSERT(m_ptr); return *((XVariant*)m_ptr); }

		template <typename T>			// asign anything operator - causes ambiguity
		Ref& operator=(const T& val)	{ if (m_ptr) *((XVariant*)m_ptr) = val; return *this; }

		QString				AsQStr() const { if (m_ptr) return ((XVariant*)m_ptr)->AsQStr(); return QString(); }
		QByteArray			AsQBytes() const { if (m_ptr) return ((XVariant*)m_ptr)->AsQBytes(); return QByteArray(); }

		template<typename T>
		QSet<T>				AsQSet() const { if (m_ptr) return ((XVariant*)m_ptr)->AsQSet(); return QSet<T>(); }
		template<typename T>
		QList<T>			AsQList() const { if (m_ptr) return ((XVariant*)m_ptr)->AsQList(); return QList<T>(); }
		QStringList			AsQList() const { if (m_ptr) return ((XVariant*)m_ptr)->AsQList(); return QStringList(); }
	};

	//
	// Pass through contructors
	//

	XVariant(EType Type = VAR_TYPE_EMPTY) : CVariant((FW::AbstractMemPool*)nullptr, Type) {}
	XVariant(const CVariant& Variant) : CVariant(Variant) {}

	template<typename T>
	XVariant(const T& val)								: CVariant((FW::AbstractMemPool*)nullptr, val) {}
	XVariant(const XRef& val)							: CVariant(val.operator XVariant &()) {}

	XVariant(const std::wstring& wstr, bool bUtf8 = false) : CVariant(wstr, bUtf8) {}
	XVariant(const wchar_t* wstr, size_t len, bool bUtf8 = false)	: CVariant(wstr, len, bUtf8) {}

	XVariant& operator =(const XRef& val)				{ CVariant::operator=(val.operator XVariant &()); return *this; }

	//
	// Qt Types
	//

	XVariant(const QString& String, bool bUtf8 = false) : CVariant(String.toStdWString(), bUtf8) {}
	QString AsQStr() const
	{
		EType Type = GetType();
		if (Type == VAR_TYPE_ASCII || Type == VAR_TYPE_BYTES || Type == VAR_TYPE_UTF8 || Type == VAR_TYPE_UNICODE)
			return ToQString();
		else if (Type == VAR_TYPE_DOUBLE)
			return QString::number(To<double>());
		else if (Type == VAR_TYPE_SINT)
			return QString::number(To<sint32>());
		else if (Type == VAR_TYPE_UINT)
			return QString::number(To<uint32>());
		else
			return QString();
	}

	XVariant(const QByteArray& Bytes)					: CVariant((byte*)Bytes.data(), Bytes.size(), VAR_TYPE_BYTES) {}
	QByteArray AsQBytes() const							{auto pVal = Val(true); if (!pVal.Error && pVal.Value->IsRaw()) { return QByteArray((char*)pVal.Value->Payload, pVal.Value->Size); } return QByteArray();}

	//
	// Overwriten getters/setters
	//

	XVariant&				At(const char* Name)		{return *(XVariant*)&CVariant::At(Name);}
	const XVariant&			At(const char* Name) const	{return *(const XVariant*)&CVariant::At(Name);}
	XVariant&				At(uint32 Index)			{return *(XVariant*)&CVariant::At(Index);}
	const XVariant&			At(uint32 Index) const		{return *(const XVariant*)&CVariant::At(Index);}

	XVariant				Get(const char* Name) const {return CVariant::Get(Name);}
	XVariant				Get(uint32 Index) const {return CVariant::Get(Index);}

	template<typename T>
	XVariant(const QList<T>& List) : CVariant() {
		BeginList();
		foreach(const QString& Item, List)
			Write(Item);
		Finish();
	}

	XVariant(const QStringList& List) : CVariant() {
		BeginList();
		foreach(const QString& Item, List)
			Write(Item.toStdWString());
		Finish();
	}

	template<typename T>
	QSet<T>				AsQSet() const {
		QSet<T> List;
		ReadRawList([&](const CVariant& vData) {
			List.insert(vData.To<T>());
		});
		return List;
	}

	QSet<QString>		AsQSet() const {
		QSet<QString> List;
		ReadRawList([&](const CVariant& vData) {
			const XVariant& Data = *(XVariant*)&vData;
			List.insert(Data.AsQStr());
		});
		return List;
	}

	template<typename T>
	QList<T>				AsQList() const {
		QList<T> List;
		ReadRawList([&](const CVariant& vData) {
			List.append(vData.To<T>());
		});
		return List;
	}

	QStringList				AsQList() const {
		QStringList List;
		ReadRawList([&](const CVariant& vData) {
			const XVariant& Data = *(XVariant*)&vData;
			List.append(Data.AsQStr());
		});
		return List;
	}

	XRef operator[](const char* Name)				{return *(XRef*)&CVariant::At(Name);}
	const XRef operator[](const char* Name) const	{return *(const XRef*)&CVariant::At(Name);}
	XRef operator[](uint32 Index)					{return *(XRef*)&CVariant::At(Index);}
	const XRef operator[](uint32 Index)	const		{return *(const XRef*)&CVariant::At(Index);}

	XVariant Find(const char* Name) const {CVariant Data; CVariant::Find(Name, Data); return Data;}
	XVariant Find(uint32 Index) const {CVariant Data; CVariant::Find(Index, Data); return Data;}

	// String writing

	EResult WriteQStr(const char* Name, const QString& str)	{return CVariant::Write(Name, str.toStdWString());}
	EResult WriteQStr(const QString& str)					{return CVariant::Write(str.toStdWString());}
	EResult WriteQStr(uint32 Index, const QString& str)		{return CVariant::Write(Index, str.toStdWString());}

	//
	// Qt Conversion
	//

	enum QFlags
	{
		eNone = 0,
		eUtf8 = 1,
	};

	bool					FromQVariant(const QVariant& qVariant, QFlags Flags = eNone);
	QVariant				ToQVariant() const;

	QString					SerializeXml() const;
	bool					ParseXml(const QString& String);

protected:
	QString					ToQString() const;

	static void				Serialize(const XVariant& Variant, QXmlStreamWriter &xml);
	static XVariant			Parse(QXmlStreamReader &xml, bool*pOK = NULL);

	static void				Serialize(const QString& Name, const XVariant& Variant, QXmlStreamWriter &xml);
	static bool				Parse(QString &Name, XVariant &Variant, QXmlStreamReader &xml);

	static QString			GetTypeStr(int Type);
	static int				GetTypeValue(QString Type);
};