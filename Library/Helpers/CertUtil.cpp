#include "pch.h"
#include "CertUtil.h"
#include <windows.h>
#include <wincrypt.h>
#include <softpub.h>
#include <mscat.h>
#include <wintrust.h>
#include "../Common/Strings.h"

static SCICertificatePtr MakeCertificateInfo(PCCERT_CONTEXT certContext)
{
	if (!certContext) 
		return nullptr;

	SCICertificatePtr pSignerCert = std::make_shared<SCICertificate>();

	wchar_t subjectName[256] = {};
	CertGetNameStringW(
		certContext,
		CERT_NAME_SIMPLE_DISPLAY_TYPE,
		0,
		NULL,
		subjectName,
		sizeof(subjectName));
	//wchar_t issuerName[256] = {};
	//CertGetNameStringW(
	//	certContext,
	//	CERT_NAME_SIMPLE_DISPLAY_TYPE,
	//	CERT_NAME_ISSUER_FLAG,
	//	NULL,
	//	issuerName,
	//	sizeof(issuerName));
	pSignerCert->Subject = subjectName;

	// Get SHA-1 fingerprint
	pSignerCert->SHA1 = std::vector<BYTE>(20);
	DWORD sha1HashSize = sizeof(pSignerCert->SHA1);
	if (CertGetCertificateContextProperty(certContext, CERT_SHA1_HASH_PROP_ID, pSignerCert->SHA1.data(), &sha1HashSize))
		pSignerCert->SHA1.resize(sha1HashSize);
	else
		pSignerCert->SHA1.clear();

	// Get SHA-256 fingerprint
	pSignerCert->SHA256 = std::vector<BYTE>(32);
	DWORD sha256HashSize = sizeof(pSignerCert->SHA256);
	if (CertGetCertificateContextProperty(certContext, CERT_SHA256_HASH_PROP_ID, pSignerCert->SHA256.data(), &sha256HashSize))
		pSignerCert->SHA256.resize(sha256HashSize);
	else
		pSignerCert->SHA256.clear();

	// Compute SHA-384 fingerprint
	pSignerCert->SHA384 = std::vector<BYTE>(48);
	DWORD sha384HashSize = sizeof(pSignerCert->SHA384);
	if (CryptHashCertificate(NULL, CALG_SHA_384, 0, certContext->pbCertEncoded, certContext->cbCertEncoded, pSignerCert->SHA384.data(), &sha384HashSize))
		pSignerCert->SHA384.resize(sha384HashSize);
	else
		pSignerCert->SHA384.clear();

	// Compute SHA-512 fingerprint
	pSignerCert->SHA512 = std::vector<BYTE>(64);
	DWORD sha512HashSize = sizeof(pSignerCert->SHA512);
	if (CryptHashCertificate(NULL, CALG_SHA_512, 0, certContext->pbCertEncoded, certContext->cbCertEncoded, pSignerCert->SHA512.data(), &sha512HashSize))
		pSignerCert->SHA512.resize(sha512HashSize);
	else
		pSignerCert->SHA512.clear();

	return pSignerCert;
}

bool IsEmbeddedSignatureValid(const std::wstring& filePath, SCICertificatePtr* ppSigner = NULL) 
{
	// Use the built-in GUID from Softpub.h:
	GUID wvtPolicyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;

	WINTRUST_FILE_INFO fileInfo = {};
	fileInfo.cbStruct = sizeof(fileInfo);
	fileInfo.pcwszFilePath = filePath.c_str();

	WINTRUST_DATA wtd = {};
	wtd.cbStruct = sizeof(wtd);
	wtd.dwUnionChoice = WTD_CHOICE_FILE;
	wtd.pFile = &fileInfo;
	wtd.dwUIChoice = WTD_UI_NONE;
	wtd.fdwRevocationChecks = WTD_REVOKE_NONE;  
	wtd.dwProvFlags = WTD_SAFER_FLAG;
	if(ppSigner)
		wtd.dwStateAction = WTD_STATEACTION_VERIFY;

	LONG lStatus = WinVerifyTrust(
		(HWND)INVALID_HANDLE_VALUE,
		&wvtPolicyGUID,
		&wtd
	);

	if (ppSigner)
	{
		if (lStatus == ERROR_SUCCESS) 
		{
			// Retrieve the signer information
			CRYPT_PROVIDER_DATA* pProvData = WTHelperProvDataFromStateData(wtd.hWVTStateData);
			if (pProvData) {
				CRYPT_PROVIDER_SGNR* pProvSigner = WTHelperGetProvSignerFromChain(pProvData, 0, FALSE, 0);
				if (pProvSigner)
					*ppSigner = MakeCertificateInfo(pProvSigner->pasCertChain[0].pCert);
			}
		}

		// Clean up
		wtd.dwStateAction = WTD_STATEACTION_CLOSE;
		WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &wvtPolicyGUID, &wtd);
	}

	return (lStatus == ERROR_SUCCESS);
}

SCICertificatePtr RetrieveCertificateChain(PCCERT_CONTEXT certContext, HCERTSTORE hAdditionalStore = NULL)
{
	if (!certContext)
		return nullptr;

	CERT_CHAIN_PARA chainPara = {};
	chainPara.cbSize = sizeof(chainPara);

	PCCERT_CHAIN_CONTEXT chainContext = NULL;
	BOOL result = CertGetCertificateChain(
		/* hChainEngine        */ NULL,
		/* pCertContext        */ certContext,
		/* pTime               */ NULL,
		/* hAdditionalStore    */ hAdditionalStore,
		/* pChainPara          */ &chainPara,
		/* dwFlags             */ CERT_CHAIN_CACHE_END_CERT,
		/* pvReserved          */ NULL,
		/* ppChainContext      */ &chainContext);

	if (!result || !chainContext)
		return nullptr;

	SCICertificatePtr pSignerCert;
	SCICertificatePtr* ppNextCert = &pSignerCert;

	// Complete Certificate Chain (Leaf -> Root)
	if (chainContext->cChain > 0) {
		PCERT_SIMPLE_CHAIN simpleChain = chainContext->rgpChain[0];
		for (DWORD i = 0; i < simpleChain->cElement; i++) {
			PCCERT_CONTEXT chainCert = simpleChain->rgpElement[i]->pCertContext;
			*ppNextCert = MakeCertificateInfo(chainCert);
			if(!*ppNextCert)
				break;
			ppNextCert = &(*ppNextCert)->Issuer;
		}
	}

	CertFreeCertificateChain(chainContext);

	return pSignerCert;
}

SEmbeddedCIInfoPtr GetEmbeddedCIInfo(const std::wstring& filePath)
{
	HCERTSTORE hStore = NULL;
	HCRYPTMSG hMsg = NULL;	

	BOOL res = CryptQueryObject(
		CERT_QUERY_OBJECT_FILE,
		filePath.c_str(),
		CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
		CERT_QUERY_FORMAT_FLAG_BINARY,
		0,
		NULL,
		NULL,
		NULL,
		&hStore,
		&hMsg,
		NULL);

	if (!res || !hMsg || !hStore) {
		if (hStore)  CertCloseStore(hStore, 0);
		if (hMsg)    CryptMsgClose(hMsg);
		return nullptr;
	}

	DWORD signerCount = 0;
	DWORD dataSize = sizeof(signerCount);
	if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_COUNT_PARAM, 0, &signerCount, &dataSize)) {
		CertCloseStore(hStore, 0);
		CryptMsgClose(hMsg);
		return nullptr;
	}

	SEmbeddedCIInfoPtr pInfo = std::make_shared<SEmbeddedCIInfo>();

	pInfo->EmbeddedSignatureValid = IsEmbeddedSignatureValid(filePath);

	for (DWORD index = 0; index < signerCount; ++index) { // Retrieve the signer info from the message for each signer

		DWORD signerInfoSize = 0;
		if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, index, NULL, &signerInfoSize))
			continue;

		PCMSG_SIGNER_INFO signerInfo = (PCMSG_SIGNER_INFO)malloc(signerInfoSize);
		if (!signerInfo)
			continue;

		if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, index, signerInfo, &signerInfoSize)) {
			free(signerInfo);
			continue;
		}

		// Locate the signing certificate in the store by Issuer+SerialNumber
		CERT_INFO certInfo = {};
		certInfo.Issuer = signerInfo->Issuer;
		certInfo.SerialNumber = signerInfo->SerialNumber;

		PCCERT_CONTEXT certContext = CertFindCertificateInStore(
			hStore,
			X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
			0,
			CERT_FIND_SUBJECT_CERT,
			&certInfo,
			NULL);

		if (certContext)
		{
			SCICertificatePtr pSignerCert = RetrieveCertificateChain(certContext, hStore);
			if (!pSignerCert)
				pSignerCert = MakeCertificateInfo(certContext);
			if (pSignerCert) // fallback
				pInfo->EmbeddedSigners.push_back(pSignerCert);

			CertFreeCertificateContext(certContext);
		}

		free(signerInfo);
	}

	// Cleanup
	CertCloseStore(hStore, 0);
	CryptMsgClose(hMsg);

	return pInfo;
}

bool EnumerateCatalogSigners(const std::wstring& catalogFilePath, std::vector<SCICertificatePtr>& List) 
{
	// Typically, a catalog is a PKCS7-signed file
	HCERTSTORE hCatMsgStore = NULL;
	HCRYPTMSG hMsg = NULL;
	if (!CryptQueryObject(
		CERT_QUERY_OBJECT_FILE,
		catalogFilePath.c_str(),
		CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED,
		CERT_QUERY_FORMAT_FLAG_BINARY,
		0,
		NULL, // pdwMsgAndCertEncodingType
		NULL, // pdwContentType
		NULL, // pdwFormatType
		&hCatMsgStore,
		&hMsg,
		NULL))
	{
		// If not found or not signed, just return
		return false;
	}

	// Check how many signers
	DWORD signerCount = 0;
	DWORD dataSize = sizeof(signerCount);
	if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_COUNT_PARAM, 0, &signerCount, &dataSize)) {
		// no signers or error
		if (hCatMsgStore) CertCloseStore(hCatMsgStore, 0);
		if (hMsg)         CryptMsgClose(hMsg);
		return false;
	}

	// For each signer, locate its cert in hCatMsgStore
	for (DWORD i = 0; i < signerCount; i++) {
		DWORD signerInfoSize = 0;
		if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, i, NULL, &signerInfoSize)) {
			continue;
		}
		PCMSG_SIGNER_INFO signerInfo = (PCMSG_SIGNER_INFO)malloc(signerInfoSize);
		if (!signerInfo) {
			continue;
		}
		if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, i, signerInfo, &signerInfoSize)) {
			free(signerInfo);
			continue;
		}

		CERT_INFO certInfo = {};
		certInfo.Issuer       = signerInfo->Issuer;
		certInfo.SerialNumber = signerInfo->SerialNumber;

		PCCERT_CONTEXT leafCatCert = CertFindCertificateInStore(
			hCatMsgStore,
			X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
			0,
			CERT_FIND_SUBJECT_CERT,
			&certInfo,
			NULL);

		if (leafCatCert) 
		{
			SCICertificatePtr pSignerCert = RetrieveCertificateChain(leafCatCert, hCatMsgStore);
			if (!pSignerCert)
				pSignerCert = MakeCertificateInfo(leafCatCert);
			if (pSignerCert) // fallback
				List.push_back(pSignerCert);

			CertFreeCertificateContext(leafCatCert);
		}
		free(signerInfo);
	}

	if (hCatMsgStore) CertCloseStore(hCatMsgStore, 0);
	if (hMsg)         CryptMsgClose(hMsg);

	return List.size() > 0;
}

SCatalogCIInfoPtr GetCatalogCIInfo(const std::wstring& filePath)
{
	HCATADMIN hCatAdmin = NULL;
	if (!CryptCATAdminAcquireContext(&hCatAdmin, NULL, 0))
		return nullptr;

	// Open the file to compute its hash
	HANDLE hFile = CreateFileW(
		filePath.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (hFile == INVALID_HANDLE_VALUE) {
		CryptCATAdminReleaseContext(hCatAdmin, 0);
		return nullptr;
	}

	// Compute the file hash
	BYTE hash[64] = {};
	DWORD hashSize = sizeof(hash);
	if (!CryptCATAdminCalcHashFromFileHandle(hFile, &hashSize, hash, 0)) {
		CloseHandle(hFile);
		CryptCATAdminReleaseContext(hCatAdmin, 0);
		return nullptr;
	}

	CloseHandle(hFile);

	// Enumerate all catalogs that match the file hash
	HCATINFO hCatInfo = NULL;
	hCatInfo = CryptCATAdminEnumCatalogFromHash(hCatAdmin, hash, hashSize, 0, NULL);

	if (!hCatInfo) {
		CryptCATAdminReleaseContext(hCatAdmin, 0);
		return nullptr;
	}

	SCatalogCIInfoPtr pInfo = std::make_shared<SCatalogCIInfo>();

	// If there's at least one matching catalog, we loop
	while (hCatInfo) {
		// For each matching catalog, retrieve info
		CATALOG_INFO catInfo = {};
		catInfo.cbStruct = sizeof(CATALOG_INFO);

		if (CryptCATCatalogInfoFromContext(hCatInfo, &catInfo, 0)) 
		{
			if (pInfo->CatalogSigners.find(catInfo.wszCatalogFile) == pInfo->CatalogSigners.end())
			{
				// 1) Enumerate signers for this catalog
				if (!EnumerateCatalogSigners(catInfo.wszCatalogFile, pInfo->CatalogSigners[catInfo.wszCatalogFile]))
					pInfo->CatalogSigners.erase(catInfo.wszCatalogFile);

				// 2) Open the catalog as a Cert Store and enumerate all certificates
				//HCERTSTORE hStore = CertOpenStore(
				//	CERT_STORE_PROV_FILENAME,
				//	X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
				//	NULL,
				//	0,
				//	catInfo.wszCatalogFile);

				//if (hStore) {
				//	std::cout << "\n  [*] Enumerating certificates in the catalog store:\n\n";
				//	PCCERT_CONTEXT enumCertContext = NULL;
				//	while ((enumCertContext = CertEnumCertificatesInStore(hStore, enumCertContext)) != NULL) {
				//		PrintCertificateDetails(enumCertContext);
				//		std::cout << "  [*] Building chain for above certificate...\n";
				//		RetrieveCertificateChain(enumCertContext, hStore);
				//	}
				//	CertCloseStore(hStore, 0);
				//} else {
				//	std::cerr << "  [!] Failed to open catalog file as a certificate store.\n";
				//}
			}
		}

		// Move to the next catalog in the chain
		HCATINFO hNextCatInfo = CryptCATAdminEnumCatalogFromHash(hCatAdmin, hash, hashSize, 0, &hCatInfo);

		// Release the current catalog context before going to next
		CryptCATAdminReleaseCatalogContext(hCatAdmin, hCatInfo, 0);
		hCatInfo = hNextCatInfo;
	}

	// Finally, release the CatAdmin context
	CryptCATAdminReleaseContext(hCatAdmin, 0);

	return pInfo;
}

// todo: add check if file changed!!!!!

/*std::shared_mutex g_EmbeddedCertCacheMutex;
std::map<std::wstring, SEmbeddedCIInfoPtr> g_EmbeddedCertCache;

SEmbeddedCIInfoPtr GetEmbeddedCIInfo(const std::wstring& filePath)
{
	std::shared_lock lock(g_EmbeddedCertCacheMutex);
	auto F = g_EmbeddedCertCache.find(MkLower(filePath));
	if (F != g_EmbeddedCertCache.end())
		return F->second;
	lock.unlock();

	std::unique_lock ulock(g_EmbeddedCertCacheMutex);
	return g_EmbeddedCertCache[MkLower(filePath)] = GetEmbeddedCIInfoImpl(filePath);
}

std::shared_mutex g_CatalogCertCacheMutex;
std::map<std::wstring, SCatalogCIInfoPtr> g_CatalogCertCache;

SCatalogCIInfoPtr GetCatalogCIInfo(const std::wstring& filePath)
{
	std::shared_lock lock(g_CatalogCertCacheMutex);
	auto F = g_CatalogCertCache.find(MkLower(filePath));
	if (F != g_CatalogCertCache.end())
		return F->second;
	lock.unlock();

	std::unique_lock ulock(g_CatalogCertCacheMutex);
	return g_CatalogCertCache[MkLower(filePath)] = GetCatalogCIInfoImpl(filePath);
}*/

bool GetEmbeddedSignatureStatus(const std::wstring& filePath, bool* pValid)
{
	HCERTSTORE hStore = NULL;
	HCRYPTMSG hMsg = NULL;	

	BOOL res = CryptQueryObject(
		CERT_QUERY_OBJECT_FILE,
		filePath.c_str(),
		CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
		CERT_QUERY_FORMAT_FLAG_BINARY,
		0,
		NULL,
		NULL,
		NULL,
		&hStore,
		&hMsg,
		NULL);

	if (!res || !hMsg || !hStore) {
		if (hStore)  CertCloseStore(hStore, 0);
		if (hMsg)    CryptMsgClose(hMsg);
		return false;
	}

	DWORD signerCount = 0;
	DWORD dataSize = sizeof(signerCount);
	if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_COUNT_PARAM, 0, &signerCount, &dataSize)) {
		CertCloseStore(hStore, 0);
		CryptMsgClose(hMsg);
		return false;
	}

	*pValid = IsEmbeddedSignatureValid(filePath);

	// Cleanup
	CertCloseStore(hStore, 0);
	CryptMsgClose(hMsg);

	return true;
}

#include "../PHlib/include/ph.h"

std::wstring PHStr2WStr(PPH_STRING phStr, bool bFree = false);

SCICertificatePtr GetCIInfo(const std::wstring& filePath)
{
	PPH_STRING phFilePath = PhCreateStringEx((WCHAR*)filePath.c_str(), filePath.length() * sizeof(WCHAR));
	PPH_STRING phSigner;

	BYTE SignerSHA1Hash[20];
	DWORD SignerSHA1HashSize = sizeof(SignerSHA1Hash);
	BYTE SignerSHAXHash[64];
	DWORD SignerSHAXHashSize = sizeof(SignerSHAXHash);
	VERIFY_RESULT result = PhVerifyFileCached(phFilePath, NULL, &phSigner, SignerSHA1Hash, &SignerSHA1HashSize, SignerSHAXHash, &SignerSHAXHashSize, FALSE, FALSE);
	PhDereferenceObject(phFilePath);
	std::wstring Signer = PHStr2WStr(phSigner, TRUE);

	if (result != VrTrusted)
		return nullptr;

	SCICertificatePtr pInfo = std::make_shared<SCICertificate>();
	pInfo->Subject = Signer;
	pInfo->SHA1 = std::vector<BYTE>(SignerSHA1Hash, SignerSHA1Hash + SignerSHA1HashSize);
	pInfo->SHA256 = std::vector<BYTE>(SignerSHAXHash, SignerSHAXHash + SignerSHAXHashSize);
	return pInfo;
}