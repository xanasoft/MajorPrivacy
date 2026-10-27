#pragma once

#include "resource.h"

#define ERR_OK				0
#define ERR_UNKNOWN_TYPE	1
#define ERR_FILE_NOT_OPENED	2
#define ERR_UNKNOWN_CIPHER	3
#define ERR_WRONG_PASSWORD	4
#define ERR_KEY_REQUIRED	5
#define ERR_PRIVILEGE		6
#define ERR_INTERNAL		7
#define ERR_FILE_MAPPING	8
#define ERR_CREATE_EVENT	9
#define ERR_IMDISK_FAILED	10
#define ERR_IMDISK_TIMEOUT	11
#define ERR_UNKNOWN_COMMAND	12
#define ERR_MALLOC_ERROR	13
#define ERR_INVALID_PARAM	14
#define ERR_DATA_TO_LONG	15
#define ERR_DATA_NOT_FOUND	16
#define ERR_IO_FAILED		17
#define ERR_BUFFER_TO_SMALL	18

#define DC_MAX_PASSWORD 128

#pragma pack (push, 1)

//
// SPassword::slot - which credential this password is.
//
//   0    the header password
//   > 0  that key slot, 1 based
//   < 0  try the header password first, then every key slot up to abs(n)
//
// SPASS_SLOT_ANY is -KEY_SLOT_MAX, spelled out here so that a caller does not
// have to reach into DiskCryptor's volume_header.h for one constant. CryptoIO
// asserts the two agree.
//
#define SPASS_SLOT_HEADER	0
#define SPASS_SLOT_ANY		(-100)

//
// SPassword::kdf - which key derivation to run.
//
//   0..10  the KDF to use: 0 is PBKDF2, 1-10 are Argon2id cost levels
//   -1     none: pass is already the derived key, and size must be 256
//   -2     try PBKDF2 then the default Argon2id
//   -3     try every KDF, which is very slow
//
// SPASS_KDF_RAW is how the header editor avoids paying for Argon2 a second
// time when it applies a change - the same trick DiskCryptor's GUI uses when
// it hands an update to its driver. It is KDF_NONE, spelled out here so a
// caller does not need DiskCryptor's volume_header.h for one constant.
//
#define SPASS_KDF_RAW		(-1)
#define SPASS_KDF_DEFAULT	(-2)
#define SPASS_KDF_ALL		(-3)
#define SPASS_RAW_KEY_SIZE	256

struct SPassword // CAUTION: must be binary compatible with _dc_pass
{
	int   size;
	WCHAR pass[DC_MAX_PASSWORD];
	int   kdf;
	int   slot;
};

struct SCryptInfo 
{
	unsigned char	cipher_id;
	unsigned short	version;
	unsigned int	head_len;
	int				slot_count;
	int				head_kdf;
	unsigned int    disk_id;
	int				used_slot;	// 0 - opened with the header password, > 0 - 1 based key slot index
};

#define IMBOX_SLOT_NAME_LEN	20

//
// How many key slots a new container gets, and the most it may have. Spelled
// out here so a caller does not need DiskCryptor's volume_header.h for two
// numbers; CryptoIO asserts that they still agree with it.
//
#define IMBOX_SLOTS_DEFAULT	4
#define IMBOX_SLOTS_MAX		100

struct SSlotInfo // one entry per key slot, reported after a successful mount
{
	unsigned long	flags;					// SF_* from volume_header.h
	unsigned short	type;					// DC_SLOT_TYPE_*
	unsigned char	kdf;					// key derivation function of this slot
	char			name[IMBOX_SLOT_NAME_LEN];	// UTF-8, zero padded, not necessarily NUL terminated
};

struct SSection
{
	union {
		struct {
			SPassword pw;
		} in;
		struct {
			WCHAR mount[MAX_PATH + 1]; // 0 terminated
			WCHAR fs[8 + 1];
			SCryptInfo info;
		} out;
		UCHAR _1k[1024];
	};
	union {
		struct {
			USHORT id;
			USHORT size;
			BYTE data[0];
		};
		BYTE _3k[3072];
	};
};

static_assert(sizeof(SSection) == 0x1000, "Invalid SSection size"); // 4k

//
// Bulk header transport.
//
// A volume header is 64 KiB by default and SSection's data area is 3 KiB, so
// the header cannot travel in it. Rather than a second handle to inherit and
// clean up, the one section is simply made longer: SSection keeps its fixed
// 4 KiB - so the password going in and the SCryptInfo coming out do not move -
// and the header sits behind it. The mapping length is passed to ImBox on the
// command line as sec_size=, because a mapped view does not know its own size.
//
// get_header fills this in with the *plaintext* header. set_header expects it
// to hold a header sealed under the volume's current header key: that is what
// authenticates the update, exactly as it does between DiskCryptor's GUI and
// its driver. ImBox unseals it with the key it derived itself and refuses
// anything that does not come out as a valid header.
//
struct SHeaderData
{
	ULONG	size;		// bytes present in data
	ULONG	flags;		// HF_* update flags, set_header only
	ULONG	head_len;	// the volume's header length
	ULONG	reserved;
	BYTE	data[0];
};

#define SECTION_HEADER_DATA(pSec)		((struct SHeaderData*)(((BYTE*)(pSec)) + sizeof(struct SSection)))
#define SECTION_SIZE_FOR_HEADER(_len)	(sizeof(struct SSection) + sizeof(struct SHeaderData) + (_len))

#pragma pack (pop)

#define SECTION_PARAM_ID_NEW_PASS		0x0001
#define SECTION_PARAM_ID_SLOTS			0x0002 // SSlotInfo[], one per key slot of the mounted volume
#define SECTION_PARAM_ID_HEADER			0x0003 // SHeaderData follows the SSection, see above