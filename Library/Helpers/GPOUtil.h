#pragma once
#include "../lib_global.h"
//#include "../Status.h"

class LIBRARY_EXPORT CGPO
{
public:

	CGPO();
	~CGPO();

	HRESULT OpenGPO(bool bWritable = false);

	HKEY GetUserRoot();
	HKEY GetMachineRoot();

	HRESULT SaveGPO();

	void Close();

private:
	struct SGPO* m;
};