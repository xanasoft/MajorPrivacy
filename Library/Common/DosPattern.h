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

#pragma once

/*extern "C" {
#include "../Driver.h"
}*/

#define NOGDI
#include "../../Framework/Header.h"

#include "../../Framework/Object.h"
#include "../../Framework/String.h"

/////////////////////////////////////////////////////////////////////////////
// CDosPattern
//

class CDosPattern: public FW::Object
{
public:
	CDosPattern(FW::AbstractMemPool* pMemPool);
	CDosPattern(FW::AbstractMemPool* pMemPool, const FW::StringW& str);
	~CDosPattern();

	bool Set(const FW::StringW& str);
	FW::StringW Get() const { return m_Pattern; }
	bool IsEmpty() const;
	bool IsExact() const;
	int GetWildCount() const;

	ULONG Match(FW::StringW str) const;

protected:
	FW::StringW m_Pattern;

	struct _PATTERN* m_pPattern = nullptr;
};

typedef FW::SharedPtr<CDosPattern> CDosPatternPtr;
