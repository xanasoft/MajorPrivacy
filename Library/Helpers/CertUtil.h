#pragma once
#include "../lib_global.h"
#include "../Status.h"

struct SCICertificate
{
	std::wstring Subject;
	std::vector<BYTE> SHA1;		// 20 bytes
	std::vector<BYTE> SHA256;	// 32 bytes
	std::vector<BYTE> SHA384;	// 48 bytes
	std::vector<BYTE> SHA512;	// 64 bytes
	std::shared_ptr<SCICertificate> Issuer;
};

typedef std::shared_ptr<SCICertificate> SCICertificatePtr;

struct SEmbeddedCIInfo
{
	bool EmbeddedSignatureValid = false;
	std::vector<SCICertificatePtr> EmbeddedSigners;
};

typedef std::shared_ptr<SEmbeddedCIInfo> SEmbeddedCIInfoPtr;

LIBRARY_EXPORT SEmbeddedCIInfoPtr GetEmbeddedCIInfo(const std::wstring& filePath);

struct SCatalogCIInfo
{
	std::map<std::wstring, std::vector<SCICertificatePtr>> CatalogSigners;
};

typedef std::shared_ptr<SCatalogCIInfo> SCatalogCIInfoPtr;

LIBRARY_EXPORT SCatalogCIInfoPtr GetCatalogCIInfo(const std::wstring& filePath);

LIBRARY_EXPORT SCICertificatePtr GetCIInfo(const std::wstring& filePath);