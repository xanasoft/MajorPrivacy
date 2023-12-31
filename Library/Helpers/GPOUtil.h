#pragma once
#include "../lib_global.h"
//#include "../Status.h"

class LIBRARY_EXPORT CGPO
{
public:

	CGPO();
	~CGPO();

	bool Open(bool bWritable = false);

	HKEY GetUserRoot();
	HKEY GetMachineRoot();

	bool Save();

	void Close();

private:
	struct SGPO* m;
};