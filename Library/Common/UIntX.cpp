#include "pch.h"
#include "UIntX.h"
#include "Exception.h"
#include "Strings.h"

void CUIntX::Init(bool bFill)
{
	for (UINT i=0; i<GetDWordCount(); i++)
		SetDWord(i, bFill ? -1 : 0);
}

void CUIntX::Init(const CUIntX& uValue, UINT uBits, bool bRand)
{
	memcpy(GetData(), uValue.GetData(), GetSize());

	if(uBits != -1)
	{
		// fill rest with random data
		for (UINT i = uBits; i < GetBitSize(); i++)
			SetBit(i, bRand ? (rand() & 2) : 0); // ToDo: user better random
	}
}

void CUIntX::Init(uint32 uValue)
{
	SetDWord(0, uValue);
	for (UINT i=1; i<GetDWordCount(); i++)
		SetDWord(i, 0);
}

void CUIntX::Init(const unsigned char* pRaw)
{
	memcpy(GetData(), pRaw, GetSize());
}

void CUIntX::Init(const StVariant& Variant)
{
	if(Variant.GetSize() != GetSize())
		throw CException(L"Variant is not a uInt128");
	memcpy(GetData(), Variant.GetData(), GetSize());
}

//void CUIntX::SetRandomValue()
//{
//	for (UINT i=0; i<GetDWordCount(); i++)
//		SetDWord(GetRand64(), 0);
//}

bool CUIntX::GetBit(UINT uBit, bool bBigEndian) const
{
	if (uBit >= GetBitSize())
		return false;
	if(bBigEndian)
		uBit = (GetBitSize() - 1) - uBit;

	UINT iNum = uBit >> 3;
	UINT iShift = uBit & 7;
	const byte* Data = GetData();
	return ((Data[iNum] >> iShift) & 1);
}

void CUIntX::SetBit(UINT uBit, bool bValue, bool bBigEndian)
{
	if (uBit >= GetBitSize())
		return;
	if(bBigEndian)
		uBit = (GetBitSize() - 1) - uBit;

	UINT iNum = uBit >> 3;
	UINT iShift = uBit & 7;
	byte* Data = GetData();
	if (bValue)
		Data[iNum] |= (1 << iShift);
	else
		Data[iNum] &= ~(1 << iShift);
}

void CUIntX::And(const CUIntX& uValue)
{
	for (UINT i=0; i<GetDWordCount(); i++)
		SetDWord(i, GetDWord(i) & uValue.GetDWord(i));
}

void CUIntX::Or(const CUIntX& uValue)
{
	for (UINT i=0; i<GetDWordCount(); i++)
		SetDWord(i, GetDWord(i) | uValue.GetDWord(i));
}

void CUIntX::Xor(const CUIntX& uValue)
{
	for (UINT i=0; i<GetDWordCount(); i++)
		SetDWord(i, GetDWord(i) ^ uValue.GetDWord(i));
}

void CUIntX::Add(const CUIntX& uValue)
{
	if (uValue.IsNull())
		return;
	__int64 iSum = 0;
	for (UINT i=0; i<GetDWordCount(); i++)
	{
		iSum += GetDWord(i);
		iSum += uValue.GetDWord(i);
		SetDWord(i, (uint32)iSum);
		iSum = iSum >> 32;
	}
}

void CUIntX::Subtract(const CUIntX& uValue)
{
	if (uValue.IsNull())
		return;
	__int64 iSum = 0;
	for (int i = GetDWordCount() - 1; i>=0; i--)
	{
		iSum += GetDWord(i);
		iSum -= uValue.GetDWord(i);
		SetDWord(i, (uint32)iSum);
		iSum = iSum >> 32;
	}
}

void CUIntX::Multi(uint32 uValue)
{
	if(uValue == 0)
		Init(false);
	else if((uValue & (uValue - 1)) == 0)
	{
		int Pos = 0;
		uValue -= 1;
		for (; uValue > 0; uValue >>= 1, ++Pos);
		ShiftLeft(Pos);
	}
	else
	{
		ASSERT(0);
		/*CUIntX Tmp = *this;
		for (int i = 0; i < uValue; i++)
			Add(Tmp);*/
	}
}

void CUIntX::Div(uint32 uValue)
{
	if(uValue == 0)
		Init(true);
	else if((uValue & (uValue - 1)) == 0)
	{
		int Pos = 0;
		uValue -= 1;
		for (; uValue > 0; uValue >>= 1, ++Pos);
		ShiftRight(Pos);
	}
	else
	{
		ASSERT(0);
		/*uint32 uBit, uRemain = 0;
		for (int i = 0; i < 128; i++)
		{
			uBit = GetBit(0);
			uRemain <<= 1;
			if (uBit)
				uRemain |= 1;
			ShiftLeft(1);
			if (uRemain >= uValue)
			{
				uRemain -= uValue;
				SetBit(127, 1);
			}
		}*/
	}
}

bool CUIntX::IsNull() const
{
	for (UINT i=0; i<GetDWordCount(); i++)
	{
		if(GetDWord(i) != 0)
			return false;
	}
	return true;
}

int CUIntX::CompareTo(const CUIntX& uOther) const
{
	for (int i = GetDWordCount() - 1; i >= 0; i--)
	{
		if (GetDWord(i) < uOther.GetDWord(i))
			return -1;
		if (GetDWord(i) > uOther.GetDWord(i))
			return 1;
	}
	return 0;
}

void CUIntX::ShiftLeft(UINT uBits)
{
	if ((uBits == 0) || IsNull())
		return;
	if (uBits > GetBitSize())
	{
		Init(false);
		return;
	}

	byte* Data = GetData();
	UINT iIndexShift = uBits >> 3;

	int i = (int)(GetSize() - iIndexShift) - 1;
	short iShifted = ((short)Data[i]) << (uBits & 7);
	Data[i+iIndexShift] = (byte)iShifted;
	for (i--; i >= 0; i--)
	{
		iShifted = ((short)Data[i]) << (uBits & 7);
		Data[i+iIndexShift] = (byte)iShifted;
		Data[i+iIndexShift+1] |= (byte)(iShifted >> 8);
	}
	for (UINT i = 0; i < iIndexShift; i++)
		Data[i] = 0;
}

void CUIntX::ShiftRight(UINT uBits)
{
	if ((uBits == 0) || IsNull())
		return;
	if (uBits > GetBitSize())
	{
		Init(false);
		return;
	}

	byte* Data = GetData();
	UINT iIndexShift = uBits >> 3;

	UINT i = iIndexShift;
	short iShifted = ((short)Data[i]) << (8 - (uBits & 7));
	for (i++; i < GetSize(); i++)
	{
		Data[i-iIndexShift-1] = (byte)(iShifted >> 8);
		iShifted = ((short)Data[i]) << (8 - (uBits & 7));
		Data[i-iIndexShift-1] |= (byte)(iShifted);
	}
	Data[i-iIndexShift-1] = (byte)(iShifted >> 8);
	for (size_t i = GetSize() - 1; i >= GetSize() - iIndexShift; i--)
		Data[i] = 0;
}

void CUIntX::Invert()
{
	for (UINT i=0; i<GetDWordCount(); i++)
		SetDWord(i, ~GetDWord(i));
}

std::wstring CUIntX::ToHex() const
{
	const byte* pData = GetData();
	std::wstring Hex;
	for(size_t i = GetSize() - 1; i >= 0; i--)
	{
		wchar_t buf[3];

		buf[0] = (pData[i] >> 4) & 0xf;
		if (buf[0] < 10)
            buf[0] += '0';
        else
            buf[0] += 'A' - 10;

		buf[1] = (pData[i]) & 0xf;
		if (buf[1] < 10)
            buf[1] += '0';
        else
            buf[1] += 'A' - 10;

		buf[2] = 0;
	
		Hex.append(buf);
	}
	return Hex;
}

bool CUIntX::FromHex(const std::wstring& Hex)
{
	if(Hex.length() != GetSize()*2)
		return false;

	byte* pData = GetData();
	int j = 0;
	for(size_t i = GetSize() - 1; i >= 0; i--)
	{
		wchar_t ch1 = Hex[i*2];
		int dig1;
		if(isdigit(ch1)) 
			dig1 = ch1 - '0';
		else if(ch1>='A' && ch1<='F') 
			dig1 = ch1 - 'A' + 10;
		else if(ch1>='a' && ch1<='f') 
			dig1 = ch1 - 'a' + 10;
		else
			return false;

		wchar_t ch2 = Hex[i*2 + 1];
		int dig2;
		if(isdigit(ch2)) 
			dig2 = ch2 - '0';
		else if(ch2>='A' && ch2<='F') 
			dig2 = ch2 - 'A' + 10;
		else if(ch2>='a' && ch2<='f') 
			dig2 = ch2 - 'a' + 10;
		else
			return false;

		pData[j++] = dig1*16 + dig2;
	}
	return true;
}

std::wstring CUIntX::ToBin() const
{
	std::wstring Bin;
	for(int i = GetBitSize() - 1; i >= 0; i--)
		Bin.append(GetBit(i, false) ? L"1" : L"0");
	return Bin;
}