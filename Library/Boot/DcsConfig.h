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
// DcsProp - the boot loader's configuration file on the EFI partition.
//
// The format is one flat list of named strings:
//
//   <DiskCrypto><configuration>
//     <config key="KeyboardLayout">1</config>
//     <config key="VerboseDebug">0</config>
//   </configuration></DiskCrypto>
//
// which is held here as an StVariant map of key to string, and written back
// out whole. That is the important property: the map carries every key the
// file had, not only the ones this program has a name for, so a setting written
// by DiskCryptor - or by a newer DCS than this build knows about - survives
// being loaded, edited and saved. There is no list of recognised keys to fall
// behind.
//
// Ported from the config half of DiskCryptor's dcapi/efiinst.c.
//

#include "../lib_global.h"
#include "../Status.h"
#include "../Common/StVariant.h"

#include <string>
#include <vector>

//
// The keys this program has an opinion about. Everything else in the file is
// carried through untouched, so this list is a convenience rather than a
// definition of what a DcsProp may contain.
//
#define DCS_KEY_KEYBOARD_LAYOUT		L"KeyboardLayout"
#define DCS_KEY_VERBOSE_DEBUG		L"VerboseDebug"
#define DCS_KEY_PASSWORD_MSG		L"PasswordMsg"
#define DCS_KEY_AUTH_RETRY			L"AuthorizeRetry"
#define DCS_KEY_PASSWORD_TIMEOUT	L"PasswordTimeout"
#define DCS_KEY_DCS_MODULE			L"DcsModule"

class LIBRARY_EXPORT CDcsConfig
{
public:
	CDcsConfig();

	//
	// Parse and serialise. FromXml takes the file as it is on disk - UTF-8,
	// which is what the declaration says - and ToXml produces the same.
	//
	STATUS					FromXml(const std::string& Xml);
	std::string				ToXml() const;

	// Read from / write to \EFI\DCS\DcsProp on an EFI partition.
	STATUS					Load(const std::wstring& EspRoot);
	STATUS					Save(const std::wstring& EspRoot) const;

	//
	// The whole map. A caller that wants to show everything, or replace the lot,
	// works on this; the accessors below are for the handful of keys with a
	// control of their own.
	//
	const StVariant&		GetValues() const { return m_Values; }
	void					SetValues(const StVariant& Values);

	bool					Has(const std::wstring& Key) const;
	std::wstring			GetStr(const std::wstring& Key, const std::wstring& Default = L"") const;
	int						GetInt(const std::wstring& Key, int Default = 0) const;
	void					SetStr(const std::wstring& Key, const std::wstring& Value);
	void					SetInt(const std::wstring& Key, int Value);
	void					Remove(const std::wstring& Key);

	// Keys in the order the file listed them, with anything added since at the
	// end. Only so that saving a file does not reshuffle it.
	const std::vector<std::wstring>&	GetKeyOrder() const { return m_Order; }

	bool					IsEmpty() const { return m_Order.empty(); }

	// The minimal configuration a fresh install starts from.
	static CDcsConfig		MakeDefault();

protected:
	void					Touch(const std::wstring& Key);

	StVariant				m_Values;	// VAR_TYPE_MAP, key -> string
	std::vector<std::wstring>	m_Order;
};
