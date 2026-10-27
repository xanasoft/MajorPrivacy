/*
    * DCrypt volume_lib - status codes and header operation flags
    * Copyright (c) 2026 DiskCryptor contributors
    *
    * SPDX-License-Identifier: MIT
    *
    * volume_lib returns the project's existing ST_* codes rather than inventing
    * a parallel set: the driver and dcapi already handle them, and a second
    * numbering would have to be translated at every call site.
    *
    * The driver and dcapi get them from include\dcconst.h, which is on their
    * include path. DcsPkg has no dcconst.h at all - its own dc_header.c
    * returned plain 1 and 0 - so for EFI the subset volume_lib actually uses is
    * defined here instead.
    *
    * dcconst.h is preferred when it is reachable, so that a tree which extends
    * it does not end up with two sources of truth. Everything below is
    * #ifndef-guarded, so including dcconst.h first also works.
    *
    * The values match dcconst.h and must keep matching. There is no way to
    * assert that from here without requiring the file, so it is called out in
    * DESIGN.md as something to check if dcconst.h is ever renumbered.
*/
#ifndef _DC_STATUS_H_
#define _DC_STATUS_H_

#if defined(__has_include)
 #if __has_include("dcconst.h")
  #include "dcconst.h"
 #endif
#endif

#ifndef ST_OK
 #define ST_OK              0    /* success */
#endif
#ifndef ST_ERROR
 #define ST_ERROR           1    /* unspecified failure */
#endif
#ifndef ST_RW_ERR
 #define ST_RW_ERR          3    /* read/write error */
#endif
#ifndef ST_PASS_ERR
 #define ST_PASS_ERR        4    /* nothing in the search space matched */
#endif
#ifndef ST_NOMEM
 #define ST_NOMEM           9    /* allocation failed */
#endif
#ifndef ST_INV_DATA_SIZE
 #define ST_INV_DATA_SIZE   12   /* buffer or header length not usable */
#endif
#ifndef ST_IO_ERROR
 #define ST_IO_ERROR        15   /* device I/O error */
#endif
#ifndef ST_FINISHED
 #define ST_FINISHED        32   /* reached the end */
#endif
#ifndef ST_INV_SECT
 #define ST_INV_SECT        34   /* unsupported sector size */
#endif
#ifndef ST_CANCEL
 #define ST_CANCEL          42   /* aborted */
#endif
#ifndef ST_INV_VOLUME
 #define ST_INV_VOLUME      48   /* header is structurally invalid */
#endif
#ifndef ST_INCOMPATIBLE
 #define ST_INCOMPATIBLE    52   /* feature absent in this header version */
#endif
#ifndef ST_INVALID_PARAM
 #define ST_INVALID_PARAM   55   /* caller error */
#endif
#ifndef ST_SMALL_BUFF
 #define ST_SMALL_BUFF      63   /* buffer too small */
#endif
#ifndef ST_BAD_INDEX
 #define ST_BAD_INDEX       64   /* slot index out of range */
#endif
#ifndef ST_NF_SPACE
 #define ST_NF_SPACE        20   /* not enough space */
#endif
#ifndef ST_SLOT_NOT_OK
 #define ST_SLOT_NOT_OK     65   /* key slot could not be rewrapped */
#endif
#ifndef ST_EMPTY_KEYFILES
 #define ST_EMPTY_KEYFILES  44   /* no keyfile was found where one was named */
#endif

/*
 * Header operation flags - the flags argument to io_write_header, which
 * says how much of the header to rewrite. Same arrangement as the status
 * codes above: dcconst.h when it is reachable, these otherwise, and the
 * values have to keep matching.
 */
#ifndef HF_DEFAULT
 #define HF_DEFAULT       0x0000  /* write the entire header */
#endif
#ifndef HF_UPDATE_BASE
 #define HF_UPDATE_BASE   0x0001  /* header base only */
#endif
#ifndef HF_UPDATE_SLOTS
 #define HF_UPDATE_SLOTS  0x0002  /* key slots only */
#endif
#ifndef HF_UPDATE_EXT
 #define HF_UPDATE_EXT    0x0004  /* extended header */
#endif
#ifndef HF_CLEAR_SLOTS
 #define HF_CLEAR_SLOTS   0x0010  /* overwrite the slot area */
#endif
#ifndef HF_HEADER_FILL
 #define HF_HEADER_FILL   0x0020  /* wipe header sectors first */
#endif
#ifndef HF_KEEP_SALT
 #define HF_KEEP_SALT     0x0040  /* keep the original salt */
#endif
#ifndef HF_BACKUP_HEADER
 #define HF_BACKUP_HEADER 0x0080  /* backup header at the partition end */
#endif
#ifndef HF_UPDATE_ALL
 #define HF_UPDATE_ALL (HF_UPDATE_BASE | HF_UPDATE_SLOTS | HF_UPDATE_EXT)
#endif

#endif /* _DC_STATUS_H_ */
