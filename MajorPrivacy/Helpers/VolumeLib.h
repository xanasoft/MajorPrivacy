#pragma once
/*
 * Copyright 2026 David Xanatos, xanasoft.com
 *
 * This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

//
// dcvolume.dll, loaded the first time something asks for it and never before.
//
// MajorPrivacy has to start and keep working on a machine where this DLL was
// never installed - it is a privacy suite that can also edit volume headers,
// not a volume tool that happens to have a tray icon. So nothing links against
// it, nothing touches it at startup, and the one feature that needs it reports
// a plain error rather than the process failing to load.
//
// ImBox.exe takes the opposite deal: it links the import library and is bound
// before main, because it cannot open a single container without this code.
//

#include <QString>

extern "C" {
// spelled out rather than by name: this file used to be called
// DcVolume.h, which on a case-insensitive filesystem matched itself
#include "dcvolume.h"
}

//
// DiskCryptor's defines.h hands out min and max as macros, and this header is
// included from C++ that also uses std::min and QWidget::maximumSize. The
// macros have done their job by the time the include finishes; nothing below
// this line should still be seeing them.
//
#ifdef min
 #undef min
#endif
#ifdef max
 #undef max
#endif

class CDcVolume
{
public:
	//
	// The function table, or NULL when the library is absent, is not loadable,
	// or implements a different API version. Cached after the first call,
	// including the failure - a DLL that was missing a moment ago is not going
	// to be there on the next click, and retrying would just repeat the error.
	//
	static const dcvol_api*	Api();

	static bool				IsAvailable() { return Api() != nullptr; }

	// Why Api() returned NULL, ready to put in front of a user.
	static QString			Error();
};
