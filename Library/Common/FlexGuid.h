#pragma once
#include "../lib_global.h"
#include "Variant.h"

struct SGuid
{  
	DWORD Data1;
	WORD Data2;
	WORD Data3;
	WORD Data4;
	BYTE Data5[6];
};

class LIBRARY_EXPORT CFlexGuid
{
public:
	CFlexGuid();
	CFlexGuid(const std::wstring& Str) { FromWString(Str); }
	CFlexGuid(const std::string& Str) { FromString(Str); }
	CFlexGuid(const CFlexGuid& Other) { *this = Other; }
	CFlexGuid(CFlexGuid&& Other); // move constructor
	CFlexGuid(const CVariant& Variant) { FromVariant(Variant); }
	~CFlexGuid();

	CFlexGuid& operator = (const CFlexGuid& Other);

	bool			IsNull() const { return m_Type == Null; }
	bool			IsRegular() const { return m_Type == Regular || m_Type == Null; }
	bool			IsString() const { return m_Type == Unicode || m_Type == Ascii; }

	void			Clear();

	void			SetRegularGuid(const SGuid* pGuid);
	SGuid*			GetRegularGuid() const;

	void			FromWString(const std::wstring& Str) { FromWString(Str.c_str(), Str.size()); }
	std::wstring	ToWString() const;
	void			FromString(const std::string& Str) { FromAString(Str.c_str(), Str.size()); }
	std::string		ToString() const;

	int Compare(const CFlexGuid& other) const;
	bool operator<(const CFlexGuid& other) const { return Compare(other) < 0; }
	bool operator==(const CFlexGuid& other) const { return Compare(other) == 0; }
	bool operator != (const CFlexGuid& Other) const { return !(*this == Other); }

	bool			FromVariant(const CVariant& Variant);
	CVariant		ToVariant(bool bTextOnly/* = false*/) const;

	static CVariant WriteList(const std::vector<CFlexGuid>& List, bool bTextOnly/* = false*/)
	{
		CVariant Var;
		Var.BeginList();
		for (const CFlexGuid& Guid : List)
			Var.WriteVariant(Guid.ToVariant(bTextOnly));
		Var.Finish();
		return Var;
	}

protected:
	void			FromWString(const wchar_t* pStr, size_t uLen = -1, bool bIgnoreCase = false);
	void			FromAString(const char* pStr, size_t uLen = -1, bool bIgnoreCase = false);
	bool		    ToWString(wchar_t* pStr) const;
	bool		    ToAString(char* pStr) const;

	//
	// Note: on a 64 bit system the fields will be pointer aligned,
	// hence the size of the object if > 16 will be at leats 24,
	// so lets use put all the 24 bytes to good use
	//
	union {
		byte		m_Data[16];
		struct {
			size_t	m_Length;
			void*	m_String;
		};
	};
	// 16
	uint32			m_LowerCaseMask = 0; // WARNING: Do now change Data and LowerCaseMask order, this is a HACK, see ToVariant/FromVariant
	// 20
	enum EType : uint8
	{
		Null = 0,
		Regular = 1,
		Unicode = 2,
		Ascii = 3
	}				m_Type = Null;
	// 21
	uint8			m_Reserved1 = 0;
	// 22
	uint16			m_Reserved2 = 0;
	// 24
	// +++
	// 
	// only 3 bytes unused
	// 
};

#ifdef QT_VERSION

#include "XVariant.h"

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

	static QSet<QFlexGuid> ReadVariantSet(const XVariant& Var)
	{
		QSet<QFlexGuid> Set;
		Var.ReadRawList([&Set](const XVariant& var) {
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
#endif