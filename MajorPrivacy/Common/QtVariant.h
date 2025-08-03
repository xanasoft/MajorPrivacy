#pragma once

#include "../Framework/Common/Variant.h"
#include "../Framework/Common/VariantRW.h"

class QtVariant : public FW::CVariant
{
public:
	explicit QtVariant(FW::AbstractMemPool* pMemPool = nullptr, EType Type = VAR_TYPE_EMPTY) : FW::CVariant(pMemPool, Type) {}
	QtVariant(const FW::CVariant& Variant) : FW::CVariant(Variant) {}
	QtVariant(const QtVariant& Variant) : FW::CVariant(Variant) {}
	QtVariant(CVariant&& Variant) : FW::CVariant(Variant.Allocator()) {InitMove(Variant);}
	//QtVariant(FW::CVariant&& Variant) : FW::CVariant(*(QtVariant*)&Variant) {}

	explicit QtVariant(const byte* Payload, size_t Size, EType Type = VAR_TYPE_BYTES) : FW::CVariant((FW::AbstractMemPool*)nullptr, Payload, Size, Type) {}

	template<typename T>
	QtVariant(const T& val)	: FW::CVariant(val) {}

	template <typename T>
	QtVariant& operator=(T other)	{ return (QtVariant&)FW::CVariant::operator=(other); }

	QtVariant& operator=(const QtVariant& Other)		{Clear(); InitAssign(Other); return *this;}
	QtVariant& operator=(CVariant&& Other)				{Clear(); InitMove(Other); return *this;}
	//QtVariant& operator=(FW::CVariant&& Other)		{Clear(); InitMove(Other); return *this;}


	// Byte Array
	explicit QtVariant(const CBuffer& Buffer) : FW::CVariant(Buffer) {}
	explicit QtVariant(CBuffer& Buffer, bool bTake = false) : FW::CVariant(Buffer) {}
//	explicit QtVariant(const std::vector<byte>& vec)	:  FW::CVariant(vec.data(), vec.size(), VAR_TYPE_BYTES) {}
//	QtVariant& operator=(const std::vector<byte>& vec) {Clear(); InitValue(VAR_TYPE_BYTES, vec.size(), vec.data()); return *this;}

	// ASCII String
	explicit QtVariant(const char* str, size_t len = -1) : FW::CVariant(str, len) {}
//	QtVariant& operator=(const std::string& str) {Clear(); InitValue(VAR_TYPE_ASCII, str.length(), str.c_str()); return *this;}
//	operator std::string() const			{return ToString();}

	// WCHAR String
	explicit QtVariant(const wchar_t* wstr, size_t len = -1, bool bUtf8 = false) : FW::CVariant(wstr, len, bUtf8) {}
//	QtVariant& operator=(const std::wstring& wstr) {Clear(); InitValue(VAR_TYPE_UNICODE, wstr.length()*sizeof(wchar_t), wstr.c_str()); return *this;}
//	operator std::wstring() const			{return ToWString();}

	// QString
	QtVariant& operator=(const QString& qstr) {Clear(); InitValue(VAR_TYPE_UNICODE, qstr.length()*sizeof(wchar_t), qstr.utf16()); return *this;}
	operator QString() const			{return AsQStr();}


	//////////////////////////////////////////////////////////////////////////////////////////////
	// Type Conversion

	template <class T> 
	T To(const T& Default) const {if (!IsValid()) return Default; return *this;} // tix me TODO dont use cast as cast can throw!!!! get mustbe safe
	template <class T> 
	T To() const {return *this;}

	template <class T> 
	T AsNum(bool* pOk = NULL) const
	{
		if (pOk) *pOk = true;
		try {
			EType Type = GetType();
			if (Type == VAR_TYPE_SINT) {
				sint64 sint = 0;
				GetInt(sizeof(sint64), &sint);
				return sint;
			}
			else if (Type == VAR_TYPE_UINT) {
				uint64 uint = 0;
				GetInt(sizeof(sint64), &uint);
				return uint;
			}
#ifndef VAR_NO_FPU
			else if (Type == VAR_TYPE_DOUBLE) {
				double val = 0;
				GetDouble(sizeof(double), &val);
				return (long long)val;
			}
#endif
			else if (Type == VAR_TYPE_ASCII || Type == VAR_TYPE_BYTES || Type == VAR_TYPE_UTF8 || Type == VAR_TYPE_UNICODE)
			{
				QString Num = ToQString();
				wchar_t* endPtr;
				if (Num.contains(L"."))
					return (T)Num.toDouble();
				else
					return (T)Num.toULongLong();
			}
		} catch (...) {}
		if (pOk) *pOk = false;
		return 0;
	}


	QtVariant(FW::AbstractMemPool* pMemPool, const QString& String, bool bUtf8 = false) : CVariant(pMemPool, (const wchar_t*)String.utf16(), String.length(), bUtf8) {}
	QtVariant(const QString& String, bool bUtf8 = false) : CVariant((const wchar_t*)String.utf16(), String.length(), bUtf8) {}
	QString AsQStr() const
	{
		EType Type = GetType();
		if (Type == VAR_TYPE_ASCII || Type == VAR_TYPE_BYTES || Type == VAR_TYPE_UTF8 || Type == VAR_TYPE_UNICODE)
			return ToQString();
		else if (Type == VAR_TYPE_DOUBLE)
			return QString::number(To<double>());
		else if (Type == VAR_TYPE_SINT)
			return QString::number(To<sint64>());
		else if (Type == VAR_TYPE_UINT)
			return QString::number(To<uint64>());
		else
			return QString();
	}

	QtVariant(FW::AbstractMemPool* pMemPool, const QByteArray& Bytes) : CVariant(pMemPool, (byte*)Bytes.data(), Bytes.size(), VAR_TYPE_BYTES) {}
	QtVariant(const QByteArray& Bytes) : CVariant((byte*)Bytes.data(), Bytes.size(), VAR_TYPE_BYTES) {}
	QByteArray AsQBytes() const							{if (!IsRaw()) return QByteArray(); return QByteArray((char*)GetData(), GetSize());}


	template<typename T>
	QtVariant(const QList<T>& List) : QtVariant(nullptr, List) {}
	template<typename T>
	QtVariant(FW::AbstractMemPool* pMemPool, const QList<T>& List) : CVariant(pMemPool) {
		FW::CVariantWriter Writer;
		Writer.BeginList();
		foreach(const QString& Item, List)
			Writer.Write(Item);
		this->Assign(Writer.Finish());
	}

	QtVariant(const QStringList& List, bool bUtf8 = false) : QtVariant(nullptr, List, bUtf8) {}
	QtVariant(FW::AbstractMemPool* pMemPool, const QStringList& List, bool bUtf8 = false) : CVariant() {
		FW::CVariantWriter Writer;
		Writer.BeginList();
		foreach(const QString& Item, List)
			Writer.Write((const wchar_t*)Item.utf16(), Item.length(), bUtf8);
		this->Assign(Writer.Finish());
	}

	template<typename T>
	QSet<T>				AsQSet() const {
		QSet<T> List;
		FW::CVariantReader Reader(*this);
		Reader.ReadRawList([&](const CVariant& vData) {
			List.insert(vData.To<T>());
		});
		return List;
	}

	QSet<QString>		AsQSet() const {
		QSet<QString> List;
		FW::CVariantReader Reader(*this);
		Reader.ReadRawList([&](const CVariant& vData) {
			const QtVariant& Data = *(QtVariant*)&vData;
			List.insert(Data.AsQStr());
		});
		return List;
	}

	template<typename T>
	QList<T>				AsQList() const {
		QList<T> List;
		FW::CVariantReader Reader(*this);
		Reader.ReadRawList([&](const CVariant& vData) {
			List.append(vData.To<T>());
		});
		return List;
	}

	QStringList				AsQList() const {
		QStringList List;
		FW::CVariantReader Reader(*this);
		Reader.ReadRawList([&](const CVariant& vData) {
			const QtVariant& Data = *(QtVariant*)&vData;
			List.append(Data.AsQStr());
		});
		return List;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////
	// Container Access by reference

	class Ref : public FW::CVariantRef<QtVariant>
	{
	public:
		Ref(const FW::CVariant* ptr = nullptr) : FW::CVariantRef<QtVariant>((QtVariant*)ptr) {}
		Ref(const QtVariant* ptr = nullptr) : FW::CVariantRef<QtVariant>(ptr) {}

		template <typename T>
		Ref& operator=(const T& val)					{if (m_ptr) m_ptr->operator=(val); return *this;}

		operator FW::CVariant() const					{if (m_ptr) return FW::CVariant(*m_ptr); return FW::CVariant();}

		operator QString() const						{if (m_ptr) return m_ptr->AsQStr(); return "";}

		QString				AsQStr() const				{if (m_ptr) return m_ptr->AsQStr(); return QString();}
		QByteArray			AsQBytes() const			{if (m_ptr) return m_ptr->AsQBytes(); return QByteArray();}

		template<typename T>
		QSet<T>				AsQSet() const				{if (m_ptr) return m_ptr->AsQSet(); return QSet<T>();}
		template<typename T>
		QList<T>			AsQList() const				{if (m_ptr) return m_ptr->AsQList(); return QList<T>();}
		QStringList			AsQList() const				{if (m_ptr) return m_ptr->AsQList(); return QStringList();}
	};

	QtVariant(const Ref& Other) : FW::CVariant(Other.m_ptr ? Other.m_ptr->m_pMem : nullptr) 
														{if (Other.m_ptr) Assign(*Other.Ptr());}

	QtVariant& operator=(const Ref& Other)				{Clear(); if (Other.m_ptr) InitAssign(*Other.Ptr()); return *this;}


	QtVariant				Get(const char* Name) const {return FW::CVariant::Get(Name);}
	QtVariant				Get(uint32 Index) const		{return FW::CVariant::Get(Index);}

	const Ref				At(const char* Name) const	{auto Ret = PtrAt(Name); if (Ret.Error) Throw(Ret.Error); return Ret.Error ? NULL : Ret.Value;}
	Ref						At(const char* Name)		{auto Ret = PtrAt(Name, true); if (Ret.Error) Throw(Ret.Error); return Ret.Error ? NULL : Ret.Value;}
	Ref operator[](const char* Name)					{return At(Name);}	// Map
	const Ref operator[](const char* Name) const		{return At(Name);}	// Map

	const Ref				At(uint32 Index) const		{auto Ret = PtrAt(Index); if (Ret.Error) Throw(Ret.Error); return Ret.Error ? NULL : Ret.Value;}
	Ref						At(uint32 Index)			{auto Ret = PtrAt(Index, true); if (Ret.Error) Throw(Ret.Error); return Ret.Error ? NULL : Ret.Value;}
	Ref operator[](uint32 Index)						{return At(Index);} // List or Index
	const Ref operator[](uint32 Index)	const			{return At(Index);} // List or Index


	//////////////////////////////////////////////////////////////////////////////////////////////
	// Qt Conversion

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

	static void				Serialize(const QtVariant& Variant, QXmlStreamWriter &xml);
	static QtVariant			Parse(QXmlStreamReader &xml, bool*pOK = NULL);

	static void				Serialize(const QString& Name, const QtVariant& Variant, QXmlStreamWriter &xml);
	static bool				Parse(QString &Name, QtVariant &Variant, QXmlStreamReader &xml);

	static QString			GetTypeStr(int Type);
	static int				GetTypeValue(QString Type);
};



class QtVariantWriter : public FW::CVariantWriter
{
public:
#ifdef KERNEL_MODE
	QtVariantWriter(FW::AbstractMemPool* pMemPool) : FW::CVariantWriter(pMemPool) {}
#else
	QtVariantWriter(FW::AbstractMemPool* pMemPool = nullptr) : FW::CVariantWriter(pMemPool) {}
#endif

	EResult WriteEx(const char* Name, const QString& qstr, bool bUtf8 = false)	{return Write(Name, (const wchar_t*)qstr.utf16(), qstr.length(), bUtf8);}

	EResult WriteEx(const QString& qstr, bool bUtf8 = false)					{return Write((const wchar_t*)qstr.utf16(), qstr.length(), bUtf8);}

	EResult WriteEx(uint32 Index, const QString& qstr, bool bUtf8 = false)		{return Write(Index, (const wchar_t*)qstr.utf16(), qstr.length(), bUtf8);}
};



class QtVariantReader : public FW::CVariantReader
{
public:
	QtVariantReader(const FW::CVariant& Variant) : FW::CVariantReader(Variant) {}

	QtVariant				Find(const char* Name) const {QtVariant Data(m_Variant.Allocator()); FW::CVariantReader::Find(m_Variant, Name, Data); return Data;}

	QtVariant				Find(uint32 Index) const {QtVariant Data(m_Variant.Allocator()); FW::CVariantReader::Find(m_Variant, Index, Data); return Data;}
};