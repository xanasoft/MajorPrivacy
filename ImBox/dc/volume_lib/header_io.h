/*
    * DCrypt volume_lib - reading and writing a header on a device
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * See PROVENANCE.md: this file began as an extraction from DiskCryptor's
    * driver, and everything that still carried ntldr's expression was rewritten
    * before the licence changed.
*/
#ifndef _HEADER_IO_H_
#define _HEADER_IO_H_

#include "defines.h"
#include "volume_header.h"
#include "xts_fast.h"
#include "dc_dev.h"
/* HF_* - the flags argument below */
#include "dc_status.h"

int io_read_header_full(dc_dev *dev, dc_rw_fn rw, u64 pos, dc_header **header, xts_key *hdr_key, int hdr_len);
int io_read_header(dc_dev *dev, dc_rw_fn rw, u64 pos, dc_header **header, xts_key **out_key, dc_pass *password, int* out_kdf, ULONG *interrupt_cmd);
int io_write_header(dc_dev *dev, dc_rw_fn rw, u64 pos, dc_header *header, xts_key *hdr_key, dc_pass *password, u32 flags, ULONG *interrupt_cmd);

unsigned long calculate_header_crc(dc_header* header);

BOOLEAN is_volume_header_correct(dc_header *header);


int get_ext_header(dc_header *header, dc_ext_header **out_ext_hdr);

#endif