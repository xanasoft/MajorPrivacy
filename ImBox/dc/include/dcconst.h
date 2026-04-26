#ifndef _DCCONST_H_
#define _DCCONST_H_

/* hook control flags */
#define F_NONE              0x0000
#define F_ENABLED           0x0001 // device mounted
#define F_SYNC              0x0002 // synchronous IRP processing mode
#define F_SYSTEM            0x0004 // this is a system device
#define F_REMOVABLE         0x0008 // this is removable device
#define F_HIBERNATE         0x0010 // this device used for hibernation
#define F_CRASHDUMP         0x0020 // this device used for crashdumping
#define F_UNSUPRT           0x0040 // device unsupported
#define F_DISABLE           0x0080 // device temporary disabled
#define F_REENCRYPT         0x0100 // reencryption in progress
#define F_FORMATTING        0x0200 // formatting in progress
#define F_NO_AUTO_MOUNT     0x0400 // automounting disabled for this device
#define F_PROTECT_DCSYS     0x0800 // protect $dcsys$ file from any access
#define F_PREVENT_ENC       0x1000 // fail any encrypt/decrypt requests with ST_CANCEL status
#define F_CDROM             0x2000 // this is CD-ROM device
#define F_NO_REDIRECT       0x4000 // redirection area is not used
#define F_SSD               0x8000 // this is SSD disk
#define F_PENDING	    0x00010000 // this device has a pending header
#define F_FS_DETECTED   0x00020000 // valid filesystem signature detected in sector 0
#define F_HEAD_BACKUP   0x00040000 // backup header exists at partition end

#define F_CLEAR_ON_UNMOUNT ( \
	F_ENABLED | F_SYNC | F_REENCRYPT | F_FORMATTING | F_PROTECT_DCSYS | F_NO_REDIRECT | F_FS_DETECTED | F_HEAD_BACKUP )

/* unmount flags */
#define MF_FORCE            0x01 // unmount volume if FSCTL_LOCK_VOLUME fail
#define MF_NOFSCTL          0x02 // no send FSCTL_DISMOUNT_VOLUME
#define MF_NOSYNC           0x04 // no stop synchronous mode thread
#define MF_DELMP            0x08 // delete volume mount point when unmount
#define MF_NOWAIT_IO        0x10 // no wait for IO IRPs completion

/* mount flags */
#define MF_READ_ONLY        0x20 // mount volume as read only
#define MF_USE_BACKUP       0x40 // try to mount from backup header

/* header operation flags */
#define HF_DEFAULT          0x00 // default operation - write entire header
#define HF_UPDATE_BASE      0x01 // only update header base
#define HF_UPDATE_SLOTS     0x02 // update key slots only
#define HF_UPDATE_EXT       0x04 // update extended header
//#define HF_UPDATE_EXP       0x08 // update expanded header
#define HF_CLEAR_SLOTS      0x10 // clear key slots area
#define HF_HEADER_FILL      0x20 // wipe header sectors before updating
#define HF_KEEP_SALT        0x40 // keep original salt
#define HF_BACKUP_HEADER    0x80 // handle backup header at partition end instead of primary header at sector 0

#define HF_UPDATE_ALL (HF_UPDATE_BASE | HF_UPDATE_SLOTS | HF_UPDATE_EXT)

/* Update layout flags passed by Init type & 0xFFFFFF00 */
#define S_RESIZE_HEADER     0x0100 // resize header
                         // 0x0200
#define S_BACKUP_HEADER     0x0400 // create header backup at partition end
#define S_REMOVE_BACKUP     0x0800 // remove header backup from partition end
#define S_STORAGE_TO_FILE   0x1000 // move storage area to file - does not reclaim slack space
#define S_STORAGE_TO_END    0x2000 // move storage area to partition end - atempts to schrink the FS when not enough slack space is available

/* operation status codes */
#define ST_OK			    0  /* operation completed successfully */
#define ST_ERROR            1  /* unknown error    */
#define ST_NF_DEVICE        2  /* device not found */
#define ST_RW_ERR           3  /* read/write error */
#define ST_PASS_ERR         4  /* invalid password */
#define ST_ALR_MOUNT        5  /* device is already mounted */
#define ST_NO_MOUNT         6  /* device not mounted */
#define ST_LOCK_ERR         7  /* error on volume locking  */
#define ST_UNMOUNTABLE      8  /* device is unmountable */
#define ST_NOMEM            9  /* not enough memory */
#define ST_ERR_THREAD       10 /* error on creating system thread */
#define ST_INV_WIPE_MODE    11 /* invalid data wipe mode */
#define ST_INV_DATA_SIZE    12 /* invalid data size */
#define ST_ACCESS_DENIED    13 /* access denied */
#define ST_NF_FILE          14 /* file not found */
#define ST_IO_ERROR         15 /* disk I/O error */
#define ST_UNK_FS           16 /* unsupported file system */
#define ST_ERR_BOOT         17 /* invalid FS bootsector, please format partition */      
#define ST_MBR_ERR          18 /* MBR is corrupted */
#define ST_BLDR_INSTALLED   19 /* bootloader is already installed */
#define ST_NF_SPACE         20 /* not enough space after partitions to install bootloader */
#define ST_BLDR_NOTINST     21 /* bootloader is not installed */
#define ST_INV_BLDR_SIZE    22 /* invalid bootloader size */
#define ST_BLDR_NO_CONF     23 /* bootloader corrupted, config not found */
#define ST_BLDR_OLD_VER     24 /* old bootloader cannot be configured */
#define ST_AUTORUNNED       25 /* */
#define ST_NEED_EXIT        26 /* */
#define ST_NO_ADMIN         27 /* user does not have admin privileges */
#define ST_NF_BOOT_DEV      28 /* boot device not found */
#define ST_REG_ERROR        29 /* cannot open registry key */
#define ST_NF_REG_KEY       30 /* registry key not found */
#define ST_SCM_ERROR        31 /* cannot open SCM database */
#define ST_FINISHED         32 /* encryption finished */
#define ST_INV_SECT         34 /* device has unsupported sector size */
#define ST_CLUS_USED        35 /* shrinking error, last clusters are used */
#define ST_NF_PT_SPACE      36 /* not enough free space in partition to continue encrypting */
#define ST_MEDIA_CHANGED    37 /* removable media changed */
#define ST_NO_MEDIA         38 /* no removable media in device */
#define ST_DEVICE_BUSY      39 /* device is busy */
#define ST_INV_MEDIA_TYPE   40 /* media type not supported */
#define ST_FORMAT_NEEDED    41 /* */
#define ST_CANCEL           42 /* operation canceled */
#define ST_INV_VOL_VER      43 /* invalid volume version */
#define ST_EMPTY_KEYFILES   44 /* keyfiles not found */
#define ST_NOT_BACKUP       45 /* this is not a backup file */
#define ST_NO_OPEN_FILE     46 /* cannot open file */
#define ST_NO_CREATE_FILE   47 /* cannot create file */
#define ST_INV_VOLUME       48 /* invalid volume header */
#define ST_OLD_VERSION      49 /* */
#define ST_NEW_VERSION      50 /* */
#define ST_ENCRYPTED        51 /* */
#define ST_INCOMPATIBLE     52 /* */
#define ST_LOADED           53 /* */
#define ST_VOLUME_TOO_NEW   54 /* */
#define ST_INVALID_PARAM    55 /* an invalid parameter was provided */
#define ST_INV_FORMAT       56 /* disk has incompatible partition format */
#define ST_NO_OPEN_DIR      57 /* cannot open directory */
#define ST_DIR_NOT_EMPTY    58 /* directory is not empty */
#define ST_BL_NOT_PASSED    59 /* bootloader check not passed */
#define ST_SB_NO_PASS	    60 /* secureboot enabled and not accepting DCS signature */
#define ST_SHIM_MISSING	    61 /* shim package is missing */
#define ST_HEADER_TO_BIG    62 /* volume header is to big */
#define ST_BUF_TO_SMALL     63 /* buffer is to small */
#define ST_BAD_INDEX        64 /* bad index */
#define ST_SLOT_NOT_OK      65 /* failed to set up keyslot */
#define ST_SHRINK_FAILED    66 /* failed to shrink filesystem */
#define ST_PASS_NOT_FOUND   67 /* password not found in cache */
#define ST_MORE_DATA        68 /* more data available than buffer can hold */

/* data wipe modes */
#define WP_NONE				0 // no data wipe
#define WP_DOD_E			1 // US DoD 5220.22-M (8-306. / E)
#define WP_DOD				2 // US DoD 5220.22-M (8-306. / E, C and E)
#define WP_GUTMANN			3 // Gutmann
#define WP_NUM				4

#define WP_SKIP_UNUSED      0xFF // special wipe mode: skip unused sectors

/* Keyfile mixing modes */
#define KEYFILE_MIX_LEGACY		 0
#define KEYFILE_MIX_HASHED		 1
#define KEYFILE_MIX_HASHED_PRE	 2

/* registry config flags */
#define CONF_FORCE_DISMOUNT   0x001
#define CONF_CACHE_PASSWORD   0x002
#define CONF_EXPLORER_MOUNT   0x004
#define CONF_WIPEPAS_LOGOFF   0x008
#define CONF_DISMOUNT_LOGOFF  0x010
#define CONF_AUTO_START       0x020
#define CONF_HIDE_DCSYS       0x040
#define CONF_HW_CRYPTO        0x080
#define CONF_AUTOMOUNT_BOOT   0x100
#define CONF_DISABLE_TRIM     0x200
#define CONF_ENABLE_SSD_OPT   0x400

#define CONF_BLOCK_UNENC_REMOVABLE 0x0800
#define CONF_BLOCK_UNENC_HDDS      0x1000
#define CONF_BLOCK_UNENC_CDROM     0x2000
#define CONF_PROTECT_RAW_VOLUMES   0x4000

#define IS_BLOCK_UNENC_HDDS_DISABLED(_sysdev_flags) \
	((_sysdev_flags & (F_SYNC | F_FORMATTING)) || (_sysdev_flags & F_ENABLED) == 0)

/* driver status flags */
#define DST_VIA_PADLOCK	    0x01 // VIA Padlock instructions available
#define DST_INTEL_NI        0x02 // AES_NI instructions available
#define DST_INSTR_SSE2      0x04 // SSE2 instructions available
#define DST_INSTR_AVX       0x08 // SSE2 instructions available
#define DST_HW_CRYPTO   (DST_VIA_PADLOCK | DST_INTEL_NI)

#define DST_BOOTLOADER      0x10 // system started via DC bootloader
#define DST_SMALL_MEM       0x20 // BIOS base memory too small for DC bootloader
#define DST_UEFI_BOOT       0x40 // booted in UEFI mode

#define DST_ARGON2_OK       0x80 // Argon2id selftest passed (may fail on low memory systems)

/* bootloader */
#define DC_BOOTHOOK_SIZE    30 // bootloader resident memory needed

#endif