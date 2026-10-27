/*
 * Copyright 2026 David Xanatos, xanasoft.com
 *
 * This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "pch.h"
#include "DcsConfig.h"
#include "EspLocator.h"

#include <windows.h>

static const wchar_t* g_DcsPropPath = L"\\EFI\\DCS\\DcsProp";

/* ---------------------------------------------------------------------------
 * Text
 * ------------------------------------------------------------------------ */

static std::wstring FromUtf8(const std::string& Text)
{
	if (Text.empty())
		return std::wstring();

	int uLen = MultiByteToWideChar(CP_UTF8, 0, Text.c_str(), (int)Text.size(), NULL, 0);
	if (uLen <= 0)
		return std::wstring();

	std::wstring Wide(uLen, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, Text.c_str(), (int)Text.size(), &Wide[0], uLen);
	return Wide;
}

static std::string ToUtf8(const std::wstring& Text)
{
	if (Text.empty())
		return std::string();

	int uLen = WideCharToMultiByte(CP_UTF8, 0, Text.c_str(), (int)Text.size(), NULL, 0, NULL, NULL);
	if (uLen <= 0)
		return std::string();

	std::string Narrow(uLen, '\0');
	WideCharToMultiByte(CP_UTF8, 0, Text.c_str(), (int)Text.size(), &Narrow[0], uLen, NULL, NULL);
	return Narrow;
}

//
// The five entities XML defines. Nothing here needs a general parser: a DcsProp
// value is a path, a number or a prompt, and character references beyond these
// have never appeared in one.
//
static std::string XmlDecode(const std::string& Text)
{
	std::string Out;
	Out.reserve(Text.size());

	for (size_t i = 0; i < Text.size(); )
	{
		if (Text[i] != '&') {
			Out += Text[i++];
			continue;
		}

		size_t uEnd = Text.find(';', i);
		if (uEnd == std::string::npos || uEnd - i > 6) {
			Out += Text[i++];	// a bare ampersand; take it as written
			continue;
		}

		std::string Entity = Text.substr(i + 1, uEnd - i - 1);
		if (Entity == "amp")		Out += '&';
		else if (Entity == "lt")	Out += '<';
		else if (Entity == "gt")	Out += '>';
		else if (Entity == "quot")	Out += '"';
		else if (Entity == "apos")	Out += '\'';
		else {
			Out += Text.substr(i, uEnd - i + 1);	// unknown, leave it alone
		}
		i = uEnd + 1;
	}

	return Out;
}

static std::string XmlEncode(const std::string& Text)
{
	std::string Out;
	Out.reserve(Text.size());

	for (size_t i = 0; i < Text.size(); i++)
	{
		switch (Text[i]) {
		case '&':	Out += "&amp;"; break;
		case '<':	Out += "&lt;"; break;
		case '>':	Out += "&gt;"; break;
		case '"':	Out += "&quot;"; break;
		case '\'':	Out += "&apos;"; break;
		default:	Out += Text[i]; break;
		}
	}

	return Out;
}

/* ---------------------------------------------------------------------------
 * CDcsConfig
 * ------------------------------------------------------------------------ */

CDcsConfig::CDcsConfig()
	: m_Values(nullptr, VAR_TYPE_MAP)
{
}

void CDcsConfig::Touch(const std::wstring& Key)
{
	for (size_t i = 0; i < m_Order.size(); i++) {
		if (m_Order[i] == Key)
			return;
	}
	m_Order.push_back(Key);
}

STATUS CDcsConfig::FromXml(const std::string& Xml)
{
	m_Values = StVariant(nullptr, VAR_TYPE_MAP);
	m_Order.clear();

	//
	// Scanned rather than parsed. The document has exactly one shape - a flat
	// run of <config key="..."> elements - and a scanner that looks for that
	// one shape cannot be confused by the surrounding structure, whereas a
	// half-written parser very much can.
	//
	size_t uPos = 0;
	for (;;)
	{
		size_t uStart = Xml.find("<config", uPos);
		if (uStart == std::string::npos)
			break;

		size_t uKeyAt = Xml.find("key=", uStart);
		size_t uTagEnd = Xml.find('>', uStart);
		if (uKeyAt == std::string::npos || uTagEnd == std::string::npos || uKeyAt > uTagEnd) {
			uPos = uStart + 7;
			continue;
		}

		// key="..." or key='...'
		size_t uQuoteAt = Xml.find_first_of("\"'", uKeyAt + 4);
		if (uQuoteAt == std::string::npos || uQuoteAt > uTagEnd) {
			uPos = uTagEnd + 1;
			continue;
		}
		char cQuote = Xml[uQuoteAt];
		size_t uKeyEnd = Xml.find(cQuote, uQuoteAt + 1);
		if (uKeyEnd == std::string::npos || uKeyEnd > uTagEnd) {
			uPos = uTagEnd + 1;
			continue;
		}

		std::string Key = XmlDecode(Xml.substr(uQuoteAt + 1, uKeyEnd - uQuoteAt - 1));

		std::string Value;
		if (uTagEnd > 0 && Xml[uTagEnd - 1] == '/') {
			// <config key="X"/> - present and empty
			uPos = uTagEnd + 1;
		}
		else {
			size_t uValueEnd = Xml.find("</config>", uTagEnd);
			if (uValueEnd == std::string::npos)
				break;
			Value = XmlDecode(Xml.substr(uTagEnd + 1, uValueEnd - uTagEnd - 1));
			uPos = uValueEnd + 9;
		}

		if (Key.empty())
			continue;

		std::wstring WideKey = FromUtf8(Key);
		std::wstring WideValue = FromUtf8(Value);
		m_Values.Insert(Key.c_str(), StVariant(WideValue.c_str(), WideValue.length()));
		Touch(WideKey);
	}

	if (m_Order.empty())
		return ERR(STATUS_UNSUCCESSFUL, L"The boot loader configuration file holds no settings.");
	return OK;
}

std::string CDcsConfig::ToXml() const
{
	std::string Xml =
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
		"<DiskCrypto>\r\n"
		"\t<configuration>\r\n";

	//
	// Written in the order the file was read in, with anything added since at
	// the end. DCS does not care, but a configuration that reshuffles itself
	// every time it is saved is needlessly hard to compare against a backup.
	//
	for (size_t i = 0; i < m_Order.size(); i++)
	{
		std::string Key = ToUtf8(m_Order[i]);
		StVariant Value = m_Values.Get(Key.c_str());

		Xml += "\t\t<config key=\"";
		Xml += XmlEncode(Key);
		Xml += "\">";
		Xml += XmlEncode(ToUtf8(Value.AsStr()));
		Xml += "</config>\r\n";
	}

	Xml +=
		"\t</configuration>\r\n"
		"</DiskCrypto>\r\n";

	return Xml;
}

STATUS CDcsConfig::Load(const std::wstring& EspRoot)
{
	std::vector<unsigned char> Data;
	STATUS Status = CEspLocator::ReadFile(EspRoot, g_DcsPropPath, Data);
	if (Status.IsError())
		return Status;

	//
	// A UTF-8 byte order mark is legal in front of the declaration and confuses
	// the scan for "<config"; it is simply skipped.
	//
	size_t uSkip = 0;
	if (Data.size() >= 3 && Data[0] == 0xEF && Data[1] == 0xBB && Data[2] == 0xBF)
		uSkip = 3;

	std::string Xml((const char*)Data.data() + uSkip, Data.size() - uSkip);
	return FromXml(Xml);
}

STATUS CDcsConfig::Save(const std::wstring& EspRoot) const
{
	std::string Xml = ToXml();
	return CEspLocator::WriteFile(EspRoot, g_DcsPropPath, Xml.data(), Xml.size());
}

void CDcsConfig::SetValues(const StVariant& Values)
{
	m_Values = Values;

	//
	// Keys the caller added that were not in the file go on the end; ones it
	// removed drop out of the order too.
	//
	std::vector<std::wstring> Order;
	for (size_t i = 0; i < m_Order.size(); i++) {
		if (Has(m_Order[i]))
			Order.push_back(m_Order[i]);
	}

	for (uint32 i = 0; i < m_Values.Count(); i++)
	{
		const char* pKey = m_Values.Key(i);
		if (!pKey) continue;

		std::wstring Key = FromUtf8(pKey);
		bool bKnown = false;
		for (size_t j = 0; j < Order.size(); j++) {
			if (Order[j] == Key) { bKnown = true; break; }
		}
		if (!bKnown)
			Order.push_back(Key);
	}

	m_Order = Order;
}

bool CDcsConfig::Has(const std::wstring& Key) const
{
	return m_Values.Has(ToUtf8(Key).c_str());
}

std::wstring CDcsConfig::GetStr(const std::wstring& Key, const std::wstring& Default) const
{
	std::string Name = ToUtf8(Key);
	if (!m_Values.Has(Name.c_str()))
		return Default;
	return m_Values.Get(Name.c_str()).AsStr();
}

int CDcsConfig::GetInt(const std::wstring& Key, int Default) const
{
	std::wstring Value = GetStr(Key);
	if (Value.empty())
		return Default;

	wchar_t* pEnd = NULL;
	long iValue = wcstol(Value.c_str(), &pEnd, 10);
	if (pEnd == Value.c_str())
		return Default;	// not a number; leave the caller its default
	return (int)iValue;
}

void CDcsConfig::SetStr(const std::wstring& Key, const std::wstring& Value)
{
	m_Values.Insert(ToUtf8(Key).c_str(), StVariant(Value.c_str(), Value.length()));
	Touch(Key);
}

void CDcsConfig::SetInt(const std::wstring& Key, int Value)
{
	SetStr(Key, std::to_wstring(Value));
}

void CDcsConfig::Remove(const std::wstring& Key)
{
	m_Values.Remove(ToUtf8(Key).c_str());

	for (size_t i = 0; i < m_Order.size(); i++) {
		if (m_Order[i] == Key) {
			m_Order.erase(m_Order.begin() + i);
			break;
		}
	}
}

CDcsConfig CDcsConfig::MakeDefault()
{
	//
	// What a fresh install starts from. Deliberately short: DCS fills in its own
	// defaults for anything absent, and a configuration file that states every
	// default is one that silently freezes them at whatever this build thought
	// they were.
	//
	CDcsConfig Config;
	Config.SetStr(DCS_KEY_DCS_MODULE, L"MajorPrivacy");
	Config.SetInt(DCS_KEY_KEYBOARD_LAYOUT, 0);
	Config.SetInt(DCS_KEY_VERBOSE_DEBUG, 0);
	return Config;
}
