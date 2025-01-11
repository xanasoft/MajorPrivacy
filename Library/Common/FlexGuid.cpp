#include "pch.h"
#include "FlexGuid.h"

#ifdef _DEBUG
const int CFlexGuid__Size = sizeof(CFlexGuid); // must be 24 bytes
#endif

CFlexGuid::CFlexGuid()
{
    memset(m_Data, 0, 16);
}

CFlexGuid::CFlexGuid(CFlexGuid&& Other) 
{
    m_Type = Other.m_Type;
    Other.m_Type = Null;
    memcpy(m_Data, Other.m_Data, 16);
}

CFlexGuid::~CFlexGuid()
{
    if (IsString())
        free(m_String);
}

CFlexGuid& CFlexGuid::operator = (const CFlexGuid& Other)
{
	if (this == &Other)
		return *this;

    if (IsString())
        free(m_String);

	m_Type = Other.m_Type;
	if (Other.m_Type == Unicode) {
		m_Length = Other.m_Length;
		m_String = malloc(sizeof(wchar_t) * (m_Length + 1));
		wmemcpy((wchar_t*)m_String, (wchar_t*)Other.m_String, m_Length + 1);
	} else if (Other.m_Type == Ascii) {
        m_Length = Other.m_Length;
        m_String = malloc(m_Length + 1);
        memcpy(m_String, Other.m_String, m_Length + 1);
    } else
		memcpy(m_Data, Other.m_Data, 16);

	return *this;
}

void CFlexGuid::Clear()
{
    if(IsNull())
		return;
	if (IsString())
		free(m_String);
	m_Type = Null;
	m_LowerCaseMask = 0;
    memset(m_Data, 0, 16);
}

void CFlexGuid::SetRegularGuid(const SGuid* pGuid)
{
    if (IsString())
        free(m_String);

	m_Type = Regular;
    m_LowerCaseMask = 0;
	memcpy(m_Data, pGuid, 16);
}

SGuid* CFlexGuid::GetRegularGuid() const
{
	if (IsString())
		return nullptr;
	return (SGuid*)m_Data; // Null and Regular both give valid data
}

int CFlexGuid::Compare(const CFlexGuid& other) const
{
	if (m_Type == Null && other.m_Type == Null) return 0;
    if (m_Type < other.m_Type) return -1;
    if (m_Type > other.m_Type) return 1;

    if (!IsString()) 
        return memcmp(m_Data, other.m_Data, 16);

    if (m_Length < other.m_Length) return -1;
    if (m_Length > other.m_Length) return 1;

    // Note: a unicode and ascii string guid can not be the same, 
    // if a string guid would be savable as ascii it would have stored as such

	if (m_Type == Unicode)
		return _wcsnicmp((wchar_t*)m_String, (wchar_t*)other.m_String, m_Length);
    return _strnicmp((char*)m_String, (char*)other.m_String, m_Length);
}

template <typename T>
bool CFlexGuid__ParseGuid(const T* str, size_t length, unsigned char outBytes[16], uint32& upperCaseMask, uint32& lowerCaseMask)
{
    //
    // This function attempts to parse the string as a standard GUID.
    // Example formats:
    //   - {00000000-0000-0000-0000-000000000000}
    //   -  00000000-0000-0000-0000-000000000000 
    //      (with or without braces, or dashes)
    //
    // On success, the 16 bytes of the GUID are stored in outBytes,
    // and the two bitmasks indicate which nibbles were uppercase or lowercase letters.
    //

    // If string is too short to hold at least 32 hex digits, fail early
    if (length < 32) 
        return false;

    // Clear the bitmasks
    upperCaseMask = 0;
    lowerCaseMask = 0;

    // Optionally skip leading '{' and trailing '}' if present
    size_t start = 0;
    if (length >= 2 && str[0] == (T)'{' && str[length - 1] == (T)'}')
    {
        start  = 1;
        length -= 2;
    }

    // We'll gather 32 nibbles -> 16 bytes
    size_t nibCount       = 0; // number of bytes completed
    size_t nibIndex       = 0; // tracks each nibble (0..31)
    unsigned char byteVal = 0;
    int halfByte          = 0; // 0 or 1: which nibble we are filling

    for (size_t i = 0; i < length && nibCount < 16; ++i)
    {
        T c = str[start + i];

        // Skip dashes
        if (c == (T)'-')
            continue;

        // Convert this character to its nibble value
        unsigned char val;
        if (c >= (T)'0' && c <= (T)'9')
        {
            val = static_cast<unsigned char>(c - (T)'0');
        }
        else if (c >= (T)'a' && c <= (T)'f')
        {
            val = static_cast<unsigned char>(10 + (c - (T)'a'));
            // Record lowercase for this nibble
            lowerCaseMask |= (1u << nibIndex);
        }
        else if (c >= (T)'A' && c <= (T)'F')
        {
            val = static_cast<unsigned char>(10 + (c - (T)'A'));
            // Record uppercase for this nibble
            upperCaseMask |= (1u << nibIndex);
        }
        else
        {
            // Invalid character for a hex digit
            return false;
        }

        // Place nibble into the output array
        if (halfByte == 0)
        {
            // High nibble
            byteVal = static_cast<unsigned char>(val << 4);
            halfByte = 1;
        }
        else
        {
            // Low nibble
            byteVal |= val;
            outBytes[nibCount++] = byteVal;
            halfByte = 0;
        }

        ++nibIndex;  // move to the next nibble
    }

    // Must have exactly 16 bytes (32 hex nibbles),
    // and not be stuck in the "high nibble only" state
    if (nibCount != 16 || halfByte != 0)
        return false;

    return true;
}

bool CFlexGuid__IsAscii(const wchar_t* pStr, size_t uLen)
{
	for (size_t i = 0; i < uLen; i++) {
		wchar_t c = pStr[i];
		if (c < 0 || c > 0x007F)
			return false;
	}
	return true;
}

void CFlexGuid::FromWString(const wchar_t* pStr, size_t uLen, bool bIgnoreCase)
{
    Clear();
    if (!pStr || !uLen)
        return;

    if (uLen == (size_t)-1)
        uLen = wcslen(pStr);

    uint32 upperCaseMask = 0;
    uint32 lowerCaseMask = 0;
    bool bOk = CFlexGuid__ParseGuid(pStr, uLen, m_Data, upperCaseMask, lowerCaseMask);
    if (bIgnoreCase)
		lowerCaseMask = 0; // Ignore case
    if (bOk && upperCaseMask && lowerCaseMask)
		bOk = false; // Both upper and lower case hex digits were seen, which is irregular

    if (bOk)
        m_Type = Regular;
    else
    {
        m_Length = uLen;
		m_LowerCaseMask = lowerCaseMask;

        if (CFlexGuid__IsAscii(pStr, uLen))
        {
            m_Type = Ascii;
            
            m_String = malloc(uLen + 1);
            for (size_t i = 0; i < uLen; i++)
                ((char*)m_String)[i] = (char)pStr[i];
            ((char*)m_String)[uLen] = L'\0';
        }
        else // we cant store it as asci so go unicode
        {
            m_Type = Unicode;

            m_String = malloc(sizeof(wchar_t) * (uLen + 1));
            wmemcpy((wchar_t*)m_String, pStr, uLen);
            ((wchar_t*)m_String)[uLen] = L'\0';
        }
    }
}

void CFlexGuid::FromAString(const char* pStr, size_t uLen, bool bIgnoreCase)
{
    Clear();
    if (!pStr || !uLen)
        return;

    if (uLen == (size_t)-1)
        uLen = strlen(pStr);

    uint32 upperCaseMask = 0;
    uint32 lowerCaseMask = 0;
    bool bOk = CFlexGuid__ParseGuid(pStr, uLen, m_Data, upperCaseMask, lowerCaseMask);
    if (bIgnoreCase)
        lowerCaseMask = 0; // Ignore case
    if (bOk && upperCaseMask && lowerCaseMask)
        bOk = false; // Both upper and lower case hex digits were seen, which is irregular

    if (bOk)
        m_Type = Regular;
    else
    {
        m_Length = uLen;
        m_LowerCaseMask = lowerCaseMask;
        m_Type = Ascii;

        m_String = malloc(uLen + 1);
        memcpy(m_String, pStr, uLen);
        ((char*)m_String)[uLen] = L'\0'; 
    }
}

template <typename T>
void CFlexGuid__ToString(const unsigned char* Data, T* pStr, std::uint32_t lowerCaseMask)
{
    // We assume pStr is a valid, non-null pointer that can hold at least 39 T-characters
    // (including the null-terminator). The full format is 38 characters plus a null terminator:
    // {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}\0

    // Arrays for converting a nibble (0–15) into a single hex character
    // in uppercase or lowercase
    static const char s_hexUpper[] = "0123456789ABCDEF";
    static const char s_hexLower[] = "0123456789abcdef";

    // Helper to pick the correct character for a given nibble
    // based on the lowerCaseMask:
    auto nibChar = [&](unsigned char nibValue, std::uint32_t nibIndex) -> T {
        if (lowerCaseMask & (1u << nibIndex))
            return static_cast<T>(s_hexLower[nibValue]);
        else
            return static_cast<T>(s_hexUpper[nibValue]);
    };

    // Write the leading brace
    pStr[0] = static_cast<T>('{');
    int pos = 1;

    // We'll keep track of how many nibbles we've written so far:
    std::uint32_t nibIndex = 0;

    // Helper lambda to append two hex digits for one byte in Data.
    auto appendByteAsHex = [&](unsigned char byteVal) {
        // High nibble
        pStr[pos++] = nibChar((byteVal >> 4) & 0x0F, nibIndex++);
        // Low nibble
        pStr[pos++] = nibChar(byteVal & 0x0F, nibIndex++);
    };

    // 1) First 4 bytes (8 hex digits)
    for (int i = 0; i < 4; i++)
        appendByteAsHex(Data[i]);
    pStr[pos++] = static_cast<T>('-');

    // 2) Next 2 bytes (4 hex digits)
    for (int i = 4; i < 6; i++)
        appendByteAsHex(Data[i]);
    pStr[pos++] = static_cast<T>('-');

    // 3) Next 2 bytes (4 hex digits)
    for (int i = 6; i < 8; i++)
        appendByteAsHex(Data[i]);
    pStr[pos++] = static_cast<T>('-');

    // 4) Next 2 bytes (4 hex digits)
    for (int i = 8; i < 10; i++)
        appendByteAsHex(Data[i]);
    pStr[pos++] = static_cast<T>('-');

    // 5) Last 6 bytes (12 hex digits)
    for (int i = 10; i < 16; i++)
        appendByteAsHex(Data[i]);

    // Close with '}'
    pStr[pos++] = static_cast<T>('}');
    // Null-terminate
    pStr[pos] = static_cast<T>('\0');
}

bool CFlexGuid::ToWString(wchar_t* pStr) const
{
    if (IsString())
        return false;

    CFlexGuid__ToString(m_Data, pStr, m_LowerCaseMask);
    return true;
}

bool CFlexGuid::ToAString(char* pStr) const
{
    if (IsString())
        return false;

    CFlexGuid__ToString(m_Data, pStr, m_LowerCaseMask);
    return true;
}

std::wstring CFlexGuid::ToWString() const 
{
    wchar_t GuidStr[39];
    if (ToWString(GuidStr))
        return std::wstring(GuidStr, 38);
    if (m_Type == Unicode)
        return std::wstring((wchar_t*)m_String, m_Length);
    if (m_Type == Ascii)
        return std::wstring((char*)m_String, ((char*)m_String) + m_Length);
    return std::wstring();
}

std::string CFlexGuid::ToString() const 
{
    char GuidStr[39];
    if (ToAString(GuidStr))
        return std::string(GuidStr, 38);
    if (m_Type == Ascii)
        return std::string((char*)m_String, m_Length);
    if (m_Type == Unicode)
    {
#ifdef _DEBUG
        DebugBreak();
#endif
        std::string str;
        for (size_t i = 0; i < m_Length; i++) {
			if (((wchar_t*)m_String)[i] > 0x007F) str.append(1, '?');
            else str.append(1, (char)((wchar_t*)m_String)[i]);
        }
        return str;
    }
    return std::string();
}

bool CFlexGuid::FromVariant(const CVariant& Variant)
{
    CVariant::EType Type = Variant.GetType();
    if ((Type == VAR_TYPE_UINT || Type == VAR_TYPE_SINT) && Variant.GetSize() == 16)
		SetRegularGuid((SGuid*)Variant.GetData());
	else if (Type == VAR_TYPE_BYTES && (Variant.GetSize() == 16 || Variant.GetSize() == 20)) {
        Clear();
        m_Type = Regular;
        //
        // WARNING: This is a HACK which relays on the order of the fields in the header!!!
        //
        memcpy(m_Data, Variant.GetData(), Variant.GetSize());
	}
    else if (Type == VAR_TYPE_ASCII && Variant.GetSize() >= 1)
		FromAString((char*)Variant.GetData(), Variant.GetSize());
    else if (Type == VAR_TYPE_UNICODE && Variant.GetSize() >= sizeof(wchar_t))
        FromWString((wchar_t*)Variant.GetData(), Variant.GetSize()/sizeof(wchar_t));
    else if (Type == VAR_TYPE_UTF8 && Variant.GetSize() >= 1) {
		std::wstring Str = Variant.ToWString(); // this will convert the utf8 to unicode
        FromWString(Str.c_str(), Str.size());
    } 
    else {
        Clear();
        return false;
    }
    return true;
}

CVariant CFlexGuid::ToVariant(bool bTextOnly) const
{
	if (IsNull())
		return CVariant();
    if (m_Type == Unicode)
        return CVariant((wchar_t*)m_String, m_Length);
    if (m_Type == Ascii)
        return CVariant((char*)m_String, m_Length);
	// Ok, it's a regular guid
    if (bTextOnly) {
        char GuidStr[39];
        CFlexGuid__ToString(m_Data, GuidStr, m_LowerCaseMask);
        return CVariant(GuidStr, 38);
    }
    //
    // WARNING: This is a HACK which relays on the order of the fields in the header!!!
    //
    return CVariant((byte*)m_Data, m_LowerCaseMask ? 20 : 16, VAR_TYPE_BYTES);
}