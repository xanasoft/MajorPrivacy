#ifndef _VOLUME_HEADER_H_
#define _VOLUME_HEADER_H_

#define DC_VOLUME_SIGN    0x50524344        // volume header signature, text value 'DCRP'

#define HEADER_SALT_SIZE  64                // salt size in header = 512 bits
#define MAX_KEY_SIZE      (32*3)            // maximum actual key size for cascade cipher = 768 bits
#define PKCS_DERIVE_MAX   (MAX_KEY_SIZE*2)  // maximum key size into which password is expanded = 192 bytes
#define DISKKEY_SIZE	  256               // number of key bytes stored in header (taken with reserve)

#define SECTOR_SIZE       512
#define MAX_SECTOR_SIZE   4096
#define CD_SECTOR_SIZE    2048

#define MIN_PASSWORD      1	                // Minimum password length
#define MAX_PASSWORD      128               // Maximum password length

#define KEY_SLOT_COUNT    4                 // default number of key slots in v2 header
#define KEY_SLOT_MAX      100               // a reasonable maximum number of key slots, hard limit is 255
#define ALL_KEY_SLOTS     (-KEY_SLOT_MAX)

//                        1                 // 0.5 - 0.9 header version
#define DC_HDR_VERSION    2					// 1.0 - 1.4.1 header version
#define DC_HDR_VERSION_2  3					// 2.0 + header version

#define FF_KEY_SLOTS      0x01              // header supports key slots V1

#define VF_NONE           0x00
#define VF_TMP_MODE       0x01              // temporary encryption mode
#define VF_REENCRYPT      0x02              // volume re-encryption in progress
#define VF_STORAGE_FILE   0x04              // redirected area are placed in file
#define VF_NO_REDIR       0x08              // redirection area is not present - iso/file container
#define VF_EXTENDED       0x10              // this volume placed on extended partition (used only by MBR bootloader)

#define VF_BACKUP_HEADER  0x20              // backup header at partition end

/* Request-only flags (not stored in header) */
#define VF_USE_SLACK      0x40              // try to use slack space after the filesystem for redirection area
#define VF_TRY_SHRINK     0x80              // try to shrink filesystem to create slack space

#pragma pack (push, 1)

typedef struct _dc_pass {
	int            size;                    // password length in bytes without terminating null
	wchar_t        pass[MAX_PASSWORD];      // password in UTF16-LE encoding
	int            kdf;                     // password cost factor, 0 for legacy PBKDF2
	int            slot;					// s = 0 header key; s > 0 - keyslot index (1-based); s < 0 try all slots and hreader up to abs(n)
	unsigned long  tag;

} dc_pass;

static_assert(sizeof(wchar_t)* MAX_PASSWORD >= DISKKEY_SIZE, "Password field in dc_pass to short");

typedef struct _dc_header {
	unsigned char  salt[HEADER_SALT_SIZE];   // salt	64 byte - 512 bits
	unsigned long  sign;                    // signature 'DCRP'
	unsigned long  hdr_crc;                 // crc32 of decrypted volume header
	unsigned short version;                 // volume format version
	unsigned long  flags;                   // volume flags
	unsigned long  disk_id;                 // unique volume identifier
	int            alg_1;                   // crypt algo 1
	unsigned char  key_1[DISKKEY_SIZE];     // crypt key 1
	int            alg_2;                   // crypt algo 2
	unsigned char  key_2[DISKKEY_SIZE];     // crypt key 2
	// 602

	union {
		unsigned __int64 stor_off;          // redirection area offset
		unsigned __int64 data_off;          // volume data offset, if redirection area is not used
	};
	union {
		unsigned char deprecated[8];        // use_size in 0.5 - 0.9
		struct {							// 2.0 +
			unsigned long stor_len;         // redirection area size in bytes
			unsigned long head_len;         // header size in bytes
		};
	};
	unsigned __int64 tmp_size;              // temporary part size - encryption/decryption position offset
	unsigned char  tmp_wp_mode;             // data wipe mode
	// 627

	unsigned char  footer_cnt;				// footer length in blocks of 16 bytes, reserved for future use
											// footer is excluded form v2 crc calculation, footer may be plaintext
	unsigned long  feature_flags;			// feature flags bitmask (FF_*) for features in v2 and header
	unsigned short ext_hdr_off;             // extended header offset in bytes from the header start, 0 if not used

	// feature_flags & FF_KEY_SLOTS
	unsigned char  head_kdf;                // Header Key Derivation Function, 0 for legacy PBKDF2, 1-10 for Argon2id
	// v2 - key slots
	         //    key_slot_off     == DC_BASE_SIZE
	unsigned short slot_area_len;           // keyslot area size (must be multiple of xts block size)
	unsigned char  key_slot_count;			// number of key slots available
             //    key_slot_size    == slot_area_len / key_slot_count
	// v2 - embedded slot info
	         //    slot_info_off    == key_slot_off + slot_area_len
	unsigned short slot_info_size;			// single keyslot descriptor size
	         //    slot_info_len    == key_slot_count * slot_info_size	
	// 640

	unsigned char  reserved[384];           // reserved, zeros for future use
	// 1024

	unsigned char  space[1024];             // reserved for key slots
	// 2048

} dc_header;

//const dc_header_test = FIELD_OFFSET(dc_header, ext_hdr_off);

#define IS_INVALID_SECTOR_SIZE(_s) ( (_s) % SECTOR_SIZE )

#define ROUND_TO_FULL_SECTORS(len, bps) ( ( ((len) + (bps) - 1) / (bps) ) * (bps) )

#define DC_BASE_SIZE         (1024)
#define DC_AREA_SIZE         (2 * 1024)
#define DC_HEAD_SPACE        (DC_AREA_SIZE - DC_BASE_SIZE)
#define DC_CRC_AREA_SIZE_1   ((DC_AREA_SIZE - HEADER_SALT_SIZE) - 8)
#define DC_CRC_AREA_SIZE_2   ((DC_BASE_SIZE - HEADER_SALT_SIZE) - 8)
#define DC_AREA_MAX_SIZE     (32 * 1024 * 1024) // hard limit is 4GB
#define DC_AREA_MAX_SIZE_UI  (4 * 1024 * 1024)



// Keyslot types and layout in the reserved space of the header

#define DC_SLOT_TYPE_0  0					// 0 - XOR Wrap

#define SF_ACTIVE         0x01              // active
#define SF_DISABLED       0x02              // disabled
#define SF_CORRUPT        0x04              // corrupt
//#define SF_TPM_SLOT       0x10              // dedicated to TPM
//#define SF_RECOVERY_SLOR  0x20              //

#define SLOT_LABEL_LEN    20

typedef struct _dc_slot_info {
	unsigned long  crc;                     // crc32 of slot ciphertext + descriptor
	unsigned long  flags;                   // slot flags (ACTIVE, etc.)
	unsigned short type;                    // slot type
	char           slot_name[SLOT_LABEL_LEN]; // slot name in UTF-8 encoding, zero padded, not required NUL terminated
	union {
		unsigned char data[2]; // or 34     // format specific data
		struct {
			unsigned char slot_kdf;			// Slot Key Derivation Function, 0 for legacy PBKDF2, 1-10 for Argon2id
		} data_0;
	};
} dc_slot_info;

//const dc_slot_infor_test = sizeof(dc_slot_info);

static_assert(sizeof(dc_slot_info) == 32, "Invalid dc_slot_info size");



// Extended header format and layout in the reserved space of the header

#define MIN_EXT_HDR_SIZE  10 // minimum extended header size to fit crc, size and version fields

#define DC_EXT_VERSION    0                 // extended header version

typedef struct _dc_ext_header {
	unsigned long  crc;                     // crc32 of extended header data (for consistency check only)
	unsigned long  size;                    // size of extended header data in bytes
	unsigned short version;                 // extended header version
	unsigned char  data[0];				    // variant
} dc_ext_header;

//const dc_ext_header_test = sizeof(dc_ext_header);

//static_assert(sizeof(dc_ext_header) == 64, "Invalid dc_ext_header size");


#pragma pack (pop)

/* Header V1
*
* 0-626:	 header data		
* 627-2047:  zeros
*
*/


/* Header V2
*
* 0-626:	 header data		
* 640-767:   zeros
* 768-1023:  optional footer
* 1024-1919: key slots and slot info (up to 4 slots with 32 byte descriptor each)
* 1920-2047: space for extended header
*/

// The default header layout with 4 key slots must fit into DC_AREA_SIZE (2 KiB).

static_assert(sizeof(dc_header) == DC_AREA_SIZE, "Invalid dc_header size");
static_assert(FIELD_OFFSET(dc_header, space) == DC_BASE_SIZE, "Invalid dc_header base layout");

// The default header layout with 4 key slots and extended header must fit into DC_AREA_SIZE (2 KiB).
// This maintains compatibility with existing v1 headers and avoids the need for redirection area expansion.
static_assert(DC_BASE_SIZE + KEY_SLOT_COUNT * (PKCS_DERIVE_MAX + sizeof(dc_slot_info)) <= DC_AREA_SIZE, "Invalid default header layout");


#define KDF_SHA512_PKCS5_2	0

#define KDF_ARGON_MIN		1
#define KDF_ARGON_DEFAULT	5
#define KDF_ARGON_MAX		10

#define KDF_NONE	-1
#define KDF_DEFAULT -2
#define KDF_ALL		-3


#endif