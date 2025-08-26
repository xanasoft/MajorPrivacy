#pragma once
#include "../lib_global.h"

#include "../Framework/Common/Variant.h"
#include "../Framework/Common/VariantRW.h"
#include "Strings.h"

class LIBRARY_EXPORT StVariant : public FW::CVariant
{
public:
	explicit StVariant(FW::AbstractMemPool* pMemPool = nullptr, EType Type = VAR_TYPE_EMPTY) : FW::CVariant(pMemPool, Type) {}
	StVariant(const FW::CVariant& Variant) : FW::CVariant(Variant) {}
	StVariant(const StVariant& Variant) : FW::CVariant(Variant) {}
	StVariant(CVariant&& Variant) : FW::CVariant(Variant.Allocator()) {InitMove(Variant);}
	//StVariant(FW::CVariant&& Variant) : FW::CVariant(*(StVariant*)&Variant) {}

#ifndef NO_CRT
	explicit StVariant(const byte* Payload, size_t Size, EType Type = VAR_TYPE_BYTES) : FW::CVariant((FW::AbstractMemPool*)nullptr, Payload, Size, Type) {}
#endif
	explicit StVariant(FW::AbstractMemPool* pMemPool, const byte* Payload, size_t Size, EType Type = VAR_TYPE_BYTES) : FW::CVariant(pMemPool, Payload, Size, Type) {}
	
#ifndef NO_CRT
	template<typename T>
	StVariant(const T& val)	: FW::CVariant(val) {}
#endif
	template<typename T>
	StVariant(FW::AbstractMemPool* pMemPool, const T& val): FW::CVariant(pMemPool, val) {}

	template <typename T>
	StVariant& operator=(T other)						{ return (StVariant&)FW::CVariant::operator=(other); }

	StVariant& operator=(const StVariant& Other)		{Clear(); InitAssign(Other); return *this;}
	StVariant& operator=(CVariant&& Other)				{Clear(); InitMove(Other); return *this;}
	//StVariant& operator=(FW::CVariant&& Other)		{Clear(); InitMove(Other); return *this;}

	// Byte Array
	explicit StVariant(const CBuffer& Buffer) : FW::CVariant(Buffer) {}
	explicit StVariant(CBuffer& Buffer, bool bTake = false) : FW::CVariant(Buffer, bTake) {}
	explicit StVariant(const std::vector<byte>& vec) : FW::CVariant(nullptr, vec.data(), vec.size(), VAR_TYPE_BYTES) {}
	explicit StVariant(FW::AbstractMemPool* pMemPool, const std::vector<byte>& vec) : FW::CVariant(pMemPool, vec.data(), vec.size(), VAR_TYPE_BYTES) {}
	StVariant& operator=(const std::vector<byte>& vec)	{Clear(); InitValue(VAR_TYPE_BYTES, vec.size(), vec.data()); return *this;}

	// ASCII String
	explicit StVariant(const char* str, size_t len = -1) : FW::CVariant(str, len) {}
	StVariant& operator=(const std::string& str)		{Clear(); InitValue(VAR_TYPE_ASCII, str.length(), str.c_str()); return *this;}
	operator std::string() const						{return ToString();}

	// WCHAR String
	explicit StVariant(const wchar_t* wstr, size_t len = -1, bool bUtf8 = false) : FW::CVariant(wstr, len, bUtf8) {}
	StVariant& operator=(const std::wstring& wstr)		{Clear(); InitValue(VAR_TYPE_UNICODE, wstr.length()*sizeof(wchar_t), wstr.c_str()); return *this;}
	operator std::wstring() const						{return ToWString();}

	//////////////////////////////////////////////////////////////////////////////////////////////
	// Type Conversion

	template <class T> 
	T To(const T& Default) const {if (!IsValid()) return Default; return *this;} // tix me TODO dont use cast as cast can throw!!!! get mustbe safe
	template <class T> 
	T To() const {return *this;}

	std::vector<byte> AsBytes() const;

	std::string ToString() const;
	std::wstring ToWString() const;

	std::wstring AsStr(bool* pOk = NULL) const;

	template <class T> 
	T AsNum(bool* pOk = NULL) const
	{
		if (pOk) *pOk = true;
#ifndef VAR_NO_EXCEPTIONS
		try {
#endif
			EType Type = GetType();
			if (Type == VAR_TYPE_SINT) {
				sint64 sint = 0;
				GetInt(sizeof(sint64), &sint);
				return (T)sint;
			}
			else if (Type == VAR_TYPE_UINT) {
				uint64 uint = 0;
				GetInt(sizeof(sint64), &uint);
				return (T)uint;
			}
#ifndef VAR_NO_FPU
			else if (Type == VAR_TYPE_DOUBLE) {
				double val = 0;
				GetDouble(sizeof(double), &val);
				return (T)val;
			}
#endif
			else if (Type == VAR_TYPE_ASCII || Type == VAR_TYPE_BYTES || Type == VAR_TYPE_UTF8 || Type == VAR_TYPE_UNICODE)
			{
				std::wstring Num = ToWString();
				wchar_t* endPtr;
				if (Num.find(L".") != std::wstring::npos)
					return (T)std::wcstod(Num.c_str(), &endPtr);
				else
					return (T)std::wcstoll(Num.c_str(), &endPtr, 10);
			}
#ifndef VAR_NO_EXCEPTIONS
		} catch (...) {}
#endif
		if (pOk) *pOk = false;
		return 0;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////
	// Other

	template<typename T>
	explicit StVariant(const std::vector<T>& List) : StVariant(nullptr, List) {}
	template<typename T>
	explicit StVariant(FW::AbstractMemPool* pMemPool, const std::vector<T>& List) : FW::CVariant(pMemPool) 
	{
		FW::CVariantWriter Writer;
		Writer.BeginList();
		for (const auto& I : List)
			Writer.Write(I);
		this->Assign(Writer.Finish());
	}
	template<typename T>
	StVariant& operator=(const std::vector<T>& List) {
		Assign(StVariant(List));
		return *this;
	}

	explicit StVariant(const std::vector<std::wstring>& List, bool bUtf8 = false) : StVariant(nullptr, List, bUtf8) {}
	explicit StVariant(FW::AbstractMemPool* pMemPool, const std::vector<std::wstring>& List, bool bUtf8 = false) : FW::CVariant(pMemPool) 
	{
		FW::CVariantWriter Writer;
		Writer.BeginList();
		for(auto I = List.begin(); I != List.end(); ++I)
			Writer.Write(I->c_str(), I->length(), bUtf8);
		this->Assign(Writer.Finish());
	}

	explicit StVariant(const std::vector<std::string>& List) : StVariant(nullptr, List) {}
	explicit StVariant(FW::AbstractMemPool* pMemPool, const std::vector<std::string>& List) : FW::CVariant(pMemPool) 
	{
		FW::CVariantWriter Writer;
		Writer.BeginList();
		for(auto I = List.begin(); I != List.end(); ++I)
			Writer.Write(I->c_str());
		this->Assign(Writer.Finish());
	}

	template<typename T>
	explicit StVariant(const std::set<T>& List) : StVariant(nullptr, List) {}
	template<typename T>
	explicit StVariant(FW::AbstractMemPool* pMemPool, const std::set<T>& List) : FW::CVariant() 
	{
		FW::CVariantWriter Writer;
		Writer.BeginList();
		for (const auto& I : List)
			Writer.Write(I);
		this->Assign(Writer.Finish());
	}
	template<typename T>
	StVariant& operator=(const std::set<T>& List) {
		Assign(StVariant(List));
		return *this;
	}


	explicit StVariant(const std::set<std::wstring>& List, bool bUtf8 = false) : StVariant(nullptr, List, bUtf8) {}
	explicit StVariant(FW::AbstractMemPool* pMemPool, const std::set<std::wstring>& List, bool bUtf8 = false) : FW::CVariant(pMemPool)
	{
		FW::CVariantWriter Writer;
		Writer.BeginList();
		for(auto I = List.begin(); I != List.end(); ++I)
			Writer.Write(I->c_str(), I->length(), bUtf8);
		this->Assign(Writer.Finish());
	}

	explicit StVariant(const std::set<std::string>& List) : StVariant(nullptr, List) {}
	explicit StVariant(FW::AbstractMemPool* pMemPool, const std::set<std::string>& List) : FW::CVariant(pMemPool)
	{
		FW::CVariantWriter Writer;
		Writer.BeginList();
		for(auto I = List.begin(); I != List.end(); ++I)
			Writer.Write(I->c_str());
		this->Assign(Writer.Finish());
	}

	template<typename T>
	std::vector<T>			AsList() const {
		std::vector<T> List;
		FW::CVariantReader Reader(*this);
		Reader.ReadRawList([&](const CVariant& Data) {
			List.push_back(((StVariant*)&Data)->To<T>());
		});
		return List;
	}

	/*std::vector<std::wstring> AsStrList() const {
		std::vector<std::wstring> List;
		ReadRawList([&](const CVariant& Data) {
			List.push_back(Data.AsStr());
		});
		return List;
	}*/

	//////////////////////////////////////////////////////////////////////////////////////////////
	// Container Access by reference

	class Ref : public FW::CVariantRef<StVariant>
	{
	public:
		Ref(const FW::CVariant* ptr = nullptr) : FW::CVariantRef<StVariant>((StVariant*)ptr) {}
		Ref(const StVariant* ptr = nullptr) : FW::CVariantRef<StVariant>(ptr) {}

		template <typename T>
		Ref& operator=(const T& val)					{if (m_ptr) m_ptr->operator=(val); return *this;}

		operator FW::CVariant() const					{if (m_ptr) return FW::CVariant(*m_ptr); return FW::CVariant();}

		operator std::string() const					{if (m_ptr) return m_ptr-> ToString(); return "";}
		operator std::wstring() const					{if (m_ptr) return m_ptr-> ToWString(); return L"";}

		std::string ToString() const					{if (m_ptr) return m_ptr->ToString(); return "";}
		std::wstring ToWString() const					{if (m_ptr) return m_ptr->ToWString(); return L"";}

		std::wstring AsStr(bool* pOk = NULL) const		{if (m_ptr) return ((StVariant*)m_ptr)->AsStr(pOk); if (pOk) *pOk = false; return L"";}

		std::vector<byte> AsBytes() const				{if (m_ptr) return ((StVariant*)m_ptr)->AsBytes(); return std::vector<byte>();}
	};

	StVariant(const Ref& Other) : FW::CVariant(Other.m_ptr ? Other.m_ptr->m_pMem : nullptr) 
														{if (Other.m_ptr) Assign(*Other.Ptr());}
	
	StVariant& operator=(const Ref& Other)				{Clear(); if (Other.m_ptr) InitAssign(*Other.Ptr()); return *this;}

	StVariant				Get(const char* Name) const {return FW::CVariant::Get(Name);}
	StVariant				Get(uint32 Index) const		{return FW::CVariant::Get(Index);}

	const Ref				At(const char* Name) const	{auto Ret = PtrAt(Name); if (Ret.Error) Throw(Ret.Error); return Ret.Error ? NULL : Ret.Value;}
	Ref						At(const char* Name)		{auto Ret = PtrAt(Name, true); if (Ret.Error) Throw(Ret.Error); return Ret.Error ? NULL : Ret.Value;}
	Ref operator[](const char* Name)					{return At(Name);}	// Map
	const Ref operator[](const char* Name) const		{return At(Name);}	// Map

	const Ref				At(uint32 Index) const		{auto Ret = PtrAt(Index); if (Ret.Error) Throw(Ret.Error); return Ret.Error ? NULL : Ret.Value;}
	Ref						At(uint32 Index)			{auto Ret = PtrAt(Index, true); if (Ret.Error) Throw(Ret.Error); return Ret.Error ? NULL : Ret.Value;}
	Ref operator[](uint32 Index)						{return At(Index);} // List or Index
	const Ref operator[](uint32 Index)	const			{return At(Index);} // List or Index

	std::wstring WKey(uint32 Index) const // Map
	{
		std::wstring Name; 
		const char* pName = Key(Index);
		if (pName) Name = s2w(pName);
		return Name;
	}
};



class LIBRARY_EXPORT StVariantWriter : public FW::CVariantWriter
{
public:
#ifdef KERNEL_MODE
	StVariantWriter(FW::AbstractMemPool* pMemPool) : FW::CVariantWriter(pMemPool) {}
#else
	StVariantWriter(FW::AbstractMemPool* pMemPool = nullptr) : FW::CVariantWriter(pMemPool) {}
#endif

	EResult WriteEx(const char* Name, const std::string& str)						{return WriteValue(Name, VAR_TYPE_ASCII, str.length(), str.c_str());}
	EResult WriteEx(const char* Name, const std::wstring& wstr, bool bUtf8 = false)	{return Write(Name, wstr.c_str(), wstr.length(), bUtf8);}
	EResult WriteEx(const char* Name, const FW::StringW& strw)						{return FW::CVariantWriter::Write(Name, strw);}

	EResult WriteEx(const std::string& str)											{return WriteValue(VAR_TYPE_ASCII, str.length(), str.c_str());}
	EResult WriteEx(const std::wstring& wstr, bool bUtf8 = false)					{return Write(wstr.c_str(), wstr.length(), bUtf8);}
	EResult WriteEx(const FW::StringW& strw)										{return FW::CVariantWriter::Write(strw);}

	EResult WriteEx(uint32 Index, const std::string& str)							{return WriteValue(Index, VAR_TYPE_ASCII, str.length(), str.c_str());}
	EResult WriteEx(uint32 Index, const std::wstring& wstr, bool bUtf8 = false)		{return Write(Index, wstr.c_str(), wstr.length(), bUtf8);}
	EResult WriteEx(uint32 Index, const FW::StringW& strw)							{return FW::CVariantWriter::Write(Index, strw);}
};



class LIBRARY_EXPORT StVariantReader : public FW::CVariantReader
{
public:
	StVariantReader(const FW::CVariant& Variant) : FW::CVariantReader(Variant) {}

	StVariant				Find(const char* Name) const {StVariant Data(m_Variant.Allocator()); FW::CVariantReader::Find(m_Variant, Name, Data); return Data;}

	StVariant				Find(uint32 Index) const {StVariant Data(m_Variant.Allocator()); FW::CVariantReader::Find(m_Variant, Index, Data); return Data;}
};