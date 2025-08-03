#pragma once

#include "Variant.h"

FW_NAMESPACE_BEGIN

//////////////////////////////////////////////////////////////////////////////////////////////
// Reader

class FRAMEWORK_EXPORT CVariantReader
{
	struct SReader
	{
		SReader(CVariant* pVariant) : Value(pVariant->m_pMem), pVariant(pVariant) {}
		virtual ~SReader() {}
		virtual bool Next() = 0;
		virtual bool AtEnd() const = 0;

		CVariant*				pVariant;
		SVarName				Name = {nullptr, 0};
		uint32					Index = 0;
		CVariant				Value;
	}* m = nullptr;	

public:
	CVariantReader(const FW::CVariant& Variant) : m_Variant(Variant) {}
	~CVariantReader() { End(); }

	typedef CVariant::EResult EResult;

	bool Begin();
	bool Next()					{ return m ? m->Next() : false; }
	bool AtEnd() const			{ return m ? m->AtEnd() : true; }
	void End();

	SVarName Name() const		{ return m ? m->Name : SVarName {nullptr, 0}; }
	uint32 Index() const		{ return m ? m->Index : -1; }
	CVariant Value() const		{ return m ? m->Value : CVariant(m_Variant.Allocator()); }


	// Map Read
	static EResult				ReadRawMap(const CVariant& Variant, void(*cb)(const SVarName& Name, const CVariant& Data, void* Param), void* Param);
	template <typename F>
	static void ReadRawMapCM(const SVarName& Name, const CVariant& Data, void* Param) {F* func = (F*)(Param); (*func)(Name, Data);}
	template <typename F>
	static EResult				ReadRawMap(const CVariant& Variant, F&& cb) {return ReadRawMap(Variant, ReadRawMapCM<F>, (void*)(&cb));}
	template <typename F>
	EResult						ReadRawMap(F&& cb) const {return FW::CVariantReader::ReadRawMap(m_Variant, ReadRawMapCM<F>, (void*)(&cb));}

	static EResult				Find(const CVariant& Variant, const char* Name, CVariant& Data);
	static CVariant				Find(const CVariant& Variant, const char* Name) {CVariant Data(Variant.m_pMem); Find(Variant, Name, Data); return Data;}
	CVariant					Find(const char* Name) const { CVariant Data(m_Variant.m_pMem); Find(m_Variant, Name, Data); return Data; }

	// List Read
	static EResult				ReadRawList(const CVariant& Variant, void(*cb)(const CVariant& Data, void* Param), void* Param);
	template <typename F>
	static void ReadRawListCB(const CVariant& Data, void* Param) {F* func = (F*)(Param); (*func)(Data);}
	template <typename F>
	static EResult				ReadRawList(const CVariant& Variant, F&& cb) {return ReadRawList(Variant, ReadRawListCB<F>, (void*)(&cb));}
	template <typename F>
	EResult						ReadRawList(F&& cb) const {return FW::CVariantReader::ReadRawList(m_Variant, ReadRawListCB<F>, (void*)(&cb));}

	// Index Read
	static EResult				ReadRawIndex(const CVariant& Variant, void(*cb)(uint32 Index, const CVariant& Data, void* Param), void* Param);
	template <typename F>
	static void ReadRawIndexCB(uint32 Index, const CVariant& Data, void* Param) {F* func = (F*)(Param); (*func)(Index, Data);}
	template <typename F>
	static EResult				ReadRawIndex(const CVariant& Variant, F&& cb) {return ReadRawIndex(Variant, ReadRawIndexCB<F>, (void*)(&cb));}
	template <typename F>
	EResult						ReadRawIndex(F&& cb) const {return FW::CVariantReader::ReadRawIndex(m_Variant, ReadRawIndexCB<F>, (void*)(&cb));}

	static EResult				Find(const CVariant& Variant, uint32 Index, CVariant& Data);
	static CVariant				Find(const CVariant& Variant, uint32 Index) {CVariant Data(Variant.m_pMem); Find(Variant, Index, Data); return Data;}
	CVariant					Find(uint32 Index) const { CVariant Data(m_Variant.m_pMem); Find(m_Variant, Index, Data); return Data; }

protected:
	const FW::CVariant&			m_Variant;

	struct SRawReader: SReader
	{
		SRawReader(CVariant* pVariant)
			: SReader(pVariant), Buffer(pVariant->m_pMem, pVariant->GetData(), pVariant->GetSize(), true) { Ready = Load(); }
		bool Load();
		bool Next() { Value.Clear(); Ready = Load(); return Ready; }
		bool AtEnd() const {return !Ready;}
		CBuffer					Buffer;
		bool					Ready;
	};

	struct SMapReader : SReader
	{
		SMapReader(CVariant* pVariant)
			: SReader(pVariant), I(CVariant::MapBegin(pVariant->m_p.Map)) { Load(); }
		bool Load();
		bool Next() {if (AtEnd()) return false; ++I; return Load();}
		bool AtEnd() const {return I == CVariant::MapEnd();}
		CVariant::TMap::Iterator I;
	};

	struct SListReader : SReader
	{
		SListReader(CVariant* pVariant)
			: SReader(pVariant) { Load(); }
		bool Load();
		bool Next() {if (AtEnd()) return false; i++; return Load();}
		bool AtEnd() const {return i >= pVariant->m_p.List->Count;}
		uint32 i = 0;
	};

	struct SIndexReader : SReader
	{
		SIndexReader(CVariant* pVariant)
			: SReader(pVariant), I(CVariant::IndexBegin(pVariant->m_p.Index)) { Load(); }
		bool Load();
		bool Next() {if (AtEnd()) return false; ++I; return Load();}
		bool AtEnd() const {return I == CVariant::IndexEnd();}
		CVariant::TIndex::Iterator I;
	};
};


//////////////////////////////////////////////////////////////////////////////////////////////
// Writer

class FRAMEWORK_EXPORT CVariantWriter
{
public:

	typedef CVariant::EResult EResult;
	typedef CVariant::EType EType;

#ifdef KERNEL_MODE
	CVariantWriter(FW::AbstractMemPool* pMemPool) : m_Buffer(pMemPool) {}
#else
	CVariantWriter(FW::AbstractMemPool* pMemPool = nullptr) : m_Buffer(pMemPool) {}
#endif
	
	FW::AbstractMemPool*	Allocator() const { return m_Buffer.Allocator(); }

	// Map Write
	EResult					BeginMap() {return BeginWrite(VAR_TYPE_MAP);}
	EResult					WriteValue(const char* Name, EType Type, size_t Size, const void* Value);
	EResult					WriteVariant(const char* Name, const CVariant& Variant);

	// List Write
	EResult					BeginList() {return BeginWrite(VAR_TYPE_LIST);}
	EResult					WriteValue(EType Type, size_t Size, const void* Value);
	EResult					WriteVariant(const CVariant& Variant);

	// Index Write
	EResult					BeginIndex() {return BeginWrite(VAR_TYPE_INDEX);}
	EResult					WriteValue(uint32 Index, EType Type, size_t Size, const void* Value);
	EResult					WriteVariant(uint32 Index, const CVariant& Variant);

	CVariant					Finish();

	//////////////////////////////////////////////////////////////////////////////////////////////
	// Map Write Implementation

	//EResult Write(const char* Name, const CBuffer& Buffer)			{return WriteValue(Name, VAR_TYPE_BYTES, Buffer.GetSize(), Buffer.GetBuffer());}

	//template<typename T>
	//EResult Write(const char* Name, const T& List)
	//{
	//	CVariant vList;
	//	vList.BeginList();
	//	for(auto I = List.begin(); I != List.end(); ++I)
	//		vList.Write(*I);
	//	vList.Finish();
	//	return WriteVariant(Name, vList);
	//}

	EResult Write(const char* Name, const bool& b)					{return WriteValue(Name, VAR_TYPE_SINT, sizeof(bool), &b);}
	EResult Write(const char* Name, const sint8& sint)				{return WriteValue(Name, VAR_TYPE_SINT, sizeof(sint8), &sint);}
	EResult Write(const char* Name, const uint8& uint)				{return WriteValue(Name, VAR_TYPE_UINT, sizeof(uint8), &uint);}
	EResult Write(const char* Name, const sint16& sint)				{return WriteValue(Name, VAR_TYPE_SINT, sizeof(sint16), &sint);}
	EResult Write(const char* Name, const uint16& uint)				{return WriteValue(Name, VAR_TYPE_UINT, sizeof(uint16), &uint);}
	EResult Write(const char* Name, const sint32& sint)				{return WriteValue(Name, VAR_TYPE_SINT, sizeof(sint32), &sint);}
	EResult Write(const char* Name, const uint32& uint)				{return WriteValue(Name, VAR_TYPE_UINT, sizeof(uint32), &uint);}
	EResult Write(const char* Name, const sint64& sint)				{return WriteValue(Name, VAR_TYPE_SINT, sizeof(sint64), &sint);}
	EResult Write(const char* Name, const uint64& uint)				{return WriteValue(Name, VAR_TYPE_UINT, sizeof(uint64), &uint);}

#ifndef VAR_NO_FPU
	EResult Write(const char* Name, const float& val)				{return WriteValue(Name, VAR_TYPE_DOUBLE, sizeof(float), &val);}
	EResult Write(const char* Name, const double& val)				{return WriteValue(Name, VAR_TYPE_DOUBLE, sizeof(double), &val);}
#endif

	EResult Write(const char* Name, const char* str)				{return WriteValue(Name, VAR_TYPE_ASCII, strlen(str), str);}
	EResult Write(const char* Name, const wchar_t* wstr, size_t len, bool bUtf8 = false)
	{
		EResult Res;
		if (len == -1) len = wcslen(wstr);
		if (bUtf8)
			Res = Write(Name, ToUtf8(m_Buffer.Allocator(), wstr, len));
		else
			Res = WriteValue(Name, VAR_TYPE_UNICODE, len * sizeof(wchar_t), wstr);
		return Res;
	}

	EResult Write(const char* Name, const FW::StringA& str)			{return WriteValue(Name, VAR_TYPE_ASCII, str.Length(), str.ConstData());}
	EResult Write(const char* Name, const FW::StringW& wstr, bool bUtf8 = false) {return Write(Name, wstr.ConstData(), wstr.Length(), bUtf8);}


	//////////////////////////////////////////////////////////////////////////////////////////////
	// List Write Implementation

	//EResult Write(const CBuffer& Buffer)		{return WriteValue(VAR_TYPE_BYTES, Buffer.GetSize(), Buffer.GetBuffer());}

	//template<typename T>
	//EResult Write(const T& List)
	//{
	//	CVariant vList;
	//	vList.BeginList();
	//	for(auto I = List.begin(); I != List.end(); ++I)
	//		vList.Write(*I);
	//	vList.Finish();
	//	return WriteVariant(vList);
	//}

	EResult Write(const bool& b)				{return WriteValue(VAR_TYPE_SINT, sizeof(bool), &b);}
	EResult Write(const sint8& sint)			{return WriteValue(VAR_TYPE_SINT, sizeof(sint8), &sint);}
	EResult Write(const uint8& uint)			{return WriteValue(VAR_TYPE_UINT, sizeof(uint8), &uint);}
	EResult Write(const sint16& sint)			{return WriteValue(VAR_TYPE_SINT, sizeof(sint16), &sint);}
	EResult Write(const uint16& uint)			{return WriteValue(VAR_TYPE_UINT, sizeof(uint16), &uint);}
	EResult Write(const sint32& sint)			{return WriteValue(VAR_TYPE_SINT, sizeof(sint32), &sint);}
	EResult Write(const uint32& uint)			{return WriteValue(VAR_TYPE_UINT, sizeof(uint32), &uint);}
	EResult Write(const sint64& sint)			{return WriteValue(VAR_TYPE_SINT, sizeof(sint64), &sint);}
	EResult Write(const uint64& uint)			{return WriteValue(VAR_TYPE_UINT, sizeof(uint64), &uint);}

#ifndef VAR_NO_FPU
	EResult Write(const float& val)				{return WriteValue(VAR_TYPE_DOUBLE, sizeof(float), &val);}
	EResult Write(const double& val)			{return WriteValue(VAR_TYPE_DOUBLE, sizeof(double), &val);}
#endif

	EResult Write(const char* str)				{return WriteValue(VAR_TYPE_ASCII, strlen(str), str);}
	EResult Write(const wchar_t* wstr, size_t len, bool bUtf8 = false)
	{
		EResult Res;
		if (len == -1) len = wcslen(wstr);
		if (bUtf8) 
			Res = Write(ToUtf8(m_Buffer.Allocator(), wstr, len));
		else
			Res = WriteValue(VAR_TYPE_UNICODE, len * sizeof(wchar_t), wstr);
		return Res;
	}

	EResult Write(const FW::StringA& str)		{return WriteValue(VAR_TYPE_ASCII, str.Length(), str.ConstData());}
	EResult Write(const FW::StringW& wstr, bool bUtf8 = false) {return Write(wstr.ConstData(), wstr.Length(), bUtf8);}

	//////////////////////////////////////////////////////////////////////////////////////////////
	// Index Write Implementation

	//EResult Write(uint32 Index, const CBuffer& Buffer)		{return WriteValue(Index, VAR_TYPE_BYTES, Buffer.GetSize(), Buffer.GetBuffer());}

	//template<typename T>
	//EResult Write(uint32 Index, const T& List)
	//{
	//	CVariant vList;
	//	vList.BeginList();
	//	for(auto I = List.begin(); I != List.end(); ++I)
	//		vList.Write(*I);
	//	vList.Finish();
	//	return WriteVariant(Index, vList);
	//}

	EResult Write(uint32 Index, const bool& b)				{return WriteValue(Index, VAR_TYPE_SINT, sizeof(bool), &b);}
	EResult Write(uint32 Index, const sint8& sint)			{return WriteValue(Index, VAR_TYPE_SINT, sizeof(sint8), &sint);}
	EResult Write(uint32 Index, const uint8& uint)			{return WriteValue(Index, VAR_TYPE_UINT, sizeof(uint8), &uint);}
	EResult Write(uint32 Index, const sint16& sint)			{return WriteValue(Index, VAR_TYPE_SINT, sizeof(sint16), &sint);}
	EResult Write(uint32 Index, const uint16& uint)			{return WriteValue(Index, VAR_TYPE_UINT, sizeof(uint16), &uint);}
	EResult Write(uint32 Index, const sint32& sint)			{return WriteValue(Index, VAR_TYPE_SINT, sizeof(sint32), &sint);}
	EResult Write(uint32 Index, const uint32& uint)			{return WriteValue(Index, VAR_TYPE_UINT, sizeof(uint32), &uint);}
	EResult Write(uint32 Index, const sint64& sint)			{return WriteValue(Index, VAR_TYPE_SINT, sizeof(sint64), &sint);}
	EResult Write(uint32 Index, const uint64& uint)			{return WriteValue(Index, VAR_TYPE_UINT, sizeof(uint64), &uint);}

#ifndef VAR_NO_FPU
	EResult Write(uint32 Index, const float& val)			{return WriteValue(Index, VAR_TYPE_DOUBLE, sizeof(float), &val);}
	EResult Write(uint32 Index, const double& val)			{return WriteValue(Index, VAR_TYPE_DOUBLE, sizeof(double), &val);}
#endif

	EResult Write(uint32 Index, const char* str)			{return WriteValue(Index, VAR_TYPE_ASCII, strlen(str), str);}
	EResult Write(uint32 Index, const wchar_t* wstr, size_t len, bool bUtf8 = false)
	{
		EResult Res;
		if (len == -1) len = wcslen(wstr);
		if (bUtf8) 
			Res = Write(Index, ToUtf8(m_Buffer.Allocator(), wstr, len));
		else
			Res = WriteValue(Index, VAR_TYPE_UNICODE, len*sizeof(wchar_t), wstr);
		return Res;
	}

	EResult Write(uint32 Index, const FW::StringA& str)		{return WriteValue(Index, VAR_TYPE_ASCII, str.Length(), str.ConstData());}
	EResult Write(uint32 Index, const FW::StringW& wstr, bool bUtf8 = false) {return Write(Index, wstr.ConstData(), wstr.Length(), bUtf8);}

protected:
	EResult					BeginWrite(EType Type);

	EType					m_Type = VAR_TYPE_EMPTY;
	CBuffer					m_Buffer;
};


FW_NAMESPACE_END