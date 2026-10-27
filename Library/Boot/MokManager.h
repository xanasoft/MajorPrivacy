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
// Machine Owner Keys - telling shim which loaders this machine's owner trusts.
//
// Secure Boot only runs binaries signed by a key in the firmware's db, and
// nobody outside Microsoft's signing programme is in there. Shim is the way
// round that: it is itself signed by Microsoft, and it will run anything signed
// by a key in its own list - the MOK list - which the owner of the machine
// controls.
//
// Nothing here changes that list. Shim's list lives in a boot-services-only
// variable Windows cannot write, so what these functions do is leave a *request*
// in a variable shim reads at the next boot: MokNew holds what to enrol, MokAuth
// holds a hash of it and of a password, and MokManager asks for that password on
// screen before doing anything. That is the whole security model, and it is why
// every enrolment here ends with "reboot and confirm" rather than "done".
//
// Ported from DiskCryptor's dcapi/mok_sup.c and the parts of dccon/mok_menu.c
// that are operations rather than console output.
//

#include "../lib_global.h"
#include "../Status.h"
#include "../Crypto/SecurePassword.h"

#include <string>
#include <vector>

//
// Which list. Shim keeps an allow list and a deny list, and every operation
// exists for both - the variables differ only by an X in the name.
//
enum EMokList
{
	eMokAllow = 0,		// MokNew / MokDel / MokListRT
	eMokDeny,			// MokXNew / MokXDel / MokListXRT
};

struct LIBRARY_EXPORT SMokEntry
{
	bool			bIsCert = false;	// an X.509 certificate rather than a hash
	std::wstring	TypeName;			// "X.509", "SHA-256", ...
	std::wstring	Subject;			// the certificate's subject, when it could be read
	std::vector<unsigned char>	Data;	// the DER certificate or the raw hash

	std::wstring	ToHex() const;		// for a hash entry, something to show
};

struct LIBRARY_EXPORT SSecureBootState
{
	bool	bUefi = false;
	bool	bSecureBoot = false;
	bool	bSetupMode = false;

	//
	// Whether shim has ever run on this machine. MokListRT only exists because
	// shim created it, so its presence is the test - and if it is absent, none
	// of the other MOK state means anything yet.
	//
	bool	bShimPresent = false;

	int		iShimValidation = -1;	// 1 disabled, 0 enabled, -1 never set
	int		iDbIgnored = -1;		// 1 firmware db ignored, 0 used, -1 never set

	bool	bPasswordSet = false;

	// requests waiting for the next boot
	bool	bPendingEnroll = false;
	bool	bPendingDelete = false;
	bool	bPendingReset = false;
	bool	bPendingEnrollDeny = false;
	bool	bPendingDeleteDeny = false;

	int		iEnrolledCount = 0;		// entries in MokListRT
	int		iDeniedCount = 0;		// entries in MokListXRT
};

//
// What a certificate says about itself, parsed out of the DER so that the
// window showing it does not have to know any CryptoAPI. Dates are formatted
// here, as UTC, rather than handed over as Windows types.
//
struct LIBRARY_EXPORT SMokCertInfo
{
	std::wstring	Subject;			// full X.500 name, one RDN per line
	std::wstring	SubjectName;		// the short display name
	std::wstring	Issuer;
	std::wstring	IssuerName;
	std::wstring	Serial;				// hex, most significant byte first
	std::wstring	NotBefore;			// "2026-09-20 09:55:08 UTC"
	std::wstring	NotAfter;
	std::wstring	SignatureAlgorithm;
	std::wstring	PublicKeyAlgorithm;
	int				iPublicKeyBits = 0;
	int				iVersion = 0;
	std::wstring	Sha1;				// thumbprints, hex
	std::wstring	Sha256;
	bool			bSelfSigned = false;
};

class LIBRARY_EXPORT CMokManager
{
public:
	static SSecureBootState		GetState();

	//
	// What is enrolled now, what the firmware itself trusts, and what it
	// refuses. The last two are the firmware's own db and dbx, which shim
	// consults alongside its list.
	//
	static std::vector<SMokEntry>	List(EMokList List = eMokAllow);
	static std::vector<SMokEntry>	ListFirmwareDb();
	static std::vector<SMokEntry>	ListFirmwareDbx();

	//
	// The keys the firmware's own trust chain is rooted in. PK is the single
	// platform key that owns the machine; the KEKs are what it delegates to,
	// and are what may sign an update to db or dbx. Both are read only here -
	// they are changed in the firmware setup.
	//
	static std::vector<SMokEntry>	ListFirmwarePk();
	static std::vector<SMokEntry>	ListFirmwareKek();

	/* --------------------------------------------------------------------- */

	//
	// Queue an enrolment. The password is the one MokManager will ask for at
	// the next boot - it is not stored anywhere, only hashed into the request.
	//
	static STATUS	EnrollCertFiles(const std::vector<std::wstring>& Files, const CSecurePassword& Password, EMokList List = eMokAllow);

	//
	// The same enrollment from certificates already in memory, for a certificate
	// that lives inside the shipped package rather than on disk. EnrollCertFiles
	// is this with LoadCertFile in front of it.
	//
	static STATUS	EnrollCerts(const std::vector<std::vector<unsigned char>>& Certs, const CSecurePassword& Password, EMokList List = eMokAllow);
	static STATUS	EnrollCert(const std::vector<unsigned char>& Cert, const CSecurePassword& Password, EMokList List = eMokAllow);
	static STATUS	EnrollHash(const std::vector<unsigned char>& Hash, const CSecurePassword& Password, EMokList List = eMokAllow);

	static STATUS	DeleteCertFiles(const std::vector<std::wstring>& Files, const CSecurePassword& Password, EMokList List = eMokAllow);
	static STATUS	DeleteCert(const std::vector<unsigned char>& Cert, const CSecurePassword& Password, EMokList List = eMokAllow);
	static STATUS	DeleteHash(const std::vector<unsigned char>& Hash, const CSecurePassword& Password, EMokList List = eMokAllow);

	// Take back a request that has not been confirmed yet.
	static STATUS	RevokeEnroll(EMokList List = eMokAllow);
	static STATUS	RevokeDelete(EMokList List = eMokAllow);

	//
	// Queue a reset, which empties the list entirely. Expressed as an auth with
	// nothing to enrol, which is how MokManager tells the two apart.
	//
	static STATUS	Reset(const CSecurePassword& Password, EMokList List = eMokAllow);

	static bool		IsCertEnrolled(const std::vector<unsigned char>& Cert);

	/* --------------------------------------------------------------------- */

	// The password MokManager asks for when it is entered from its own menu
	// rather than against a queued request.
	static STATUS	SetPassword(const CSecurePassword& Password);
	static STATUS	ClearPassword();

	//
	// Turn shim's own signature checking off, or tell it to ignore the
	// firmware's db. Both take effect only after being confirmed at the next
	// boot, and both weaken Secure Boot - the window says so.
	//
	static STATUS	SetShimValidation(bool bEnable, const CSecurePassword& Password);
	static STATUS	SetDbUsage(bool bUseDb, const CSecurePassword& Password);

	// How long MokManager waits at its menu. -1 waits forever.
	static STATUS	SetTimeout(int iSeconds);
	static STATUS	SetVerbose(bool bVerbose);

	// Write every entry of a list out as .der / .hash files.
	static RESULT(int)	Export(const std::wstring& Directory, EMokList List = eMokAllow);

	/* --------------------------------------------------------------------- */

	// Read a certificate from a file. DER as it stands, PEM decoded.
	static RESULT(std::vector<unsigned char>)	LoadCertFile(const std::wstring& Path);

	// Everything a certificate says about itself, for the details window.
	static RESULT(SMokCertInfo)	GetCertInfo(const std::vector<unsigned char>& Cert);

	// Parse an EFI_SIGNATURE_LIST blob, wherever it came from.
	static std::vector<SMokEntry>	ParseSigList(const std::vector<unsigned char>& Data);

	//
	// Secure Boot db trust: whether the firmware would run a PE as it is signed, the
	// way the firmware itself decides it - the file's Authenticode signer must chain
	// up to a certificate enrolled in db. ListDbCertThumbprints returns the SHA-1
	// thumbprints of every X.509 certificate in db; IsFileTrustedByDb builds the
	// signer's certificate chain and returns true if any certificate in it (the signer
	// leaf for a self-rolled db, or an intermediate CA for a Microsoft-signed shim) is
	// one of them. (In the spirit of DiskCryptor dcapi/sb_sup.c, extended from a
	// leaf-only match to the full chain so a CA-signed binary is recognised too.)
	//
	static std::vector<std::vector<unsigned char>>	ListDbCertThumbprints();
	static bool						IsFileTrustedByDb(const std::wstring& Path,
											const std::vector<std::vector<unsigned char>>& DbThumbprints);

	//
	// The same question asked of an image already in memory, so that a package
	// file can be judged without being written out somewhere first. The embedded
	// signature is read straight from the blob - CryptQueryObject does that for a
	// PE in memory exactly as it does for one on disk - and IsFileTrustedByDb is
	// just this with a read in front of it.
	//
	static bool						IsDataTrustedByDb(const std::vector<unsigned char>& Image,
											const std::vector<std::vector<unsigned char>>& DbThumbprints);

protected:
	static std::vector<unsigned char>	BuildCertSigList(const std::vector<unsigned char>& Cert);
	static std::vector<unsigned char>	BuildHashSigList(const std::vector<unsigned char>& Hash);
	static std::vector<unsigned char>	BuildAuth(const std::vector<unsigned char>& Data, const CSecurePassword& Password);

	static STATUS	SetRequest(const std::wstring& DataVar, const std::wstring& AuthVar,
							   const std::vector<unsigned char>& SigList, const CSecurePassword& Password);
	static STATUS	SetToggle(const std::wstring& Name, unsigned long uState, const CSecurePassword& Password);
};
