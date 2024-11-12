/*
* Copyright (c) 2023-2024 David Xanatos, xanasoft.com
* All rights reserved.
*
* This file is part of MajorPrivacy.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
*/

#include "DosPattern.h"
extern "C" {
//#include "../driver.h"
//#include "../../Driver/SBIE/pool.h"
#include "../../Driver/SBIE/list.h"
#include "../../Driver/SBIE/pattern.h"
}

/////////////////////////////////////////////////////////////////////////////
// CDosPattern
//

CDosPattern::CDosPattern(FW::AbstractMemPool* pMemPool)
	: FW::Object(pMemPool), m_Pattern(pMemPool)
{
	//DbgPrintEx(DPFLTR_DEFAULT_ID, 0xFFFFFFFF, "CDosPattern::CDosPattern()\n");
}

CDosPattern::CDosPattern(FW::AbstractMemPool* pMemPool, const FW::StringW& str)
	: FW::Object(pMemPool), m_Pattern(pMemPool)
{
	//DbgPrintEx(DPFLTR_DEFAULT_ID, 0xFFFFFFFF, "CDosPattern::CDosPattern()\n");

	Set(str);
}

CDosPattern::~CDosPattern()
{
	//DbgPrintEx(DPFLTR_DEFAULT_ID, 0xFFFFFFFF, "CDosPattern::~CDosPattern()\n");

	if(m_pPattern)
		Pattern_Free(m_pPattern);
}

bool CDosPattern::Set(const FW::StringW& str)
{
	if (m_pPattern) {
		Pattern_Free(m_pPattern);
		m_pPattern = NULL;
	}

	m_Pattern = str;
	if(str.IsEmpty())
		return true;

	m_pPattern = Pattern_Create(Driver_Pool, str.ConstData(), TRUE, 0);
	return !!m_pPattern;
}

ULONG CDosPattern::Match(FW::StringW str) const
{
	if (!m_pPattern)
		return 0;	
	if(wchar_t* pstr = str.Data())
		_wcslwr(pstr); // make string lower // todo

	//
	// This function returns the match length 
	// which is defined as the length of the string matching up to the first wildcard character + 1,
	// if there are no wild cards the length of the string is returned.
	// 
	// Therefor IsExact() must take rpecedence over the match length
	//

	ULONG Ret = (ULONG)Pattern_MatchX(m_pPattern, str.ConstData(), (int)str.Length());

	//
	// TODO: we want the a pattern like C:\Test\* to also match C:\Test
	// fix this to not need a second comparison !!!!
	//

	if (Ret == 0) {
		str.Append(L"\\");
		Ret = (ULONG)Pattern_MatchX(m_pPattern, str.ConstData(), (int)str.Length());
	}
	return Ret;
}

bool CDosPattern::IsEmpty() const
{
	return !m_pPattern;
}

bool CDosPattern::IsExact() const
{
	if (!m_pPattern) return false;
	return Pattern_Exact(m_pPattern);
}

int CDosPattern::GetWildCount() const
{
	if (!m_pPattern) return 0;
	return Pattern_Wildcards(m_pPattern);
}