#pragma once

#define VAR_NO_EXCEPTIONS

//#define VAR_NO_FPU

#include "Buffer.h"
#include "VariantDefs.h"
//#include "../Core/Memory.h"
#include "../Core/SafeRef.h"
#include "../Core/RetValue.h"
#include "../Core/String.h"
#include "../Core/Map.h"
#include "../Core/Array.h"
#include "../Core/Table.h"


union _TVarIndex
{
	uint32 u;
	char c[4];
};

__inline bool operator < (_TVarIndex l, _TVarIndex r) { return l.u < r.u; }
__inline bool operator == (_TVarIndex l, _TVarIndex r) { return l.u == r.u; }

struct SVarName
{
	const char* Buf;
	size_t Len;
};

#define VAR_DATA_SIZE 8

#define VAR_TEST_NAME(Name,Test) (Name.Len == (sizeof(Test) - 1) && memcmp(Test, Name.Buf, Name.Len) == 0)

FW_NAMESPACE_BEGIN

class FRAMEWORK_EXPORT CVariant : public AbstractContainer
{
public:

	typedef VAR_TYPE EType;

	enum EResult
	{
		eErrNone = 0,
		eErrNotWritable,
		eErrWriteInProgrees,
		eErrWriteNotReady,
		eErrInvalidName,
		eErrTypeMismatch,
		eErrNotFound,
		eErrBufferShort,
		eErrAllocFailed,
		eErrIsEmpty,
		eErrInvalidHeader,
		eErrBufferWriteFailed,
		eErrIntegerOverflow,
		eErrIndexOutOfBounds,
	};

	static const char*		ErrorString(EResult Error);

	explicit CVariant(FW::AbstractMemPool* pMemPool = nullptr, EType Type = VAR_TYPE_EMPTY);
	CVariant(const CVariant& Variant);
	CVariant(CVariant&& Variant);
	~CVariant();

	EResult					FromPacket(const CBuffer* pPacket, bool bDerived = false);
	EResult					ToPacket(CBuffer* pPacket) const;

	CVariant& operator=(const CVariant& Other)			{Clear(); EResult Err = InitAssign(Other); if (Err) Throw(Err); return *this;}
	CVariant& operator=(CVariant&& Other)				{Clear(); InitMove(Other); return *this;}

	int						Compare(const CVariant &R) const;

	bool operator==(const CVariant &Variant) const		{return Compare(Variant) == 0;}
	bool operator!=(const CVariant &Variant) const		{return Compare(Variant) != 0;}

	EResult					Freeze();
	EResult					Unfreeze();
	bool					IsFrozen() const { return m_bReadOnly; } // Is frozen (read only)

	uint32					Count() const;													// Map, List or Index

	bool					IsMap() const { return m_Type == VAR_TYPE_MAP; }				// Map
	const char*				Key(uint32 Index) const;										// Map
	FW::RetValue<const CVariant*, EResult> PtrAt(const char* Name) const;					// Map
	FW::RetValue<CVariant*, EResult> PtrAt(const char* Name, bool bCanAdd);					// Map
	CVariant				Get(const char* Name) const;									// Map
	bool					Has(const char* Name) const;									// Map
	EResult					Remove(const char* Name);										// Map
	EResult					Insert(const char* Name, const CVariant& Variant);				// Map

	bool					IsList() const { return m_Type == VAR_TYPE_LIST; }				// List
	bool					IsIndex() const { return m_Type == VAR_TYPE_INDEX; }			// Index
	uint32					Id(uint32 Index) const;											// Index
	FW::RetValue<const CVariant*, EResult> PtrAt(uint32 Index) const;						// List or Index
	FW::RetValue<CVariant*, EResult> PtrAt(uint32 Index, bool bCanAdd);						// List or Index
	CVariant				Get(uint32 Index) const;										// List or Index
	bool					Has(uint32 Index) const;										// Index
	EResult					Remove(uint32 Index);											// List or Index
	EResult					Append(const CVariant& Variant);								// List
	EResult					Insert(uint32 Index, const CVariant& Variant);					// Index

	EResult					Merge(const CVariant& Variant);

	struct SContainer
	{
		volatile LONG Refs;
		//uint32 uUnused;
		byte* pRawPayload;
	}; // 16

	struct SMap: SContainer
	{
		typedef FW::Table<FW::StringA, CVariant, FW::DefaultHasher<FW::StringA>, FW::DefaultContainer<CVariant>> Table;
		typedef Table::Iterator Iterator;

		uint32 Count;
		uint32 BucketCount;
#pragma warning( push )
#pragma warning( disable : 4200)
		Table::SBucketEntry* Buckets[0];
#pragma warning( pop )
	};
	typedef SMap TMap;

	struct SList: SContainer
	{
		uint32 Count;
		uint32 Capacity;
#pragma warning( push )
#pragma warning( disable : 4200)
		PVOID Data[0];
#pragma warning( pop )
	};
	typedef SList TList;

	typedef _TVarIndex TKey;
	struct SIndex: SContainer
	{
		typedef FW::Table<TKey, CVariant, FW::DefaultHasher<TKey>, FW::DefaultContainer<CVariant>> Table;
		typedef Table::Iterator Iterator;

		uint32 Count;
		uint32 BucketCount;
#pragma warning( push )
#pragma warning( disable : 4200)
		Table::SBucketEntry* Buckets[0];
#pragma warning( pop )
	};
	typedef SIndex TIndex;

	// AbstractContainer::m_pMem;
	// 8
	EType		m_Type;
	union {
		uint8	m_uFlags;
		struct {
			uint8		
				m_uEmbeddedSize	: 4,	// payload is embedded in the variant body startng at m_Embedded[0]
				m_bContainer	: 1,
				m_bRawPayload	: 1,	// m_p::RawPayload is valid
				m_bDerived		: 1,	// m_p::RawPayload points to an external buffer, don't change or free!!!
				m_bReadOnly		: 1;	// don't allow alteration
		};
	};
	char		m_Embedded[2];			// 2 + 4 + VAR_DATA_SIZE = 14
	uint32		m_uPayloadSize;			// size of the RawPayload
	// 16
	union{
		byte    Data[VAR_DATA_SIZE];

		void*   Void;
		byte*	RawPayload;

		SContainer* Container;
		SMap*   Map;
		SList*  List;
		SIndex* Index;

		CBuffer::SBuffer*		Buffer;
		FW::StringA::SStringData* StrA;
		FW::StringW::SStringData* StrW;
	}			m_p; 
	// 24


#ifndef NO_CRT
	explicit CVariant(const byte* Payload, size_t Size, EType Type = VAR_TYPE_BYTES) : CVariant((FW::AbstractMemPool*)nullptr, Payload, Size, Type) {}
#endif
	explicit CVariant(FW::AbstractMemPool* pMemPool, const byte* Payload, size_t Size, EType Type = VAR_TYPE_BYTES) : FW::AbstractContainer(pMemPool) {InitValue(Type, Size, Payload);}
	operator const byte* () const						{return GetData();}
	const byte*				GetData() const;
	uint32					GetSize() const;
	EType					GetType() const				{return m_Type;}
	bool					IsValid() const				{return m_Type != VAR_TYPE_EMPTY;}

	void					Clear();
	EResult					Assign(const CVariant& From){Clear(); return InitAssign(From);}

	CVariant				Clone(bool Full = true) const;

protected:
	friend class CVariantReader;
	friend class CVariantWriter;

	EResult					InitValue(EType Type, size_t Size, const void* Value, bool bTake = false);

	EResult					InitAssign(const CVariant& From);
	void					InitMove(CVariant& From);

	void					InitFromContainer(SContainer* pContainer, EType Type);
	EResult					InitFromBuffer(CBuffer::SBuffer* pBuffer, size_t uSize);
	EResult					InitFromStringA(FW::StringA::SStringData* pStrA, bool bUtf8 = false);
	EResult					InitFromStringW(FW::StringW::SStringData* pStrW);

	void					DetachPayload();

	static uint32			ReadHeader(const CBuffer* pPacket, EType* pType = NULL);
	static EResult			ToPacket(CBuffer* pPacket, EType Type, size_t uLength, const void* Value);

	static EResult			Throw(EResult Error);

	bool					IsRaw() const				{return m_Type != VAR_TYPE_MAP && m_Type != VAR_TYPE_LIST && m_Type != VAR_TYPE_INDEX;}

	FW::RetValue<TMap*, EResult>	GetMap(bool bCanInit, bool bExclusive) const;
	FW::RetValue<TList*, EResult>	GetList(bool bCanInit, bool bExclusive) const;
	FW::RetValue<TIndex*, EResult>	GetIndex(bool bCanInit, bool bExclusive) const;

	EResult					MkPayload(CBuffer* pPayload) const;

	static EResult			Clone(const CVariant* From, CVariant* To, bool Full = true);

	// Containers

	TMap*  					AllocMap(uint32 BucketCount = 4) const;
	TMap*					CloneMap(const TMap* pMap, bool Full) const;
	CVariant*				MapInsert(TMap*& pMap, const char* Name, size_t Len, const CVariant* pVariant, bool bUnsafe = false) const;
	void					MapRemove(TMap* pMap, const char* Name) const;
	static TMap::Iterator	MapFind(TMap* pMap, const char* Name);
	static TMap::Iterator	MapBegin(const TMap* pMap) 	{return TMap::Iterator(((TMap*)pMap)->Buckets, pMap->BucketCount);}
	static TMap::Iterator	MapEnd() 					{return TMap::Iterator();}
	void					FreeMap(TMap* pMap) const;

	TList* 					AllocList(uint32 Capacity = 4) const;
	TList*					CloneList(const TList* pList, bool Full) const;
	CVariant*				ListAppend(TList*& pList, const CVariant* pVariant) const;
	void					ListRemove(TList* pList, uint32 Index) const;
	void					FreeList(TList* pList) const;

	TIndex* 				AllocIndex(uint32 BucketCount = 4) const;
	TIndex* 				CloneIndex(const TIndex* pIndex, bool Full) const;
	CVariant*				IndexInsert(TIndex*& pIndex, uint32 Index, const CVariant* pVariant, bool bUnsafe = false) const;
	void					IndexRemove(TIndex* pIndex, uint32 Index) const;
	static TIndex::Iterator	IndexFind(const TIndex* pIndex, uint32 Index);
	static TIndex::Iterator	IndexBegin(const TIndex* pIndex) {return TIndex::Iterator(((TIndex*)pIndex)->Buckets, pIndex->BucketCount);}
	static TIndex::Iterator	IndexEnd() 					{return TIndex::Iterator();}
	void					FreeIndex(TIndex* pIndex) const;

	CBuffer::SBuffer*		AllocBuffer(size_t uSize) const;

	FW::StringA::SStringData* AllocStringA(size_t uSize) const;
	FW::StringW::SStringData* AllocStringW(size_t uSize) const;

public:

	//////////////////////////////////////////////////////////////////////////////////////////////
	// Built in Machine Types

#ifndef NO_CRT
	explicit CVariant(bool b) : CVariant((FW::AbstractMemPool*)nullptr, b)			{}
	explicit CVariant(sint8 sint) : CVariant((FW::AbstractMemPool*)nullptr, sint)	{}
	explicit CVariant(uint8 uint) : CVariant((FW::AbstractMemPool*)nullptr, uint)	{}
	explicit CVariant(sint16 sint) : CVariant((FW::AbstractMemPool*)nullptr, sint)	{}
	explicit CVariant(uint16 uint) : CVariant((FW::AbstractMemPool*)nullptr, uint)	{}
	explicit CVariant(sint32 sint) : CVariant((FW::AbstractMemPool*)nullptr, sint)	{}
	explicit CVariant(uint32 uint) : CVariant((FW::AbstractMemPool*)nullptr, uint)	{}
	explicit CVariant(sint64 sint) : CVariant((FW::AbstractMemPool*)nullptr, sint)	{}
	explicit CVariant(uint64 uint) : CVariant((FW::AbstractMemPool*)nullptr, uint)	{}
#ifndef VAR_NO_FPU
	explicit CVariant(float val) : CVariant((FW::AbstractMemPool*)nullptr, val)	{}
	explicit CVariant(double val) : CVariant((FW::AbstractMemPool*)nullptr, val)	{}
#endif
#endif
	explicit CVariant(FW::AbstractMemPool* pMemPool, bool b) : FW::AbstractContainer(pMemPool)		{InitValue(VAR_TYPE_SINT, sizeof(bool), &b);}
	explicit CVariant(FW::AbstractMemPool* pMemPool, sint8 sint) : FW::AbstractContainer(pMemPool)	{InitValue(VAR_TYPE_SINT, sizeof(sint8), &sint);}
	explicit CVariant(FW::AbstractMemPool* pMemPool, uint8 uint) : FW::AbstractContainer(pMemPool)	{InitValue(VAR_TYPE_UINT, sizeof(uint8), &uint);}
	explicit CVariant(FW::AbstractMemPool* pMemPool, sint16 sint) : FW::AbstractContainer(pMemPool)	{InitValue(VAR_TYPE_SINT, sizeof(sint16), &sint);}
	explicit CVariant(FW::AbstractMemPool* pMemPool, uint16 uint) : FW::AbstractContainer(pMemPool)	{InitValue(VAR_TYPE_UINT, sizeof(uint16), &uint);}
	explicit CVariant(FW::AbstractMemPool* pMemPool, sint32 sint) : FW::AbstractContainer(pMemPool)	{InitValue(VAR_TYPE_SINT, sizeof(sint32), &sint);}
	explicit CVariant(FW::AbstractMemPool* pMemPool, uint32 uint) : FW::AbstractContainer(pMemPool)	{InitValue(VAR_TYPE_UINT, sizeof(uint32), &uint);}
	explicit CVariant(FW::AbstractMemPool* pMemPool, sint64 sint) : FW::AbstractContainer(pMemPool)	{InitValue(VAR_TYPE_SINT, sizeof(sint64), &sint);}
	explicit CVariant(FW::AbstractMemPool* pMemPool, uint64 uint) : FW::AbstractContainer(pMemPool)	{InitValue(VAR_TYPE_UINT, sizeof(uint64), &uint);}
#ifndef VAR_NO_FPU
	explicit CVariant(FW::AbstractMemPool* pMemPool, float val) : FW::AbstractContainer(pMemPool)	{InitValue(VAR_TYPE_DOUBLE, sizeof(float), &val);}
	explicit CVariant(FW::AbstractMemPool* pMemPool, double val) : FW::AbstractContainer(pMemPool)	{InitValue(VAR_TYPE_DOUBLE, sizeof(double), &val);}
#endif

	//CVariant& operator=(bool b)						{DetachPayload(); InitValue(VAR_TYPE_SINT, sizeof(bool), &b); return *this;}
	CVariant& operator=(sint8 sint)						{DetachPayload(); InitValue(VAR_TYPE_SINT, sizeof(sint8), &sint); return *this;}
	CVariant& operator=(uint8 uint)						{DetachPayload(); InitValue(VAR_TYPE_UINT, sizeof(uint8), &uint); return *this;}
	CVariant& operator=(sint16 sint)					{DetachPayload(); InitValue(VAR_TYPE_SINT, sizeof(sint16), &sint); return *this;}
	CVariant& operator=(uint16 uint)					{DetachPayload(); InitValue(VAR_TYPE_UINT, sizeof(uint16), &uint); return *this;}
	CVariant& operator=(sint32 sint)					{DetachPayload(); InitValue(VAR_TYPE_SINT, sizeof(sint32), &sint); return *this;}
	CVariant& operator=(uint32 uint)					{DetachPayload(); InitValue(VAR_TYPE_UINT, sizeof(uint32), &uint); return *this;}
	CVariant& operator=(sint64 sint)					{DetachPayload(); InitValue(VAR_TYPE_SINT, sizeof(sint64), &sint); return *this;}
	CVariant& operator=(uint64 uint)					{DetachPayload(); InitValue(VAR_TYPE_UINT, sizeof(uint64), &uint); return *this;}
#ifndef VAR_NO_FPU
	CVariant& operator=(float val)						{DetachPayload(); InitValue(VAR_TYPE_DOUBLE, sizeof(float), &val); return *this;}
	CVariant& operator=(double val)						{DetachPayload(); InitValue(VAR_TYPE_DOUBLE, sizeof(double), &val); return *this;}
#endif

	operator bool() const								{bool b = false; ToInt(sizeof(bool), &b); return b;}
	operator sint8() const								{sint8 sint = 0; ToInt(sizeof(sint8), &sint); return sint;}
	operator uint8() const								{uint8 uint = 0; ToInt(sizeof(uint8), &uint); return uint;}
	operator sint16() const								{sint16 sint = 0; ToInt(sizeof(sint16), &sint); return sint;}
	operator uint16() const								{uint16 uint = 0; ToInt(sizeof(uint16), &uint); return uint;}
	operator sint32() const								{sint32 sint = 0; ToInt(sizeof(sint32), &sint); return sint;}
	operator uint32() const								{uint32 uint = 0; ToInt(sizeof(uint32), &uint); return uint;}
	operator sint64() const								{sint64 sint = 0; ToInt(sizeof(sint64), &sint); return sint;}
	operator uint64() const								{uint64 uint = 0; ToInt(sizeof(uint64), &uint); return uint;}
#ifndef VAR_NO_FPU
	operator float() const								{float val = 0; ToDouble(sizeof(float), &val); return val;}
	operator double() const								{double val = 0; ToDouble(sizeof(double), &val); return val;}
#endif

	//////////////////////////////////////////////////////////////////////////////////////////////
	// Byte Array

#ifndef KERNEL_MODE
	explicit CVariant(const CBuffer& Buffer) : FW::AbstractContainer(Buffer.Allocator() ? Buffer.Allocator() : (FW::AbstractMemPool*)nullptr) {
#else
	explicit CVariant(const CBuffer& Buffer) : FW::AbstractContainer(Buffer.Allocator()) {
#endif
		if (Buffer.IsContainerized())
			InitFromBuffer(Buffer.m_p.Buffer, Buffer.m_uSize);
		else
			InitValue(VAR_TYPE_BYTES, Buffer.GetSize(), Buffer.GetBuffer());
	}
#ifndef KERNEL_MODE
	explicit CVariant(CBuffer& Buffer, bool bTake) : FW::AbstractContainer(Buffer.Allocator() ? Buffer.Allocator() : (FW::AbstractMemPool*)nullptr) {
#else
	explicit CVariant(CBuffer& Buffer, bool bTake) : FW::AbstractContainer(Buffer.Allocator()) {
#endif
		if (Buffer.IsContainerized())
			InitFromBuffer(Buffer.m_p.Buffer, Buffer.m_uSize);
		else {
			size_t uSize = Buffer.GetSize();
			InitValue(VAR_TYPE_BYTES, uSize, Buffer.GetBuffer(bTake), bTake);
		}
	}
	CVariant& operator=(const CBuffer& Buffer) {
		DetachPayload();
		if (Buffer.IsContainerized())
			InitFromBuffer(Buffer.m_p.Buffer, Buffer.m_uSize);
		else 
			InitValue(VAR_TYPE_BYTES, Buffer.GetSize(), Buffer.GetBuffer());
		return *this;
	}

	CBuffer ToBuffer(bool bShared = false) const;
	operator CBuffer() const							{return ToBuffer();}

	//////////////////////////////////////////////////////////////////////////////////////////////
	// ASCII String

#ifndef NO_CRT
	explicit CVariant(const char* str, size_t len = StringA::NPos) : CVariant((FW::AbstractMemPool*)nullptr, str, len) {}
#endif
	explicit CVariant(FW::AbstractMemPool* pMemPool, const char* str, size_t len = StringA::NPos) : FW::AbstractContainer(pMemPool) {
		if (len == StringA::NPos) len = strlen(str);
		InitValue(VAR_TYPE_ASCII, len, str);
	}
	explicit CVariant(const FW::StringA& str) : FW::AbstractContainer(str.Allocator()) {InitFromStringA(str.m_ptr);}
	CVariant& operator=(const FW::StringA& str)			{DetachPayload(); InitFromStringA(str.m_ptr); return *this;}
	CVariant& operator=(const char* str)				{DetachPayload();  InitValue(VAR_TYPE_ASCII, strlen(str), str); return *this;}

	FW::StringA ToStringA(bool bShared = false) const;
	operator FW::StringA() const {return ToStringA();}

	//////////////////////////////////////////////////////////////////////////////////////////////
	// WCHAR String
#ifndef NO_CRT
	explicit CVariant(const wchar_t* wstr, size_t len = StringW::NPos, bool bUtf8 = false) : CVariant((FW::AbstractMemPool*)nullptr, wstr, len, bUtf8) {}
#endif
	explicit CVariant(FW::AbstractMemPool* pMemPool, const wchar_t* wstr, size_t len = StringW::NPos, bool bUtf8 = false) : FW::AbstractContainer(pMemPool) {
		if (len == StringW::NPos) len = wcslen(wstr);
		if (bUtf8)
			InitFromStringA(ToUtf8(pMemPool, wstr, len).m_ptr);
		else
			InitValue(VAR_TYPE_UNICODE, len * sizeof(wchar_t), wstr);
	}
	explicit CVariant(const FW::StringW& wstr, bool bUtf8 = false) : FW::AbstractContainer(wstr.Allocator()) { 
		if (bUtf8)
			InitFromStringA(ToUtf8(wstr.Allocator(), wstr.ConstData(), wstr.Length()).m_ptr, true);
		else
			InitFromStringW(wstr.m_ptr); 
	}
	CVariant& operator=(const FW::StringW& wstr)		{DetachPayload(); InitFromStringW(wstr.m_ptr); return *this;}
	CVariant& operator=(const wchar_t* wstr)			{DetachPayload(); InitValue(VAR_TYPE_UNICODE, wcslen(wstr)*sizeof(wchar_t), wstr);  return *this;}

	FW::StringW ToStringW(bool bShared = false) const;
	operator FW::StringW() const {return ToStringW();}


	//////////////////////////////////////////////////////////////////////////////////////////////
	// Type Conversion

	template <class T> 
	T To(const T& Default) const {if (!IsValid()) return Default; return *this;} // tix me TODO dont use cast as cast can throw!!!! get mustbe safe
	template <class T> 
	T To() const {return *this;}

	void ToInt(size_t Size, void* Value) const;
	EResult GetInt(size_t Size, void* Value) const;
#ifndef VAR_NO_FPU
	void ToDouble(size_t Size, void* Value) const;
	EResult GetDouble(size_t Size, void* Value) const;
#endif

	
	EResult GetBuffer(CBuffer& Buffer, bool bShared = false) const;
	EResult GetStringA(FW::StringA& StrA, bool bShared = false) const;
	EResult GetStringW(FW::StringW& StrW, bool bShared = false) const;
};

__inline bool operator<(const CVariant &L, const CVariant &R) {return L.Compare(R) > 0;}

template <typename V>
class CVariantRef
{
public:
	CVariantRef(const V* ptr = nullptr) : m_ptr((V*)ptr) {}
	CVariantRef(const CVariantRef& ptr) : m_ptr(ptr.m_ptr) {}

	CVariantRef& operator=(const CVariantRef& ptr)		{m_ptr = ptr.m_ptr; return *this;}
	CVariantRef& operator=(const V& val)				{if (m_ptr) *m_ptr = val; return *this;}

	operator bool() const								{if(!m_ptr) return false; bool b = false; m_ptr->ToInt(sizeof(bool), &b); return b;}
	operator sint8() const								{if(!m_ptr) return 0; sint8 sint = 0; m_ptr->ToInt(sizeof(sint8), &sint); return sint;}
	operator uint8() const								{if(!m_ptr) return 0; uint8 uint = 0; m_ptr->ToInt(sizeof(uint8), &uint); return uint;}
	operator sint16() const								{if(!m_ptr) return 0; sint16 sint = 0; m_ptr->ToInt(sizeof(sint16), &sint); return sint;}
	operator uint16() const								{if(!m_ptr) return 0; uint16 uint = 0; m_ptr->ToInt(sizeof(uint16), &uint); return uint;}
	operator sint32() const								{if(!m_ptr) return 0; sint32 sint = 0; m_ptr->ToInt(sizeof(sint32), &sint); return sint;}
	operator uint32() const								{if(!m_ptr) return 0; uint32 uint = 0; m_ptr->ToInt(sizeof(uint32), &uint); return uint;}
	operator sint64() const								{if(!m_ptr) return 0; if(!m_ptr) return 0; sint64 sint = 0; m_ptr->ToInt(sizeof(sint64), &sint); return sint;}
	operator uint64() const								{if(!m_ptr) return 0; uint64 uint = 0; m_ptr->ToInt(sizeof(uint64), &uint); return uint;}
#ifndef VAR_NO_FPU
	operator float() const								{if(!m_ptr) return 0.0F; float val = 0; m_ptr->ToDouble(sizeof(float), &val); return val;}
	operator double() const								{if(!m_ptr) return 0.0; double val = 0; m_ptr->ToDouble(sizeof(double), &val); return val;}
#endif
	
	operator const byte*() const						{if(!m_ptr) return nullptr; return m_ptr->GetData();}

	operator CBuffer() const							{if(!m_ptr) return CBuffer((FW::AbstractMemPool*)nullptr); return m_ptr->ToBuffer();}

	operator FW::StringA() const						{if(!m_ptr) return FW::StringA((FW::AbstractMemPool*)nullptr); return m_ptr->ToStringA();}
	operator FW::StringW() const						{if(!m_ptr) return FW::StringW((FW::AbstractMemPool*)nullptr); return m_ptr->ToStringW();}

	template <class T> 
	T To(const T& Default) const						{if (!m_ptr || !m_ptr->IsValid()) return Default; return m_ptr->To<T>();}
	template <class T> 
	T To() const										{if (!m_ptr) return T(); return m_ptr->To<T>();}

	CVariant::EResult GetStringA(FW::StringA& StrA) const {if (!m_ptr) return CVariant::eErrIsEmpty; return m_ptr->GetStringA(StrA);}
	CVariant::EResult GetStringW(FW::StringW& StrW) const {if (!m_ptr) return CVariant::eErrIsEmpty; return m_ptr->GetStringW(StrW);}

	FW::StringA ToStringA() const						{FW::StringA StrA; GetStringA(StrA); return StrA;}
	FW::StringW ToStringW() const						{FW::StringW StrW; GetStringW(StrW); return StrW;}

	V* Ptr() const										{return m_ptr;}

protected:
	friend V;
	V* m_ptr = nullptr;
};

FW_NAMESPACE_END


class CVariant : public FW::CVariant
{
public:
	explicit CVariant(FW::AbstractMemPool* pMemPool = nullptr, EType Type = VAR_TYPE_EMPTY) : FW::CVariant(pMemPool, Type) {  }
	CVariant(const FW::CVariant& Variant) : FW::CVariant(Variant) {}
	CVariant(const CVariant& Variant) : FW::CVariant(Variant) {}
	CVariant(CVariant&& Variant) : FW::CVariant(Variant.Allocator()) {InitMove(Variant);}
	//CVariant(FW::CVariant&& Variant) : FW::CVariant(*(CVariant*)&Variant) {}

#ifndef NO_CRT
	explicit CVariant(const byte* Payload, size_t Size, EType Type = VAR_TYPE_BYTES) : FW::CVariant((FW::AbstractMemPool*)nullptr, Payload, Size, Type) {}
#endif
	explicit CVariant(FW::AbstractMemPool* pMemPool, const byte* Payload, size_t Size, EType Type = VAR_TYPE_BYTES) : FW::CVariant(pMemPool, Payload, Size, Type) {}
	explicit CVariant(const FW::StringW& wstr, bool bUtf8 = false) : FW::CVariant(wstr, bUtf8) {}

//#ifndef NO_CRT
	template<typename T>
	CVariant(const T& val) : FW::CVariant(val) {}
//#endif
	template<typename T>
	CVariant(FW::AbstractMemPool* pMemPool, const T& val) : FW::CVariant(pMemPool, val) {}

	template <typename T>
	CVariant& operator=(const T& val)					{return (CVariant&)FW::CVariant::operator=(val);}

	CVariant& operator=(const CVariant& Other)			{Clear(); InitAssign(Other); return *this;}
	CVariant& operator=(CVariant&& Other)				{Clear(); InitMove(Other); return *this;}
	//CVariant& operator=(FW::CVariant&& Other)			{Clear(); InitMove(Other); return *this;}

	class Ref : public FW::CVariantRef<CVariant>
	{
	public:
		Ref(const FW::CVariant* ptr = nullptr) : FW::CVariantRef<CVariant>((CVariant*)ptr) {}
		Ref(const CVariant* ptr = nullptr) : FW::CVariantRef<CVariant>(ptr) {}

		template <typename T>
		Ref& operator=(const T& val)					{if (m_ptr) m_ptr->operator=(val); return *this;}

		operator FW::CVariant() const					{if (m_ptr) return FW::CVariant(*m_ptr); return FW::CVariant();}
	};

	CVariant(const Ref& Other) : FW::CVariant(Other.m_ptr ? Other.m_ptr->m_pMem : nullptr) 
														{if (Other.m_ptr) Assign(*Other.Ptr());}

	CVariant& operator=(const Ref& Other)				{Clear(); if (Other.m_ptr) InitAssign(*Other.Ptr()); return *this;}

	const Ref				At(const char* Name) const	{auto Ret = PtrAt(Name); if (Ret.Error) Throw(Ret.Error); return Ret.Error ? NULL : Ret.Value;}
	const Ref operator[](const char* Name) const		{return At(Name);}	// Map
	Ref						At(const char* Name)		{auto Ret = PtrAt(Name, true); if (Ret.Error) Throw(Ret.Error); return Ret.Error ? NULL : Ret.Value;}
	Ref operator[](const char* Name)					{return At(Name);}	// Map

	const Ref				At(uint32 Index) const		{auto Ret = PtrAt(Index); if (Ret.Error) Throw(Ret.Error); return Ret.Error ? NULL : Ret.Value;}
	const Ref operator[](uint32 Index)	const			{return At(Index);} // List or Index
	Ref						At(uint32 Index)			{auto Ret = PtrAt(Index, true); if (Ret.Error) Throw(Ret.Error); return Ret.Error ? NULL : Ret.Value;}
	Ref operator[](uint32 Index)						{return At(Index);} // List or Index

};
