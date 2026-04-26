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

#define DC_MAX_PASSWORD 128

#pragma pack (push, 1)

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

#pragma pack (pop)

#define SECTION_PARAM_ID_NEW_PASS		0x0001