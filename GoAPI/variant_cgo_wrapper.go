//go:build cgo
// +build cgo

package goapi

/*
#cgo CFLAGS: -I${SRCDIR}/Core -I${SRCDIR}/Common -Wno-everything
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// Include Types.h first to avoid Windows path issues
#include "Core/Types.h"
#include "Common/VariantDefs.h"

// Define the VARIANT structures inline to avoid header path issues
typedef struct _VARIANT
{
	uint8  uType;
	uint8  bError;
	uint32 uSize;
	uint32 uMaxSize;
	union {
		uchar* pData;
		uint64 uPayload;
	};
} VARIANT, * PVARIANT;

typedef struct _VARIANT_IT
{
	uchar* ptr;
	uchar* end;
} VARIANT_IT, * PVARIANT_IT;

#define VAR_ERR_BAD_SIZE		1
#define VAR_ERR_BAD_TYPE		2

// Inline the C implementation (without header includes to avoid Windows path issues)
#pragma warning(disable : 4100)

static uint32 Variant_ReadSize(uchar** ptr, size_t size)
{
	uint32 len = (uint32)-1;
	uint8 type = (*ptr)[0];
	*ptr += 1;
	if (type & VAR_LEN_FIELD)
	{
		if ((type & VAR_LEN_MASK) == VAR_LEN8 && size >= 1 + 1) {
			len = *(uint8*)*ptr;
			*ptr += 1;
		}
		else if ((type & VAR_LEN_MASK) == VAR_LEN16 && size >= 1 + 2) {
			len = *(uint16*)*ptr;
			*ptr += 2;
		}
		else if ((type & VAR_LEN_MASK) == VAR_LEN32 && size >= 1 + 4) {
			len = *(uint32*)*ptr;
			*ptr += 4;
		}
	}
	else if ((type & VAR_TYPE_MASK) == VAR_TYPE_EMPTY)
		len = 0;
	else if ((type & VAR_LEN_MASK) == VAR_LEN8)
		len = 1;
	else if ((type & VAR_LEN_MASK) == VAR_LEN16)
		len = 2;
	else if ((type & VAR_LEN_MASK) == VAR_LEN32)
		len = 4;
	else if ((type & VAR_LEN_MASK) == VAR_LEN64)
		len = 8;
	return len;
}

static uint32 Variant_WriteSize(uchar** ptr, uint8 type, size_t size)
{
	uint32 hdr_size = 1;
	(*ptr)[0] = type & VAR_TYPE_MASK;
	if ((type & VAR_TYPE_MASK) == VAR_TYPE_EMPTY)
		;
	else if (size == 1)
		(*ptr)[0] |= VAR_LEN8;
	else if (size == 2)
		(*ptr)[0] |= VAR_LEN16;
	else if (size == 4)
		(*ptr)[0] |= VAR_LEN32;
	else if (size == 8)
		(*ptr)[0] |= VAR_LEN64;
	else
	{
		if (size >= USHRT_MAX) {
			(*ptr)[0] |= VAR_LEN_FIELD | VAR_LEN32;
			*(uint32*)&(*ptr)[1] = (uint32)size;
			hdr_size += 4;
		}
		else if (size >= UCHAR_MAX) {
			(*ptr)[0] |= VAR_LEN_FIELD | VAR_LEN16;
			*(uint16*)&(*ptr)[1] = (uint16)size;
			hdr_size += 2;
		}
		else {
			(*ptr)[0] |= VAR_LEN_FIELD | VAR_LEN8;
			(*ptr)[1] = (uint8)size;
			hdr_size += 1;
		}
	}
	*ptr += hdr_size;
	return hdr_size;
}

static size_t Variant_FromBuffer(const byte* pBuffer, size_t uSize, PVARIANT out_var)
{
	if (uSize < 1 || uSize > ULONG_MAX)
		return 0;
	out_var->uType = pBuffer[0] & VAR_TYPE_MASK;
	out_var->pData = (uchar*)pBuffer;
	uint32 length = Variant_ReadSize(&out_var->pData, (uint32)uSize);
	if (length == -1 || length > (pBuffer + uSize) - out_var->pData)
		return 0;
	out_var->uSize = out_var->uMaxSize = length;
	return out_var->pData - pBuffer + length;
}

static size_t Variant_FromPacket(const byte* pBuffer, size_t uSize, char* out_name, size_t max_name, PVARIANT out_var)
{
	if (uSize < 1)
		return 0;
	const byte* ptr = pBuffer;
	uint8 name_len = *ptr++;
	if (uSize < 1 + name_len || name_len > max_name - 1)
		return 0;
	memcpy(out_name, ptr, name_len);
	out_name[name_len] = 0;
	ptr += name_len;

	return Variant_FromBuffer(ptr, uSize - (1 + name_len), out_var);
}

static int Variant_Find(const PVARIANT var, const char* name, PVARIANT out_var)
{
	size_t name_len = strlen(name);

	if (var->uSize == 0 || var->uType != VAR_TYPE_MAP)
		return 0;

	uchar* ptr = var->pData;
	uchar* end = ptr + var->uSize;
	while (ptr < end)
	{
		uint8 cur_len = *ptr++;
		if (ptr + cur_len >= end)
			break;
		char* cur_name = (char*)ptr;
		ptr += cur_len;
		if (name_len == cur_len && memcmp(cur_name, name, name_len) == 0)
			return Variant_FromBuffer(ptr, end - ptr, out_var) != 0;
		ptr += Variant_ReadSize(&ptr, end - ptr);
	}

	return 0;
}

static int Variant_Get(const PVARIANT var, uint32 index, PVARIANT out_var)
{
	if (var->uSize == 0 || var->uType != VAR_TYPE_INDEX)
		return 0;

	uchar* ptr = var->pData;
	uchar* end = ptr + var->uSize;
	while (ptr < end)
	{
		if (ptr + 4 >= end)
			break;
		uint32 cur_index = *(uint32*)ptr;
		ptr += 4;
		if (cur_index == index)
			return Variant_FromBuffer(ptr, end - ptr, out_var) != 0;
		ptr += Variant_ReadSize(&ptr, end - ptr);
	}

	return 0;
}

static int Variant_At(const PVARIANT var, int pos, PVARIANT out_var)
{
	if (var->uSize == 0 || var->uType != VAR_TYPE_LIST)
		return 0;

	uchar* ptr = var->pData;
	uchar* end = ptr + var->uSize;
	while (ptr < end)
	{
		if (pos-- == 0)
			return Variant_FromBuffer(ptr, end - ptr, out_var) != 0;
		ptr += Variant_ReadSize(&ptr, end - ptr);
	}

	return 0;
}

static int Variant_Begin(const PVARIANT var, PVARIANT_IT it)
{
	if (var->uSize == 0 || var->uType != VAR_TYPE_LIST)
		return 0;

	it->ptr = var->pData;
	it->end = it->ptr + var->uSize;
	return 1;
}

static int Variant_Next(PVARIANT_IT it, PVARIANT out_var)
{
	size_t read = Variant_FromBuffer(it->ptr, it->end - it->ptr, out_var);
	if (!read)
		return 0;
	it->ptr += read;
	return 1;
}

static int Variant_ToInt(const PVARIANT var, void* out, size_t size)
{
	if (var->uSize == 0 || (var->uType != VAR_TYPE_SINT && var->uType != VAR_TYPE_UINT))
		return 0;
	if (var->uSize > size)
	{
		for (size_t i = size; i < var->uSize; i++) {
			if (var->pData[i] != 0)
				return 0;
		}
	}

	memcpy(out, var->pData, var->uSize);
	if (var->uSize < size) {
		// Sign extend for signed integers
		byte fill = 0;
		if (var->uType == VAR_TYPE_SINT && var->uSize > 0 && (var->pData[var->uSize - 1] & 0x80))
			fill = 0xFF;
		memset((byte*)out + var->uSize, fill, size - var->uSize);
	}
	return 1;
}

static size_t Variant_RawBytesOfType(const PVARIANT var, byte** out, uint8 type1, uint8 type2)
{
	if (var->uSize == 0 || var->uType == VAR_TYPE_EMPTY || (var->uType != type1 && var->uType != type2))
		return 0;

	*out = var->pData;
	return var->uSize;
}

static size_t Variant_RawBytes(const PVARIANT var, byte** out)
{
	return Variant_RawBytesOfType(var, out, VAR_TYPE_BYTES, VAR_TYPE_ASCII);
}

static size_t Variant_RawWStr(const PVARIANT var, wchar_t** out)
{
	return Variant_RawBytesOfType(var, (byte**)out, VAR_TYPE_UNICODE, 0) / sizeof(wchar_t);
}

static size_t Variant_ToBytes(const PVARIANT var, byte* out, size_t max_size)
{
	byte* ptr;
	size_t size = Variant_RawBytes(var, &ptr);
	if (size > max_size)
		return 0;

	memcpy(out, ptr, size);
	return size;
}

static size_t Variant_ToAStr(const PVARIANT var, char* out, size_t max_count)
{
	size_t size = Variant_ToBytes(var, (byte*)out, max_count - 1);
	out[size] = 0;
	return size;
}

static size_t Variant_ToWStr(const PVARIANT var, wchar_t* out, size_t max_count)
{
	wchar_t* ptr;
	size_t size = Variant_RawWStr(var, &ptr);
	if (size > (max_count - 1))
		return 0;

	memcpy(out, ptr, size * sizeof(wchar_t));
	out[size] = 0;
	return size;
}

static void Variant_Init(uint8 uType, void* pBuffer, size_t uMaxSize, PVARIANT out_var)
{
	out_var->uMaxSize = (uint32)uMaxSize;
	out_var->pData = pBuffer;
	out_var->uSize = 0;
	out_var->uType = uType;
	out_var->bError = 0;
}

static void Variant_Prepare(uint8 uType, void* pBuffer, size_t uMaxSize, PVARIANT out_var)
{
	// based on the buffer size pick the larget len field
	uMaxSize -= 1 + 4;
	uint32 hdr_size;
	if (uMaxSize >= USHRT_MAX)
		hdr_size = 1 + 4;
	else if (uMaxSize >= UCHAR_MAX)
		hdr_size = 1 + 2;
	else
		hdr_size = 1 + 1;
	Variant_Init(uType, pBuffer ? &((byte*)pBuffer)[hdr_size] : NULL, pBuffer ? uMaxSize : 0, out_var);
}

static size_t Variant_Finish(void* pBuffer, PVARIANT out_var)
{
	if(out_var->bError)
		return 0;

	byte* out_buff = (byte*)pBuffer;
    if (out_buff && out_var->uType != VAR_TYPE_EMPTY) {
		size_t hdr_size;
		if (out_var->uType & VAR_STATIC) {
			hdr_size = 1 + 1;
			out_buff[0] = (out_var->uType & VAR_TYPE_MASK) | VAR_LEN_FIELD | VAR_LEN8;
			out_buff[1] = (uint8)out_var->uSize;
			memcpy(&out_buff[hdr_size], &out_var->uPayload, out_var->uSize);
		}
		else {
			out_buff[0] = (out_var->uType & VAR_TYPE_MASK);
			if (out_var->uMaxSize >= USHRT_MAX) {
				out_buff[0] |= VAR_LEN_FIELD | VAR_LEN32;
				*(uint32*)&out_buff[1] = (uint32)out_var->uSize;
				hdr_size = 1 + 4;
			}
			else if (out_var->uMaxSize >= UCHAR_MAX) {
				out_buff[0] |= VAR_LEN_FIELD | VAR_LEN16;
				*(uint16*)&out_buff[1] = (uint16)out_var->uSize;
				hdr_size = 1 + 2;
			}
			else {
				out_buff[0] |= VAR_LEN_FIELD | VAR_LEN8;
				out_buff[1] = (uint8)out_var->uSize;
				hdr_size = 1 + 1;
			}
		}
        return hdr_size + out_var->uSize;
    }

	return 0;
}

static void Variant_PrepareEntry(PVARIANT var, uint8 type, PVARIANT out_var)
{
	uchar* ptr = var->pData + var->uSize;
	size_t len = var->uMaxSize - var->uSize;
	Variant_Prepare(type, ptr, len, out_var);
}

static void Variant_FinishEntry(PVARIANT var, PVARIANT out_var)
{
	uchar* ptr = var->pData + var->uSize;
	size_t len = Variant_Finish(ptr, out_var);
	var->uSize += (uint32)len;
}

static void* Variant_SInit(uint8 type, const void* in, size_t size, PVARIANT out_var)
{
	if (size > sizeof(out_var->uPayload))
		return NULL;
	out_var->uType = type | VAR_STATIC;
	out_var->uSize = (uint32)size;
	out_var->uMaxSize = 0;
	memcpy(&out_var->uPayload, in, out_var->uSize);
	return &out_var->uPayload;
}

static void* Variant_Set(uint8 type, const void* in, size_t size, PVARIANT out_var)
{
	if (out_var->uMaxSize < size)
		return NULL;
	out_var->uType = type;
	out_var->uSize = (uint32)size;
	memcpy(out_var->pData, in, size);
	return &out_var->uPayload;
}

static size_t Variant_WriteRaw(uchar* ptr, uint8 type, const void* in, size_t size)
{
	uint32 hdr_size = Variant_WriteSize(&ptr, type, size);
	if(in != (const void*)-1)
		memcpy(ptr, in, size);
	return hdr_size + size;
}

static void* Variant_InsertRaw(PVARIANT var, const char* name, uint8 type, const void* in, size_t size)
{
	size_t name_len = strlen(name);

	if (var->uType == VAR_TYPE_EMPTY)
		var->uType = VAR_TYPE_MAP;
	else if (var->uType != VAR_TYPE_MAP) {
		var->bError |= VAR_ERR_BAD_TYPE;
		return NULL;
	}
	if (var->uMaxSize - var->uSize < 1 + name_len + 1 + size) {
		var->bError |= VAR_ERR_BAD_SIZE;
		return NULL;
	}

	uchar* ptr = var->pData + var->uSize;
	*ptr++ = (uint8)name_len;
	memcpy(ptr, name, name_len);
	ptr += name_len;
	var->uSize += 1 + (uint32)name_len;

	if (in) {
		size_t len = Variant_WriteRaw(ptr, type, in, size);;
		var->uSize += (uint32)len;
		ptr += len - size;
	}
	return ptr;
}

static int Variant_PrepareInsert(PVARIANT var, const char* name, uint8 type, PVARIANT out_var)
{
	if (!Variant_InsertRaw(var, name, 0, NULL, 0))
		return 0;

	Variant_PrepareEntry(var, type, out_var);
	return 1;
}

static int Variant_Insert(PVARIANT var, const char* name, const PVARIANT in_var)
{
	if (in_var->uType & VAR_STATIC)
		return !!Variant_InsertRaw(var, name, in_var->uType & VAR_TYPE_MASK, &in_var->uPayload, in_var->uSize);
	return !!Variant_InsertRaw(var, name, in_var->uType & VAR_TYPE_MASK, in_var->pData, in_var->uSize);
}

static void* Variant_AddRaw(PVARIANT var, uint32 index, uint8 type, const void* in, size_t size)
{
	if (var->uType == VAR_TYPE_EMPTY)
		var->uType = VAR_TYPE_INDEX;
	else if (var->uType != VAR_TYPE_INDEX) {
		var->bError |= VAR_ERR_BAD_TYPE;
		return NULL;
	}
	if (var->uMaxSize - var->uSize < 4 + size) {
		var->bError |= VAR_ERR_BAD_SIZE;
		return NULL;
	}

	uchar* ptr = var->pData + var->uSize;
	*(uint32*)ptr = index;
	ptr += 4;
	var->uSize += 4;

	if (in) {
		size_t len = Variant_WriteRaw(ptr, type, in, size);
		var->uSize += (uint32)len;
		ptr += len - size;
	}
	return ptr;
}

static int Variant_PrepareAdd(PVARIANT var, uint32 index, uint8 type, PVARIANT out_var)
{
	if (!Variant_AddRaw(var, index, 0, NULL, 0))
		return 0;

	Variant_PrepareEntry(var, type, out_var);
	return 1;
}

static int Variant_Add(PVARIANT var, uint32 index, const PVARIANT in_var)
{
	if (in_var->uType & VAR_STATIC)
		return !!Variant_AddRaw(var, index, in_var->uType & VAR_TYPE_MASK, &in_var->uPayload, in_var->uSize);
	return !!Variant_AddRaw(var, index, in_var->uType & VAR_TYPE_MASK, in_var->pData, in_var->uSize);
}

static void* Variant_AppendRaw(PVARIANT var, uint8 type, const void* in, size_t size)
{
	if (var->uType == VAR_TYPE_EMPTY)
		var->uType = VAR_TYPE_LIST;
	else if (var->uType != VAR_TYPE_LIST) {
		var->bError |= VAR_ERR_BAD_TYPE;
		return NULL;
	}
	if (var->uMaxSize - var->uSize < +size) {
		var->bError |= VAR_ERR_BAD_SIZE;
		return NULL;
	}

	uchar* ptr = var->pData + var->uSize;

	if (in) {
		size_t len = Variant_WriteRaw(ptr, type, in, size);
		var->uSize += (uint32)len;
		ptr += len - size;
	}
	return ptr;
}

static int Variant_PrepareAppend(PVARIANT var, uint8 type, PVARIANT out_var)
{
	if (!Variant_AppendRaw(var, 0, NULL, 0))
		return 0;

	Variant_PrepareEntry(var, type, out_var);
	return 1;
}

static int Variant_Append(PVARIANT var, const PVARIANT in_var)
{
	if (in_var->uType & VAR_STATIC)
		return !!Variant_AppendRaw(var, in_var->uType & VAR_TYPE_MASK, &in_var->uPayload, in_var->uSize);
	return !!Variant_AppendRaw(var, in_var->uType & VAR_TYPE_MASK, in_var->pData, in_var->uSize);
}

static size_t Variant_ToBuffer(const PVARIANT var, void* pBuffer, size_t uMaxSize)
{
	if (uMaxSize < 1 + 4 + var->uSize)
		return 0;

	if (var->uType & VAR_STATIC)
		return Variant_WriteRaw(pBuffer, var->uType & VAR_TYPE_MASK, &var->uPayload, var->uSize);
	return Variant_WriteRaw(pBuffer, var->uType & VAR_TYPE_MASK, var->pData, var->uSize);
}

static size_t Variant_ToPacket(char* name, const PVARIANT var, void* pBuffer, size_t uMaxSize)
{
	size_t name_len = strlen(name);

	if (uMaxSize < 1 + name_len || name_len > 0x7F)
		return 0;

	uchar* ptr = pBuffer;
	*ptr++ = (uint8)name_len;
	memcpy(ptr, name, name_len);
	ptr += name_len;

	size_t variant_size = Variant_ToBuffer(var, ptr, uMaxSize - (1 + name_len));
	if (variant_size == 0)
		return 0;

	return 1 + name_len + variant_size;
}

// Inline helper functions for creating variants

static inline uint8*  Variant_FromUInt8(uint8 in, PVARIANT out_var)		{ return (uint8*) Variant_SInit(VAR_TYPE_UINT, &in, sizeof(in),out_var); }
static inline uint16* Variant_FromUInt16(uint16 in, PVARIANT out_var)		{ return (uint16*)Variant_SInit(VAR_TYPE_UINT, &in, sizeof(in),out_var); }
static inline uint32* Variant_FromUInt32(uint32 in, PVARIANT out_var)		{ return (uint32*)Variant_SInit(VAR_TYPE_UINT, &in, sizeof(in),out_var); }
static inline uint64* Variant_FromUInt64(uint64 in, PVARIANT out_var)		{ return (uint64*)Variant_SInit(VAR_TYPE_UINT, &in, sizeof(in),out_var); }
static inline sint8*  Variant_FromSInt8(sint8 in, PVARIANT out_var)		{ return (sint8*) Variant_SInit(VAR_TYPE_SINT, &in, sizeof(in),out_var); }
static inline sint16* Variant_FromSInt16(sint16 in, PVARIANT out_var)		{ return (sint16*)Variant_SInit(VAR_TYPE_SINT, &in, sizeof(in),out_var); }
static inline sint32* Variant_FromSInt32(sint32 in, PVARIANT out_var)		{ return (sint32*)Variant_SInit(VAR_TYPE_SINT, &in, sizeof(in),out_var); }
static inline sint64* Variant_FromSInt64(sint64 in, PVARIANT out_var)		{ return (sint64*)Variant_SInit(VAR_TYPE_SINT, &in, sizeof(in),out_var); }

static inline byte* Variant_FromBytes(const byte* in, size_t size, PVARIANT out_var)					{ return (byte*)Variant_Set(VAR_TYPE_BYTES, in, size, out_var); }
static inline char* Variant_FromAStr(const char* in, size_t count, PVARIANT out_var)					{ return (char*)Variant_Set(VAR_TYPE_ASCII, in, count, out_var); }

static inline uint8  Variant_ToUInt8(const PVARIANT var, uint8 def_val)		{ uint8  val; if (Variant_ToInt(var, &val, sizeof(val))) return val; return def_val; }
static inline uint16 Variant_ToUInt16(const PVARIANT var, uint16 def_val)		{ uint16 val; if (Variant_ToInt(var, &val, sizeof(val))) return val; return def_val; }
static inline uint32 Variant_ToUInt32(const PVARIANT var, uint32 def_val)		{ uint32 val; if (Variant_ToInt(var, &val, sizeof(val))) return val; return def_val; }
static inline uint64 Variant_ToUInt64(const PVARIANT var, uint64 def_val)		{ uint64 val; if (Variant_ToInt(var, &val, sizeof(val))) return val; return def_val; }
static inline sint8  Variant_ToSInt8(const PVARIANT var, sint8 def_val)		{ sint8  val; if (Variant_ToInt(var, &val, sizeof(val))) return val; return def_val; }
static inline sint16 Variant_ToSInt16(const PVARIANT var, sint16 def_val)		{ sint16 val; if (Variant_ToInt(var, &val, sizeof(val))) return val; return def_val; }
static inline sint32 Variant_ToSInt32(const PVARIANT var, sint32 def_val)		{ sint32 val; if (Variant_ToInt(var, &val, sizeof(val))) return val; return def_val; }
static inline sint64 Variant_ToSInt64(const PVARIANT var, sint64 def_val)		{ sint64 val; if (Variant_ToInt(var, &val, sizeof(val))) return val; return def_val; }

static inline uint8*  Variant_InsertUInt8(PVARIANT var, const char* name, uint8 in)		{ return (uint8*) Variant_InsertRaw(var, name, VAR_TYPE_UINT, &in, sizeof(in)); }
static inline uint16* Variant_InsertUInt16(PVARIANT var, const char* name, uint16 in)		{ return (uint16*)Variant_InsertRaw(var, name, VAR_TYPE_UINT, &in, sizeof(in)); }
static inline uint32* Variant_InsertUInt32(PVARIANT var, const char* name, uint32 in)		{ return (uint32*)Variant_InsertRaw(var, name, VAR_TYPE_UINT, &in, sizeof(in)); }
static inline char* Variant_InsertAStr(PVARIANT var, const char* name, const char* in, size_t count)	{ return (char*)Variant_InsertRaw(var, name, VAR_TYPE_ASCII, in, count); }

static inline uint8*   Variant_AppendUInt8(PVARIANT var, uint8 in)			{ return (uint8*) Variant_AppendRaw(var, VAR_TYPE_UINT, &in, sizeof(in)); }

static inline uint32 Variant_FindUInt32(const PVARIANT var, const char* name, uint32 def_val)		{ VARIANT out_var; if (!Variant_Find(var, name, &out_var)) return def_val; return Variant_ToUInt32(&out_var, def_val); }
static inline uint16 Variant_FindUInt16(const PVARIANT var, const char* name, uint16 def_val)		{ VARIANT out_var; if (!Variant_Find(var, name, &out_var)) return def_val; return Variant_ToUInt16(&out_var, def_val); }
static inline size_t Variant_FindAStr(const PVARIANT var, const char* name, char* out, size_t max_size) { VARIANT out_var; if (!Variant_Find(var, name, &out_var)) return 0; return Variant_ToAStr(&out_var, out, max_size); }

static inline uint8  Variant_UInt8At(const PVARIANT var, int pos, uint8 def_val)				{ VARIANT out_var; if (!Variant_At(var, pos, &out_var)) return def_val; return Variant_ToUInt8(&out_var, def_val); }

*/
import "C"
import (
	"unsafe"
)

// CVariantWrapper wraps the C VARIANT struct for testing
type CVariantWrapper struct {
	variant    C.VARIANT
	buffer     []byte
	dataBuffer []byte // Separate buffer for raw data storage
}

// NewCVariantWrapper creates a new C variant wrapper
func NewCVariantWrapper(bufferSize int) *CVariantWrapper {
	return &CVariantWrapper{
		buffer:     make([]byte, bufferSize),
		dataBuffer: make([]byte, bufferSize),
	}
}

// FromUInt8 creates a C variant from uint8
func (w *CVariantWrapper) FromUInt8(value uint8) {
	C.Variant_FromUInt8(C.uint8(value), &w.variant)
}

// FromUInt16 creates a C variant from uint16
func (w *CVariantWrapper) FromUInt16(value uint16) {
	C.Variant_FromUInt16(C.uint16(value), &w.variant)
}

// FromUInt32 creates a C variant from uint32
func (w *CVariantWrapper) FromUInt32(value uint32) {
	C.Variant_FromUInt32(C.uint32(value), &w.variant)
}

// FromUInt64 creates a C variant from uint64
func (w *CVariantWrapper) FromUInt64(value uint64) {
	C.Variant_FromUInt64(C.uint64(value), &w.variant)
}

// FromSInt8 creates a C variant from int8
func (w *CVariantWrapper) FromSInt8(value int8) {
	C.Variant_FromSInt8(C.sint8(value), &w.variant)
}

// FromSInt16 creates a C variant from int16
func (w *CVariantWrapper) FromSInt16(value int16) {
	C.Variant_FromSInt16(C.sint16(value), &w.variant)
}

// FromSInt32 creates a C variant from int32
func (w *CVariantWrapper) FromSInt32(value int32) {
	C.Variant_FromSInt32(C.sint32(value), &w.variant)
}

// FromSInt64 creates a C variant from int64
func (w *CVariantWrapper) FromSInt64(value int64) {
	C.Variant_FromSInt64(C.sint64(value), &w.variant)
}

// FromString creates a C variant from a string
func (w *CVariantWrapper) FromString(value string) {
	// Allocate data buffer if needed
	if len(w.dataBuffer) < len(value)+10 {
		w.dataBuffer = make([]byte, len(value)+10)
	}
	// Allocate serialization buffer if needed
	if len(w.buffer) < len(value)+10 {
		w.buffer = make([]byte, len(value)+10)
	}
	// Initialize the variant with the data buffer
	C.Variant_Init(C.VAR_TYPE_ASCII, unsafe.Pointer(&w.dataBuffer[0]), C.size_t(len(w.dataBuffer)), &w.variant)
	// Copy the string data
	if len(value) > 0 {
		cStr := C.CString(value)
		defer C.free(unsafe.Pointer(cStr))
		C.Variant_FromAStr(cStr, C.size_t(len(value)), &w.variant)
	}
}

// FromBytes creates a C variant from bytes
func (w *CVariantWrapper) FromBytes(value []byte) {
	// Allocate data buffer if needed
	if len(w.dataBuffer) < len(value)+10 {
		w.dataBuffer = make([]byte, len(value)+10)
	}
	// Allocate serialization buffer if needed
	if len(w.buffer) < len(value)+10 {
		w.buffer = make([]byte, len(value)+10)
	}
	// Initialize the variant with the data buffer
	C.Variant_Init(C.VAR_TYPE_BYTES, unsafe.Pointer(&w.dataBuffer[0]), C.size_t(len(w.dataBuffer)), &w.variant)
	// Copy the byte data
	if len(value) > 0 {
		C.Variant_FromBytes((*C.byte)(unsafe.Pointer(&value[0])), C.size_t(len(value)), &w.variant)
	}
}

// ToBuffer serializes the C variant to a buffer
func (w *CVariantWrapper) ToBuffer() []byte {
	size := C.Variant_ToBuffer(&w.variant, unsafe.Pointer(&w.buffer[0]), C.size_t(len(w.buffer)))
	if size == 0 {
		return nil
	}
	return w.buffer[:size]
}

// ToPacket serializes the C variant to a packet with name
func (w *CVariantWrapper) ToPacket(name string) []byte {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	size := C.Variant_ToPacket(cName, &w.variant, unsafe.Pointer(&w.buffer[0]), C.size_t(len(w.buffer)))
	if size == 0 {
		return nil
	}
	return w.buffer[:size]
}

// FromBuffer deserializes a C variant from a buffer
func (w *CVariantWrapper) FromBuffer(data []byte) bool {
	size := C.Variant_FromBuffer((*C.byte)(unsafe.Pointer(&data[0])), C.size_t(len(data)), &w.variant)
	return size != 0
}

// FromPacket deserializes a C variant from a packet
func (w *CVariantWrapper) FromPacket(data []byte) (string, bool) {
	nameBuffer := make([]byte, 128)
	size := C.Variant_FromPacket(
		(*C.byte)(unsafe.Pointer(&data[0])),
		C.size_t(len(data)),
		(*C.char)(unsafe.Pointer(&nameBuffer[0])),
		C.size_t(len(nameBuffer)),
		&w.variant,
	)
	if size == 0 {
		return "", false
	}

	// Find null terminator
	nameLen := 0
	for nameLen < len(nameBuffer) && nameBuffer[nameLen] != 0 {
		nameLen++
	}
	return string(nameBuffer[:nameLen]), true
}

// AsUInt8 returns the variant value as uint8
func (w *CVariantWrapper) AsUInt8() uint8 {
	return uint8(C.Variant_ToUInt8(&w.variant, 0))
}

// AsUInt16 returns the variant value as uint16
func (w *CVariantWrapper) AsUInt16() uint16 {
	return uint16(C.Variant_ToUInt16(&w.variant, 0))
}

// AsUInt32 returns the variant value as uint32
func (w *CVariantWrapper) AsUInt32() uint32 {
	return uint32(C.Variant_ToUInt32(&w.variant, 0))
}

// AsUInt64 returns the variant value as uint64
func (w *CVariantWrapper) AsUInt64() uint64 {
	return uint64(C.Variant_ToUInt64(&w.variant, 0))
}

// AsInt8 returns the variant value as int8
func (w *CVariantWrapper) AsInt8() int8 {
	return int8(C.Variant_ToSInt8(&w.variant, 0))
}

// AsInt16 returns the variant value as int16
func (w *CVariantWrapper) AsInt16() int16 {
	return int16(C.Variant_ToSInt16(&w.variant, 0))
}

// AsInt32 returns the variant value as int32
func (w *CVariantWrapper) AsInt32() int32 {
	return int32(C.Variant_ToSInt32(&w.variant, 0))
}

// AsInt64 returns the variant value as int64
func (w *CVariantWrapper) AsInt64() int64 {
	return int64(C.Variant_ToSInt64(&w.variant, 0))
}

// AsString returns the variant value as a string
func (w *CVariantWrapper) AsString(maxLen int) string {
	buffer := make([]byte, maxLen)
	size := C.Variant_ToAStr(&w.variant, (*C.char)(unsafe.Pointer(&buffer[0])), C.size_t(len(buffer)))
	if size == 0 {
		return ""
	}
	return string(buffer[:size])
}

// AsBytes returns the variant value as bytes
func (w *CVariantWrapper) AsBytes(maxLen int) []byte {
	buffer := make([]byte, maxLen)
	size := C.Variant_ToBytes(&w.variant, (*C.byte)(unsafe.Pointer(&buffer[0])), C.size_t(len(buffer)))
	if size == 0 {
		return nil
	}
	return buffer[:size]
}

// PrepareMap prepares a C variant as a map
func (w *CVariantWrapper) PrepareMap() {
	C.Variant_Prepare(C.VAR_TYPE_MAP, unsafe.Pointer(&w.buffer[0]), C.size_t(len(w.buffer)), &w.variant)
}

// PrepareList prepares a C variant as a list
func (w *CVariantWrapper) PrepareList() {
	C.Variant_Prepare(C.VAR_TYPE_LIST, unsafe.Pointer(&w.buffer[0]), C.size_t(len(w.buffer)), &w.variant)
}

// InsertUInt8 inserts a uint8 value into a map variant
func (w *CVariantWrapper) InsertUInt8(name string, value uint8) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	C.Variant_InsertUInt8(&w.variant, cName, C.uint8(value))
}

// InsertUInt16 inserts a uint16 value into a map variant
func (w *CVariantWrapper) InsertUInt16(name string, value uint16) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	C.Variant_InsertUInt16(&w.variant, cName, C.uint16(value))
}

// InsertUInt32 inserts a uint32 value into a map variant
func (w *CVariantWrapper) InsertUInt32(name string, value uint32) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	C.Variant_InsertUInt32(&w.variant, cName, C.uint32(value))
}

// InsertString inserts a string value into a map variant
func (w *CVariantWrapper) InsertString(name, value string) {
	cName := C.CString(name)
	cValue := C.CString(value)
	defer C.free(unsafe.Pointer(cName))
	defer C.free(unsafe.Pointer(cValue))
	C.Variant_InsertAStr(&w.variant, cName, cValue, C.size_t(len(value)))
}

// AppendUInt8 appends a uint8 value to a list variant
func (w *CVariantWrapper) AppendUInt8(value uint8) {
	C.Variant_AppendUInt8(&w.variant, C.uint8(value))
}

// Finish finalizes the variant preparation
func (w *CVariantWrapper) Finish() []byte {
	size := C.Variant_Finish(unsafe.Pointer(&w.buffer[0]), &w.variant)
	if size == 0 {
		return nil
	}
	return w.buffer[:size]
}

// Find searches for a key in a map variant
func (w *CVariantWrapper) Find(name string) *CVariantWrapper {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	outWrapper := NewCVariantWrapper(0)
	result := C.Variant_Find(&w.variant, cName, &outWrapper.variant)
	if result == 0 {
		return nil
	}
	return outWrapper
}

// FindUInt32 finds and returns a uint32 value from a map variant
func (w *CVariantWrapper) FindUInt32(name string) uint32 {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	return uint32(C.Variant_FindUInt32(&w.variant, cName, 0))
}

// FindUInt16 finds and returns a uint16 value from a map variant
func (w *CVariantWrapper) FindUInt16(name string) uint16 {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	return uint16(C.Variant_FindUInt16(&w.variant, cName, 0))
}

// FindString finds and returns a string value from a map variant
func (w *CVariantWrapper) FindString(name string, maxLen int) string {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	buffer := make([]byte, maxLen)
	size := C.Variant_FindAStr(&w.variant, cName, (*C.char)(unsafe.Pointer(&buffer[0])), C.size_t(len(buffer)))
	if size == 0 {
		return ""
	}
	return string(buffer[:size])
}

// AtUInt8 returns the uint8 value at a given position in a list
func (w *CVariantWrapper) AtUInt8(pos int) uint8 {
	return uint8(C.Variant_UInt8At(&w.variant, C.int(pos), 0))
}

// GetSize returns the size of the variant data
func (w *CVariantWrapper) GetSize() uint32 {
	return uint32(w.variant.uSize)
}

// GetData returns the variant data pointer
func (w *CVariantWrapper) GetData() []byte {
	if w.variant.uSize == 0 {
		return nil
	}
	// Access the union field by computing its offset
	// The union is after uType (1 byte), bError (1 byte), uSize (4 bytes), uMaxSize (4 bytes) = 10 bytes offset
	// But we need to account for struct padding - on most platforms this will be aligned to 8 bytes
	// So the union starts at offset 16 (after 2 bytes padding)
	varPtr := unsafe.Pointer(&w.variant)
	unionOffset := unsafe.Sizeof(w.variant.uType) + unsafe.Sizeof(w.variant.bError) +
		unsafe.Sizeof(w.variant.uSize) + unsafe.Sizeof(w.variant.uMaxSize)
	// Round up to 8-byte boundary
	unionOffset = (unionOffset + 7) & ^uintptr(7)
	pData := *(**C.uchar)(unsafe.Pointer(uintptr(varPtr) + unionOffset))
	return (*[1 << 30]byte)(unsafe.Pointer(pData))[:w.variant.uSize:w.variant.uSize]
}
