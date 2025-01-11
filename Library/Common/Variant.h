#pragma once
#include "../lib_global.h"

#define VAR_NO_EXCEPTIONS

#define VAR_NO_STL
//#define VAR_NO_STL_STR
//#define VAR_NO_STL_VECT
//#define VAR_NO_FPU

#ifndef VAR_NO_STL_STR
#include <string>
#endif
#ifndef VAR_NO_STL
#include <vector>
#include <map>
#include <atomic>
#endif
#ifndef VAR_NO_STL_VECT
#include <vector>
#endif
//#include <functional>
//#include <cstdlib>

#include "Buffer.h"
#include "VariantDefs.h"
//#include "../../Framework/Memory.h"
#include "../../Framework/SafeRef.h"
#include "../../Framework/RetValue.h"
#ifdef VAR_NO_STL
#include "../../Framework/String.h"
#include "../../Framework/Map.h"
#include "../../Framework/Array.h"
#endif


union _TVarIndex
{
	uint32 u;
	char c[4];
};

__inline bool operator < (_TVarIndex l, _TVarIndex r) { return l.u < r.u; }

struct SVarName
{
	const char* Buf;
	size_t Len;
};

#define VAR_TEST_NAME(Name,Test) (Name.Len == (sizeof(Test) - 1) && memcmp(Test, Name.Buf, Name.Len) == 0)

class LIBRARY_EXPORT CVariant : public FW::AbstractContainer
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

	class Ref
	{
	public:
		Ref(const CVariant* ptr = nullptr) : m_ptr((CVariant*)ptr) {}
		Ref(const Ref& ptr) : m_ptr(ptr.m_ptr) {}

		Ref& operator=(const Ref& ptr)	{ m_ptr = ptr.m_ptr; return *this; }
		Ref& operator=(const CVariant& val)	{ if (m_ptr) *m_ptr = val; return *this; }

		operator CVariant() const		{ if (m_ptr) return *m_ptr; return CVariant((FW::AbstractMemPool*)nullptr); }

		//const CVariant* operator&() const {return m_ptr;}
		//CVariant* operator&()			{return m_ptr;}

		//const CVariant& operator*() const {return *m_ptr;}
		//CVariant& operator*()			{return *m_ptr;}

		//const CVariant* operator->() const { return m_ptr; }
		//CVariant* operator-> ()			{ return m_ptr; }

		template <typename T>			// asign anything operator - causes ambiguity
		Ref& operator=(const T& val)	{ if (m_ptr) *m_ptr = val; return *this; }

		// assign from known types operator

		//template <typename T>			// assign to anything operator - causes ambiguity
		//operator T() const { if (m_ptr) return (T)*m_ptr; return T(); }

		// assign to known types operator
		operator const byte*() const	{ if (!m_ptr) return NULL; return (byte*)*m_ptr; }
		operator const char*() const	{ if (!m_ptr || m_ptr->GetType() != VAR_TYPE_ASCII) return NULL; return (const char*)(byte*)*m_ptr; }

		operator CBuffer() const		{ if (!m_ptr) return CBuffer(nullptr); return (CBuffer)*m_ptr; }

		operator bool() const			{ if (!m_ptr) return false; return (bool)*m_ptr; }
		operator sint8() const			{ if (!m_ptr) return 0; return (sint8)*m_ptr; }
		operator uint8() const			{ if (!m_ptr) return 0; return (uint8)*m_ptr; }
		operator sint16() const			{ if (!m_ptr) return 0; return (sint16)*m_ptr; }
		operator uint16() const			{ if (!m_ptr) return 0; return (uint16)*m_ptr; }
		operator sint32() const			{ if (!m_ptr) return 0; return (sint32)*m_ptr; }
		operator uint32() const			{ if (!m_ptr) return 0; return (uint32)*m_ptr; }
#ifndef WIN32
		operator time_t() const			{ if (!m_ptr) return 0; return (sint64)*m_ptr; }
#endif
		operator sint64() const			{ if (!m_ptr) return 0; return (sint64)*m_ptr; }
		operator uint64() const			{ if (!m_ptr) return 0; return (uint64)*m_ptr; }

#ifndef VAR_NO_FPU
		operator float() const			{ if (!m_ptr) return 0.0F; return (float)*m_ptr; }
		operator double() const			{ if (!m_ptr) return 0.0; return (double)*m_ptr; }
#endif

#ifndef VAR_NO_STL_STR
		operator std::string() const	{ if (!m_ptr) return "";  return m_ptr->ToString(); }
		operator std::wstring() const	{ if (!m_ptr) return L"";  return m_ptr->ToWString(); }
#endif

		operator FW::StringA() const	{ if (!m_ptr) return FW::StringA(nullptr);  return m_ptr->ToStringA(); }
		operator FW::StringW() const	{ if (!m_ptr) return FW::StringW(nullptr);  return m_ptr->ToStringW(); }

		template <class T> 
		T To(const T& Default) const	{if (!m_ptr || !m_ptr->IsValid()) return Default; return (T)*m_ptr;}
		template <class T> 
		T To() const					{ if (!m_ptr) return T(); return (T)*m_ptr; }

#ifndef VAR_NO_STL_STR
		std::wstring AsStr(bool* pOk = NULL) const { if (m_ptr) return m_ptr->AsStr(pOk); if (pOk) *pOk = false; return L""; }
#endif

#ifndef VAR_NO_STL_VECT
		std::vector<byte> AsBytes() const { if (m_ptr) return m_ptr->AsBytes(); return std::vector<byte>(); }
#endif

#ifndef VAR_NO_STL_VECT
		template<typename T>
		std::vector<T> AsList() const	{ if (m_ptr) return m_ptr->AsList<T>(); return std::vector<T>(); }
#endif

		CVariant* Ptr() const			{ return m_ptr; }

	protected:
		friend class CVariant;
		CVariant* m_ptr = nullptr;
	};

//#ifdef KERNEL_MODE
//	explicit CVariant(FW::AbstractMemPool* pMemPool, EType Type = VAR_TYPE_EMPTY);
//#else
	explicit CVariant(FW::AbstractMemPool* pMemPool = nullptr, EType Type = VAR_TYPE_EMPTY);
//#endif
	CVariant(const CVariant& Variant);
	~CVariant();

	EResult					FromPacket(const CBuffer* pPacket, bool bDerived = false);
	EResult					ToPacket(CBuffer* pPacket/*, bool bPack = false*/) const;

	CVariant& operator=(const CVariant& Other) {
		Clear();
		if (!m_pMem)
			m_pMem = Other.Allocator();
		Assign(Other); 
		return *this;
	}
	CVariant& operator=(const Ref& Other) {
		Clear();
		if (Other.m_ptr) {
			if (!m_pMem)
				m_pMem = Other.m_ptr->Allocator();
			Assign(Other);
		}
		return *this;
	}

	int						Compare(const CVariant &R) const;

	bool operator==(const CVariant &Variant) const						{return Compare(Variant) == 0;}
	bool operator!=(const CVariant &Variant) const						{return Compare(Variant) != 0;}

	EResult					Freeze();
	EResult					Unfreeze();
	bool					IsFrozen() const;

	uint32					Count() const;													// Map, List or Index

	bool					IsMap() const;													// Map
	const char*				Key(uint32 Index) const;										// Map
#ifndef VAR_NO_STL_STR
	std::wstring			WKey(uint32 Index) const;										// Map
#endif
	FW::RetValue<const CVariant*, EResult> PtrAt(const char* Name) const;					// Map
	FW::RetValue<CVariant*, EResult> PtrAt(const char* Name, bool bCanAdd);					// Map
	const Ref				At(const char* Name) const { auto Ret = PtrAt(Name); if (Ret.Error) Throw(Ret.Error); return Ret.Error ? NULL : Ret.Value; }
	Ref						At(const char* Name) { auto Ret = PtrAt(Name, true); if (Ret.Error) Throw(Ret.Error); return Ret.Error ? NULL : Ret.Value; }
	CVariant				Get(const char* Name) const;									// Map
	bool					Has(const char* Name) const;									// Map
	EResult					Remove(const char* Name);										// Map
	EResult					Insert(const char* Name, const CVariant& Variant);				// Map

	bool					IsList() const;													// List
	bool					IsIndex() const;												// Index
	uint32					Id(uint32 Index) const;											// Index
	FW::RetValue<const CVariant*, EResult> PtrAt(uint32 Index) const;						// List or Index
	FW::RetValue<CVariant*, EResult> PtrAt(uint32 Index, bool bCanAdd);						// List or Index
	const Ref				At(uint32 Index) const { auto Ret = PtrAt(Index); if (Ret.Error) Throw(Ret.Error); return Ret.Error ? NULL : Ret.Value; }
	Ref						At(uint32 Index) { auto Ret = PtrAt(Index, true); if (Ret.Error) Throw(Ret.Error); return Ret.Error ? NULL : Ret.Value; }
	CVariant				Get(uint32 Index) const;										// List or Index
	bool					Has(uint32 Index) const;										// List or Index
	EResult					Remove(uint32 Index);											// List or Index
	EResult					Append(const CVariant& Variant);								// List
	EResult					Insert(uint32 Index, const CVariant& Variant);					// Index

	EResult					Merge(const CVariant& Variant);

	Ref operator[](const char* Name)									{return At(Name);}	// Map
	const Ref operator[](const char* Name) const						{return At(Name);}	// Map
	Ref operator[](uint32 Index)										{return At(Index);} // List or Index
	const Ref operator[](uint32 Index)	const							{return At(Index);} // List or Index

#ifndef NO_CRT
	explicit CVariant(const byte* Payload, size_t Size, EType Type = VAR_TYPE_BYTES) : CVariant((FW::AbstractMemPool*)nullptr, Payload, Size, Type) {}
#endif
	explicit CVariant(FW::AbstractMemPool* pMemPool, const byte* Payload, size_t Size, EType Type = VAR_TYPE_BYTES) : FW::AbstractContainer(pMemPool) {InitValue(Type, Size, Payload);}
	operator byte*() const												{auto pVal = Val(true); if(!pVal.Error && pVal.Value->IsRaw()) return pVal.Value->Payload; return NULL;}
	byte*					GetData() const								{auto pVal = Val(true); return pVal.Error ? NULL : pVal.Value->Payload;}
	uint32					GetSize() const								{auto pVal = Val(true); return pVal.Error ? 0 : pVal.Value->Size;}
	EType					GetType() const;
	bool					IsValid() const								{return GetType() != VAR_TYPE_EMPTY;}

	void					Clear() noexcept;

	CVariant				Clone(bool Full = true) const;
	
	//////////////////////////////////////////////////////////////////////////////////////////////
	// Variant Read/Write

	// Map Read
	//EResult					ReadRawMap(const std::function<void(const SVarName& Name, const CVariant& Data)>& cb) const;
	EResult					ReadRawMap(void(*cb)(const SVarName& Name, const CVariant& Data, void* Param), void* Param) const;
	template <typename F>
	static void ReadRawMapCM(const SVarName& Name, const CVariant& Data, void* Param) {F* func = (F*)(Param); (*func)(Name, Data);}
	template <typename F>
	EResult					ReadRawMap(F&& cb) const {return ReadRawMap(ReadRawMapCM<F>, (void*)(&cb));}

	EResult					Find(const char* Name, CVariant& Data) const;
	CVariant				Find(const char* Name) const {CVariant Data(m_pMem); Find(Name, Data); return Data;}
	// Map Write
	EResult					BeginMap() {return BeginWrite(VAR_TYPE_MAP);}
	EResult					WriteValue(const char* Name, EType Type, size_t Size, const void* Value);
	EResult					WriteVariant(const char* Name, const CVariant& Variant);

	// List Read
	//EResult					ReadRawList(const std::function<void(const CVariant& Data)>& cb) const;
	EResult					ReadRawList(void(*cb)(const CVariant& Data, void* Param), void* Param) const;
	template <typename F>
	static void ReadRawListCB(const CVariant& Data, void* Param) {F* func = (F*)(Param); (*func)(Data);}
	template <typename F>
	EResult					ReadRawList(F&& cb) const {return ReadRawList(ReadRawListCB<F>, (void*)(&cb));}
	// List Write
	EResult					BeginList() {return BeginWrite(VAR_TYPE_LIST);}
	EResult					WriteValue(EType Type, size_t Size, const void* Value);
	EResult					WriteVariant(const CVariant& Variant);

	// IMap Read
	//EResult					ReadRawIMap(const std::function<void(uint32 Index, const CVariant& Data)>& cb) const;
	EResult					ReadRawIMap(void(*cb)(uint32 Index, const CVariant& Data, void* Param), void* Param)  const;
	template <typename F>
	static void ReadRawIMapCB(uint32 Index, const CVariant& Data, void* Param) {F* func = (F*)(Param); (*func)(Index, Data);}
	template <typename F>
	EResult					ReadRawIMap(F&& cb) const {return ReadRawIMap(ReadRawIMapCB<F>, (void*)(&cb));}
	EResult					Find(uint32 Index, CVariant& Data) const;
	CVariant				Find(uint32 Index) const {CVariant Data(m_pMem); Find(Index, Data); return Data;}
	// IMap Write
	EResult					BeginIMap() {return BeginWrite(VAR_TYPE_INDEX);}
	EResult					WriteValue(uint32 Index, EType Type, size_t Size, const void* Value);
	EResult					WriteVariant(uint32 Index, const CVariant& Variant);

	EResult					Finish();

	enum EAccess : unsigned char
	{
		eReadWrite,
		eReadOnly,
		eWriteOnly,
		eDerived
	};
	struct LIBRARY_EXPORT SVariant
	{
		SVariant();
		~SVariant();

		bool				Init(EType type, size_t size = 0, const void* payload = 0, bool bTake = false);
		EResult				Clone(CVariant::SVariant* pClone, bool Full = true) const;

		bool				IsRaw() const {return Type != VAR_TYPE_MAP && Type != VAR_TYPE_LIST && Type != VAR_TYPE_INDEX;}

		uint32				Count() const;													// Map, List or Index

		const char*			Key(uint32 Index) const;										// Map
		FW::RetValue<CVariant*, EResult> Get(const char* Name/*, size_t Len = -1*/) const;	// Map
		bool				Has(const char* Name) const;									// Map
		FW::RetValue<CVariant*, EResult> Insert(const char* Name, const CVariant* pVariant, bool bUnsafe = false); // Map
		EResult				Remove(const char* Name);										// Map

		uint32				Id(uint32 Index) const;											// Index
		FW::RetValue<CVariant*, EResult> Get(uint32 Index) const;							// List or Index
		bool				Has(uint32 Index) const;										// List or Index
		EResult				Append(const CVariant* pVariant);								// List
		FW::RetValue<CVariant*, EResult> Insert(uint32 Index, const CVariant* pVariant, bool bUnsafe = false); // Index
		EResult				Remove(uint32 Index);											// List and Index

		EResult				MkPayload(CBuffer* pPayload) const;

		bool 				AllocPayload();
		void 				FreePayload();
		bool 				AllocContainer();
		void				FreeContainer();

		EType				Type;
		EAccess				Access;
		uint32				Size;
		byte*				Payload;
		byte				Buffer[8];
		typedef _TVarIndex TIndex;
#ifndef VAR_NO_STL
		mutable union UContainer
		{
			void*							Void;
			std::map<std::string, CVariant>*Map;
			std::vector<CVariant>*			List;
			std::map<TIndex, CVariant>*		Index;
			CBuffer*						Buffer;
		}									Container;
		FW::RetValue<std::map<std::string, CVariant>*, EResult>	Map() const;
		FW::RetValue<std::vector<CVariant>*, EResult>			List() const;
		FW::RetValue<std::map<TIndex, CVariant>*, EResult>		IMap() const;

		mutable std::atomic<int> Refs;
#else
		mutable union UContainer
		{
			void*							Void;
			FW::Map<FW::StringA, CVariant>*	Map;
			FW::Array<CVariant>*			List;
			FW::Map<TIndex, CVariant>*		Index;
			CBuffer*						Buffer;
		}									Container;
		FW::RetValue<FW::Map<FW::StringA, CVariant>*, EResult>	Map() const;
		FW::RetValue<FW::Array<CVariant>*, EResult>				List() const;
		FW::RetValue<FW::Map<TIndex, CVariant>*, EResult>		IMap() const;

		mutable volatile LONG Refs = 0;
#endif
		FW::AbstractMemPool* pMem;
	};
protected:

	SVariant*				m_Variant = nullptr;

	SVariant* 				AllocData() const;
	void 					FreeData(SVariant* ptr) const;

	EResult					Assign(const CVariant& Variant) noexcept;

	void					Attach(SVariant* Variant) noexcept;
	bool					InitValue(EType Type, size_t Size, const void* Value, bool bTake = false);
	FW::RetValue<SVariant*, EResult> Val(bool NoAlloc = false) const;
	FW::RetValue<SVariant*, EResult> Val();

	static uint32			ReadHeader(const CBuffer* pPacket, EType* pType = NULL);
	static EResult			ToPacket(CBuffer* pPacket, EType Type, size_t uLength, const void* Value);

	EResult					BeginWrite(EType Type);

	static EResult			Throw(EResult Error);

public:

	//////////////////////////////////////////////////////////////////////////////////////////////
	// From Type Constructors and To Type Operators

	explicit CVariant(const CBuffer& Buffer) : FW::AbstractContainer(Buffer) {InitValue(VAR_TYPE_BYTES, Buffer.GetSize(), Buffer.GetBuffer());}
	explicit CVariant(CBuffer& Buffer, bool bTake = false) : FW::AbstractContainer(Buffer) {size_t uSize = Buffer.GetSize(); InitValue(VAR_TYPE_BYTES, uSize, Buffer.GetBuffer(bTake), bTake);}
	CVariant& operator=(const CBuffer& Buffer) {
		Clear(); 
		if (!m_pMem) m_pMem = Buffer.Allocator();
		InitValue(VAR_TYPE_BYTES, Buffer.GetSize(), Buffer.GetBuffer()); 
		return *this;
	}
	operator CBuffer() const				{auto pVal = Val(true); if(!pVal.Error && pVal.Value->IsRaw()) return CBuffer(m_pMem, pVal.Value->Payload, pVal.Value->Size, true); return CBuffer(m_pMem);}

	// machine types
#ifndef NO_CRT
	explicit CVariant(const bool& b) : CVariant((FW::AbstractMemPool*)nullptr, b) {}
#endif
	explicit CVariant(FW::AbstractMemPool* pMemPool, const bool& b) : FW::AbstractContainer(pMemPool)		{InitValue(VAR_TYPE_SINT, sizeof(bool), &b);}
	//CVariant& operator=(const bool& b)		{Clear(); InitValue(VAR_TYPE_SINT, sizeof(bool), &b); return *this;}
	operator bool() const					{bool b = false; GetInt(sizeof(bool), &b); return b;}
#ifndef NO_CRT
	explicit CVariant(const sint8& sint) : CVariant((FW::AbstractMemPool*)nullptr, sint) {}
#endif
	explicit CVariant(FW::AbstractMemPool* pMemPool, const sint8& sint) : FW::AbstractContainer(pMemPool)	{InitValue(VAR_TYPE_SINT, sizeof(sint8), &sint);}
	CVariant& operator=(const sint8& sint)	{Clear(); InitValue(VAR_TYPE_SINT, sizeof(sint8), &sint); return *this;}
	operator sint8() const					{sint8 sint = 0; GetInt(sizeof(sint8), &sint); return sint;}
#ifndef NO_CRT
	explicit CVariant(const uint8& uint) : CVariant((FW::AbstractMemPool*)nullptr, uint) {}
#endif
	explicit CVariant(FW::AbstractMemPool* pMemPool, const uint8& uint) : FW::AbstractContainer(pMemPool)	{InitValue(VAR_TYPE_UINT, sizeof(uint8), &uint);}
	CVariant& operator=(const uint8& uint)	{Clear(); InitValue(VAR_TYPE_UINT, sizeof(uint8), &uint); return *this;}
	operator uint8() const					{uint8 uint = 0; GetInt(sizeof(uint8), &uint); return uint;}
#ifndef NO_CRT
	explicit CVariant(const sint16& sint) : CVariant((FW::AbstractMemPool*)nullptr, sint) {}
#endif
	explicit CVariant(FW::AbstractMemPool* pMemPool, const sint16& sint) : FW::AbstractContainer(pMemPool)	{InitValue(VAR_TYPE_SINT, sizeof(sint16), &sint);}
	CVariant& operator=(const sint16& sint)	{Clear(); InitValue(VAR_TYPE_SINT, sizeof(sint16), &sint); return *this;}
	operator sint16() const					{sint16 sint = 0; GetInt(sizeof(sint16), &sint); return sint;}
#ifndef NO_CRT
	explicit CVariant(const uint16& uint) : CVariant((FW::AbstractMemPool*)nullptr, uint) {}
#endif
	explicit CVariant(FW::AbstractMemPool* pMemPool, const uint16& uint) : FW::AbstractContainer(pMemPool)	{InitValue(VAR_TYPE_UINT, sizeof(uint16), &uint);}
	CVariant& operator=(const uint16& uint)	{Clear(); InitValue(VAR_TYPE_UINT, sizeof(uint16), &uint); return *this;}
	operator uint16() const					{uint16 uint = 0; GetInt(sizeof(uint16), &uint); return uint;}
#ifndef NO_CRT
	explicit CVariant(const sint32& sint) : CVariant((FW::AbstractMemPool*)nullptr, sint) {}
#endif
	explicit CVariant(FW::AbstractMemPool* pMemPool, const sint32& sint) : FW::AbstractContainer(pMemPool)	{InitValue(VAR_TYPE_SINT, sizeof(sint32), &sint);}
	CVariant& operator=(const sint32& sint)	{Clear(); InitValue(VAR_TYPE_SINT, sizeof(sint32), &sint); return *this;}
	operator sint32() const					{sint32 sint = 0; GetInt(sizeof(sint32), &sint); return sint;}
#ifndef NO_CRT
	explicit CVariant(const uint32& uint) : CVariant((FW::AbstractMemPool*)nullptr, uint) {}
#endif
	explicit CVariant(FW::AbstractMemPool* pMemPool, const uint32& uint) : FW::AbstractContainer(pMemPool)	{InitValue(VAR_TYPE_UINT, sizeof(uint32), &uint);}
	CVariant& operator=(const uint32& uint)	{Clear(); InitValue(VAR_TYPE_UINT, sizeof(uint32), &uint); return *this;}
	operator uint32() const					{uint32 uint = 0; GetInt(sizeof(uint32), &uint); return uint;}
#ifndef WIN32
	// Note: gcc seams to use a 32 bit time_t, so we need a conversion here
#ifndef NO_CRT
	explicit CVariant(const time_t& time) : CVariant((FW::AbstractMemPool*)nullptr, time) {}
#endif
	explicit CVariant(FW::AbstractMemPool* pMemPool, const time_t& time) : FW::AbstractContainer(pMemPool)	{sint64 uint = time; InitValue(VAR_TYPE_UINT, sizeof(sint64), &uint);}
	CVariant& operator=(const time_t& time)	{Clear(); sint64 uint = time; InitValue(VAR_TYPE_UINT, sizeof(sint64), &uint); return *this;}
	operator time_t() const					{sint64 uint = 0; GetInt(sizeof(sint64), &uint); return uint;}
#endif
#ifndef NO_CRT
	explicit CVariant(const sint64& sint) : CVariant((FW::AbstractMemPool*)nullptr, sint) {}
#endif
	explicit CVariant(FW::AbstractMemPool* pMemPool, const sint64& sint) : FW::AbstractContainer(pMemPool)	{InitValue(VAR_TYPE_SINT, sizeof(sint64), &sint);}
	CVariant& operator=(const sint64& sint)	{Clear(); InitValue(VAR_TYPE_SINT, sizeof(sint64), &sint); return *this;}
	operator sint64() const					{sint64 sint = 0; GetInt(sizeof(sint64), &sint); return sint;}
#ifndef NO_CRT
	explicit CVariant(const uint64& uint) : CVariant((FW::AbstractMemPool*)nullptr, uint) {}
#endif
	explicit CVariant(FW::AbstractMemPool* pMemPool, const uint64& uint) : FW::AbstractContainer(pMemPool)	{InitValue(VAR_TYPE_UINT, sizeof(uint64), &uint);}
	CVariant& operator=(const uint64& uint)	{Clear(); InitValue(VAR_TYPE_UINT, sizeof(uint64), &uint); return *this;}
	operator uint64() const					{uint64 uint = 0; GetInt(sizeof(uint64), &uint); return uint;}

#ifndef VAR_NO_FPU
#ifndef NO_CRT
	explicit CVariant(const float& val) : CVariant((FW::AbstractMemPool*)nullptr, val) {}
#endif
	explicit CVariant(FW::AbstractMemPool* pMemPool, const float& val) : FW::AbstractContainer(pMemPool)	{InitValue(VAR_TYPE_DOUBLE, sizeof(float), &val);}
	CVariant& operator=(const float& val)	{Clear(); InitValue(VAR_TYPE_DOUBLE, sizeof(float), &val); return *this;}
	operator float() const					{float val = 0; GetDouble(sizeof(float), &val); return val;}
#ifndef NO_CRT
	explicit CVariant(const double& val) : CVariant((FW::AbstractMemPool*)nullptr, val) {}
#endif
	explicit CVariant(FW::AbstractMemPool* pMemPool, const double& val) : FW::AbstractContainer(pMemPool)	{InitValue(VAR_TYPE_DOUBLE, sizeof(double), &val);}
	CVariant& operator=(const double& val)	{Clear(); InitValue(VAR_TYPE_DOUBLE, sizeof(double), &val); return *this;}
	operator double() const					{double val = 0; GetDouble(sizeof(double), &val); return val;}
#endif

	// strings
#ifndef NO_CRT
	explicit CVariant(const char* str, size_t len = -1) : CVariant((FW::AbstractMemPool*)nullptr, str, len) {}
#endif
	explicit CVariant(FW::AbstractMemPool* pMemPool, const char* str, size_t len = -1) : FW::AbstractContainer(pMemPool) {
		if (len == -1) len = strlen(str);
		InitValue(VAR_TYPE_ASCII, len, str);
	}
	CVariant& operator=(const char* str)	{Clear(); InitValue(VAR_TYPE_ASCII, strlen(str), str); return *this;}
#ifndef NO_CRT
	explicit CVariant(const wchar_t* wstr, size_t len = -1, bool bUtf8 = false) : CVariant((FW::AbstractMemPool*)nullptr, wstr, len, bUtf8) {}
#endif
	explicit CVariant(FW::AbstractMemPool* pMemPool, const wchar_t* wstr, size_t len = -1, bool bUtf8 = false) : FW::AbstractContainer(pMemPool) {
		if (len == -1) len = wcslen(wstr);
#ifdef BUFF_NO_STL_STR
		UNREFERENCED_PARAMETER(bUtf8);
#else
		if (bUtf8) {
			char* pUtf8 = WCharToUtf8(wstr, len, &len);
			InitValue(VAR_TYPE_UTF8, len, pUtf8, true);
		} else
#endif
			InitValue(VAR_TYPE_UNICODE, len*sizeof(wchar_t), wstr);
	}
	CVariant& operator=(const wchar_t* wstr) {Clear(); InitValue(VAR_TYPE_UNICODE, wcslen(wstr)*sizeof(wchar_t), wstr); return *this;}
#ifndef VAR_NO_STL_STR
	explicit CVariant(const std::string& str) : FW::AbstractContainer(nullptr) {InitValue(VAR_TYPE_ASCII, str.length(), str.c_str());}
	CVariant& operator=(const std::string& str) {Clear(); InitValue(VAR_TYPE_ASCII, str.length(), str.c_str()); return *this;}
	operator std::string() const			{return ToString();}
	explicit CVariant(const std::wstring& wstr, bool bUtf8 = false) : CVariant((FW::AbstractMemPool*)nullptr, wstr.c_str(), wstr.length(), bUtf8) {}
	CVariant& operator=(const std::wstring& wstr) {Clear(); InitValue(VAR_TYPE_UNICODE, wstr.length()*sizeof(wchar_t), wstr.c_str()); return *this;}
	operator std::wstring() const			{return ToWString();}
#endif
	explicit CVariant(const FW::StringA& str) : FW::AbstractContainer(nullptr) {InitValue(VAR_TYPE_ASCII, str.Length(), str.ConstData());}
	CVariant& operator=(const FW::StringA& str) {Clear(); InitValue(VAR_TYPE_ASCII, str.Length(), str.ConstData()); return *this;}
	operator FW::StringA() const			{return ToStringA();}
	//explicit CVariant(const FW::StringW& wstr, bool bUtf8 = false) : CVariant((FW::AbstractMemPool*)nullptr, wstr.ConstData(), wstr.Length(), bUtf8) {} // todo
	explicit CVariant(const FW::StringW& wstr) : CVariant((FW::AbstractMemPool*)nullptr, wstr.ConstData(), wstr.Length()) {}
	CVariant& operator=(const FW::StringW& wstr) {Clear(); InitValue(VAR_TYPE_UNICODE, wstr.Length()*sizeof(wchar_t), wstr.ConstData()); return *this;}
	operator FW::StringW() const			{return ToStringW();}

#ifndef VAR_NO_STL_VECT
	explicit CVariant(const std::vector<byte>& vec)	: FW::AbstractContainer(nullptr) {InitValue(VAR_TYPE_BYTES, vec.size(), vec.data());}
	CVariant& operator=(const std::vector<byte>& vec) {Clear(); InitValue(VAR_TYPE_BYTES, vec.size(), vec.data()); return *this;}
	//operator std::vector<byte>() const		{ const SVariant* Variant = Val(); if (Variant->IsRaw()) { std::vector<byte> vec(Variant->Size); MemCopy(vec.data(), Variant->Payload, Variant->Size); return vec; } return std::vector<byte>(); }
#endif


	//////////////////////////////////////////////////////////////////////////////////////////////
	// Conversion

	template <class T> 
	T To(const T& Default) const {if (!IsValid()) return Default; return (T)*this;}
	template <class T> 
	T To() const {return (T)*this;}

#ifndef VAR_NO_STL_STR
	std::string ToString() const;
	std::wstring ToWString() const;
#endif
	FW::StringA ToStringA() const;
	FW::StringW ToStringW() const;

	EResult GetInt(size_t Size, void* Value) const;
#ifndef VAR_NO_FPU
	EResult GetDouble(size_t Size, void* Value) const;
#endif

#ifndef VAR_NO_STL_STR
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
				std::wstring Num = ToWString();
				wchar_t* endPtr;
				if (Num.find(L".") != std::wstring::npos)
					return (T)std::wcstod(Num.c_str(), &endPtr);
				else
					return (T)std::wcstoll(Num.c_str(), &endPtr, 10);
			}
		} catch (...) {}
		if (pOk) *pOk = false;
		return 0;
	}

	std::wstring AsStr(bool* pOk = NULL) const
	{
		if (pOk) *pOk = true;
		try {
			EType Type = GetType();
			if (Type == VAR_TYPE_ASCII || Type == VAR_TYPE_BYTES || Type == VAR_TYPE_UTF8 || Type == VAR_TYPE_UNICODE)
				return ToWString();
			else if (Type == VAR_TYPE_SINT) {
				sint64 sint = 0;
				GetInt(sizeof(sint64), &sint);
				return std::to_wstring(sint);
			}
			else if (Type == VAR_TYPE_UINT) {
				uint64 uint = 0;
				GetInt(sizeof(sint64), &uint);
				return std::to_wstring(uint);
			}
#ifndef VAR_NO_FPU
			else if (Type == VAR_TYPE_DOUBLE) {
				double val = 0;
				GetDouble(sizeof(double), &val);
				return std::to_wstring(val);
			}
#endif
		}
		catch (...) {}
		if (pOk) *pOk = false;
		return L"";
	}
#endif

#ifndef VAR_NO_STL_VECT
	std::vector<byte> AsBytes() const
	{
		auto pVal = Val(true);
		if (!pVal.Error && pVal.Value->IsRaw()) { 
			std::vector<byte> vec(pVal.Value->Size); 
			MemCopy(vec.data(), pVal.Value->Payload, pVal.Value->Size); 
			return vec; 
		} 
		return std::vector<byte>();
	}

	template<typename T>
	explicit CVariant(const std::vector<T>& List) : CVariant() 
	{
		BeginList();
		for(auto I = List.begin(); I != List.end(); ++I)
			Write(*I);
		Finish();
	}
	template<typename T>
	CVariant& operator=(const std::vector<T>& List) {
		Assign(CVariant(List));
		return *this;
	}

	template<typename T>
	std::vector<T>			AsList() const {
		std::vector<T> List;
		ReadRawList([&](const CVariant& Data) {
			List.push_back(Data.To<T>());
		});
		return List;
	}

	std::vector<std::wstring> AsStrList() const {
		std::vector<std::wstring> List;
		ReadRawList([&](const CVariant& Data) {
			List.push_back(Data.AsStr());
		});
		return List;
	}
#endif


	//////////////////////////////////////////////////////////////////////////////////////////////
	// Map Read/Write Specialized Implementation

	//EResult Write(const char* Name, const CBuffer& Buffer)			{return WriteValue(Name, VAR_TYPE_BYTES, Buffer.GetSize(), Buffer.GetBuffer());}

	template<typename T>
	EResult Write(const char* Name, const T& List)
	{
		CVariant vList;
		vList.BeginList();
		for(auto I = List.begin(); I != List.end(); ++I)
			vList.Write(*I);
		vList.Finish();
		return WriteVariant(Name, vList);
	}

	EResult Write(const char* Name, const bool& b)					{return WriteValue(Name, VAR_TYPE_SINT, sizeof(bool), &b);}
	EResult Write(const char* Name, const sint8& sint)				{return WriteValue(Name, VAR_TYPE_SINT, sizeof(sint8), &sint);}
	EResult Write(const char* Name, const uint8& uint)				{return WriteValue(Name, VAR_TYPE_UINT, sizeof(uint8), &uint);}
	EResult Write(const char* Name, const sint16& sint)				{return WriteValue(Name, VAR_TYPE_SINT, sizeof(sint16), &sint);}
	EResult Write(const char* Name, const uint16& uint)				{return WriteValue(Name, VAR_TYPE_UINT, sizeof(uint16), &uint);}
	EResult Write(const char* Name, const sint32& sint)				{return WriteValue(Name, VAR_TYPE_SINT, sizeof(sint32), &sint);}
	EResult Write(const char* Name, const uint32& uint)				{return WriteValue(Name, VAR_TYPE_UINT, sizeof(uint32), &uint);}
#ifndef WIN32
	// Note: gcc seams to use a 32 bit time_t, so we need a conversion here
	EResult Write(const char* Name, const time_t& time)				{sint64 uint = time; return WriteValue(Name, VAR_TYPE_UINT, sizeof(sint64), &uint);}
#endif
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
#ifdef BUFF_NO_STL_STR
		UNREFERENCED_PARAMETER(bUtf8);
#else
		if (bUtf8) {
			char* pUtf8 = WCharToUtf8(wstr, len, &len);
			Res = WriteValue(Name, VAR_TYPE_UTF8, len, pUtf8);
			free(pUtf8);
		} else
#endif
			Res = WriteValue(Name, VAR_TYPE_UNICODE, len*sizeof(wchar_t), wstr);
		return Res;
	}
#ifndef VAR_NO_STL_STR
	EResult Write(const char* Name, const std::string& str)			{return WriteValue(Name, VAR_TYPE_ASCII, str.length(), str.c_str());}
	EResult Write(const char* Name, const std::wstring& wstr, bool bUtf8 = false) {return Write(Name, wstr.c_str(), wstr.length(), bUtf8);}
#endif
	EResult Write(const char* Name, const FW::StringA& str)			{return WriteValue(Name, VAR_TYPE_ASCII, str.Length(), str.ConstData());}
	EResult Write(const char* Name, const FW::StringW& wstr)		{return Write(Name, wstr.ConstData(), wstr.Length());}

#ifndef VAR_NO_STL_VECT
	EResult Write(const char* Name, const std::vector<byte>& vec)	{return WriteValue(Name, VAR_TYPE_BYTES, vec.size(), vec.data());}
#endif


	//////////////////////////////////////////////////////////////////////////////////////////////
	// List Read/Write Specialized Implementation

	//EResult Write(const CBuffer& Buffer)		{return WriteValue(VAR_TYPE_BYTES, Buffer.GetSize(), Buffer.GetBuffer());}

	template<typename T>
	EResult Write(const T& List)
	{
		CVariant vList;
		vList.BeginList();
		for(auto I = List.begin(); I != List.end(); ++I)
			vList.Write(*I);
		vList.Finish();
		return WriteVariant(vList);
	}

	EResult Write(const bool& b)				{return WriteValue(VAR_TYPE_SINT, sizeof(bool), &b);}
	EResult Write(const sint8& sint)			{return WriteValue(VAR_TYPE_SINT, sizeof(sint8), &sint);}
	EResult Write(const uint8& uint)			{return WriteValue(VAR_TYPE_UINT, sizeof(uint8), &uint);}
	EResult Write(const sint16& sint)			{return WriteValue(VAR_TYPE_SINT, sizeof(sint16), &sint);}
	EResult Write(const uint16& uint)			{return WriteValue(VAR_TYPE_UINT, sizeof(uint16), &uint);}
	EResult Write(const sint32& sint)			{return WriteValue(VAR_TYPE_SINT, sizeof(sint32), &sint);}
	EResult Write(const uint32& uint)			{return WriteValue(VAR_TYPE_UINT, sizeof(uint32), &uint);}
#ifndef WIN32
	// Note: gcc seams to use a 32 bit time_t, so we need a conversion here
	EResult Write(const time_t& time)			{sint64 uint = time; return WriteValue(VAR_TYPE_UINT, sizeof(sint64), &uint);}
#endif
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
#ifdef BUFF_NO_STL_STR
		UNREFERENCED_PARAMETER(bUtf8);
#else
		if (bUtf8) {
			char* pUtf8 = WCharToUtf8(wstr, len, &len);
			Res = WriteValue(VAR_TYPE_UTF8, len, pUtf8);
			free(pUtf8);
		} else
#endif
			Res = WriteValue(VAR_TYPE_UNICODE, len*sizeof(wchar_t), wstr);
		return Res;
	}
#ifndef VAR_NO_STL_STR
	EResult Write(const std::string& str)		{return WriteValue(VAR_TYPE_ASCII, str.length(), str.c_str());}
	EResult Write(const std::wstring& wstr, bool bUtf8 = false) {return Write(wstr.c_str(), wstr.length(), bUtf8);}
#endif
	EResult Write(const FW::StringA& str)		{return WriteValue(VAR_TYPE_ASCII, str.Length(), str.ConstData());}
	EResult Write(const FW::StringW& wstr)		{return Write(wstr.ConstData(), wstr.Length());}

#ifndef VAR_NO_STL_VECT
	EResult Write(const std::vector<byte>& vec) {return WriteValue(VAR_TYPE_BYTES, vec.size(), vec.data());}
#endif


	//////////////////////////////////////////////////////////////////////////////////////////////
	// IMap Read/Write Specialized Implementation

	//EResult Write(uint32 Index, const CBuffer& Buffer)		{return WriteValue(Index, VAR_TYPE_BYTES, Buffer.GetSize(), Buffer.GetBuffer());}

	template<typename T>
	EResult Write(uint32 Index, const T& List)
	{
		CVariant vList;
		vList.BeginList();
		for(auto I = List.begin(); I != List.end(); ++I)
			vList.Write(*I);
		vList.Finish();
		return WriteVariant(Index, vList);
	}

	EResult Write(uint32 Index, const bool& b)				{return WriteValue(Index, VAR_TYPE_SINT, sizeof(bool), &b);}
	EResult Write(uint32 Index, const sint8& sint)			{return WriteValue(Index, VAR_TYPE_SINT, sizeof(sint8), &sint);}
	EResult Write(uint32 Index, const uint8& uint)			{return WriteValue(Index, VAR_TYPE_UINT, sizeof(uint8), &uint);}
	EResult Write(uint32 Index, const sint16& sint)			{return WriteValue(Index, VAR_TYPE_SINT, sizeof(sint16), &sint);}
	EResult Write(uint32 Index, const uint16& uint)			{return WriteValue(Index, VAR_TYPE_UINT, sizeof(uint16), &uint);}
	EResult Write(uint32 Index, const sint32& sint)			{return WriteValue(Index, VAR_TYPE_SINT, sizeof(sint32), &sint);}
	EResult Write(uint32 Index, const uint32& uint)			{return WriteValue(Index, VAR_TYPE_UINT, sizeof(uint32), &uint);}
#ifndef WIN32
	// Note: gcc seams to use a 32 bit time_t, so we need a conversion here
	EResult Write(uint32 Index, const time_t& time)			{sint64 uint = time; return WriteValuereturn WriteValue(Index, VAR_TYPE_UINT, sizeof(sint64), &uint);}
#endif
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
#ifdef BUFF_NO_STL_STR
		UNREFERENCED_PARAMETER(bUtf8);
#else
		if (bUtf8) {
			char* pUtf8 = WCharToUtf8(wstr, len, &len);
			Res = WriteValue(Index, VAR_TYPE_UTF8, len, pUtf8);
			free(pUtf8);
		} else
#endif
			Res = WriteValue(Index, VAR_TYPE_UNICODE, len*sizeof(wchar_t), wstr);
		return Res;
	}
#ifndef VAR_NO_STL_STR
	EResult Write(uint32 Index, const std::string& str)		{return WriteValue(Index, VAR_TYPE_ASCII, str.length(), str.c_str());}
	EResult Write(uint32 Index, const std::wstring& wstr, bool bUtf8 = false) {return Write(Index, wstr.c_str(), wstr.length(), bUtf8);}
#endif
	EResult Write(uint32 Index, const FW::StringA& str)		{return WriteValue(Index, VAR_TYPE_ASCII, str.Length(), str.ConstData());}
	EResult Write(uint32 Index, const FW::StringW& wstr)	{return Write(Index, wstr.ConstData(), wstr.Length());}

#ifndef VAR_NO_STL_VECT
	EResult Write(uint32 Index, const std::vector<byte>& vec) {return WriteValue(Index, VAR_TYPE_BYTES, vec.size(), vec.data());}
#endif

};

__inline bool operator<(const CVariant &L, const CVariant &R) {return L.Compare(R) > 0;}

//////////////////////////////////////////////////////////////////////////////////////////////
// 

#ifndef VAR_NO_STL_STR
LIBRARY_EXPORT void WritePacket(const std::string& Name, const CVariant& Packet, CBuffer& Buffer);
LIBRARY_EXPORT void ReadPacket(const CBuffer& Buffer, std::string& Name, CVariant& Packet);
//LIBRARY_EXPORT bool StreamPacket(CBuffer& Buffer, std::string& Name, CVariant& Packet);
#endif 

LIBRARY_EXPORT void TestVariant();