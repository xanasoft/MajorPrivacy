# Third-party notices — DCrypt `crypto_lib`

`crypto_lib` as a whole is **MIT**. It contains no GPL- or LGPL-licensed code.

Everything below is third-party material carried in under its own terms, all of which are as
permissive as MIT or more so. Exactly one of them — Gladman's AES assembler — carries an
obligation beyond MIT's: its notice must appear in **binary** distributions too, not only in
source. Shipping this file alongside the product satisfies that.

---

## Brian Gladman — AES assembler core

**File:** `amd64/aes_amd64.nasm`
**Terms:** BSD-style, 3 clauses (attribution in source, attribution in binaries, no endorsement)

```
Copyright (c) 1998-2007, Brian Gladman, Worcester, UK. All rights reserved.

LICENSE TERMS

The free distribution and use of this software is allowed (with or without
changes) provided that:

 1. source code distributions include the above copyright notice, this
    list of conditions and the following disclaimer;

 2. binary distributions include the above copyright notice, this list
    of conditions and the following disclaimer in their documentation;

 3. the name of the copyright holder is not used to endorse products
    built using this software without specific written permission.

DISCLAIMER

This software is provided 'as is' with no explicit or implied warranties
in respect of its properties, including, but not limited to, correctness
and/or fitness for purpose.
```

**This is the one clause in the tree that binds a binary release.** DiskCryptor's
documentation must reproduce the notice above. Clause 3 also means Gladman's name must not
be used to promote anything built on it.

This file is the scalar AES-256 block core, used when AES-NI is unavailable or when the user
turns hardware crypto off. It is unmodified from upstream.

---

## Tom St Denis — LibTomCrypt SHA-512

**Files:** `sha512.c`, `sha512.h`
**Terms:** Public domain — no obligations

```
LibTomCrypt, modular cryptographic library -- Tom St Denis

The library is free for all purposes without any express guarantee it works.

Tom St Denis, tomstdenis@gmail.com, http://libtomcrypt.com
```

Local changes: ARM64 portability shims only.

---

## Argon2 reference implementation

**Files:** `Argon2/**` except `argon2_mt.c` / `argon2_mt.h`
**Terms:** CC0 1.0 Universal **or** Apache License 2.0, at our option — we take **CC0**

```
Argon2 reference source code package - reference C implementations

Copyright 2015
Daniel Dinu, Dmitry Khovratovich, Jean-Philippe Aumasson, and Samuel Neves

You may use this work under the terms of a Creative Commons CC0 1.0
License/Waiver or the Apache Public License 2.0, at your option.
```

CC0 imposes nothing at all, which keeps the whole tree at MIT-or-weaker and avoids
Apache-2.0's incompatibility with GPL-2.0 for any downstream user who needs that.

These files reached DiskCryptor by way of VeraCrypt. They carry only the upstream Argon2
notice plus a DiskCryptor integration note — no VeraCrypt copyright line. **One thing to
confirm before a formal release:** VeraCrypt is Apache-2.0, so any original expression
VeraCrypt added and that still survives here would be Apache-2.0, not CC0 — CC0 was not
VeraCrypt's to grant. The described changes were light and nothing in the files claims
otherwise, but a diff against upstream `phc-winner-argon2` would settle it. If VeraCrypt
material does survive, the fix is either to keep those files marked Apache-2.0 (still
permissive, still fine next to MIT) or to re-apply the DiskCryptor integration on top of
pristine upstream sources.

`Argon2/argon2_mt.c` and `argon2_mt.h` are DiskCryptor's own threading layer and are MIT like
the rest of the library.

---

## Specification-derived constant tables

Several tables in this library are parameters published in the algorithms' own
specifications. They were regenerated here from those published parameters by the generators
in `tools/`, not transcribed from any implementation:

| Table | File | Source | Generator |
|---|---|---|---|
| Twofish `q0` / `q1` permutations | `twofish.c` | Twofish paper, the four 4-bit t-boxes per permutation | `tools/gen_twofish_q.c` |
| Twofish MDS column tables | `twofish.c` | Twofish paper, MDS matrix over GF(2^8)/0x169 | `tools/gen_twofish_mds.c` |
| AES S-box | `aes_key.c` | FIPS-197: inverse in GF(2^8)/0x11B, then the affine transform | `tools/gen_aes_sbox.c` |
| CRC-32 table | `crc32.c` | IEEE 802.3, reflected polynomial 0xEDB88320 | `tools/gen_crc32.c` |

Each generator's output was verified byte-for-byte against independently published values for
those tables. No third-party terms attach to any of them.

---

## What is *not* here

Dropped rather than carried over, because they were GPL and could not be relicensed:

| Dropped | Was | Replaced by |
|---|---|---|
| `crypto_fast/serpent.c` | Linux `crypto/serpent_generic.c`, GPL-2.0-or-later | `serpent.c`, written from the Serpent specification |
| `crypto_fast/twofish.c` | Linux `crypto/twofish_common.c`, GPL-2.0-or-later | `twofish.c`, written from the Twofish specification |
| `amd64/twofish_amd64.nasm` | Linux `twofish-x86_64-asm_64.S` (Joachim Fritschi), GPL-2.0-or-later | nothing — table-driven C |
| `xts_fast.c`, `xts_serpent_sse2.c`, the two Serpent `.nasm` disassemblies, `xts_aes_ni_amd64.nasm`, `sha512_hmac*.c`, `sha512_pkcs5_2.c`, `crc32.c` | ntldr, GPL-3.0-only | rewritten from the relevant specifications |
| `aes_padlock_*.{nasm,asm,h}` | ntldr, GPL-3.0-only | nothing — VIA PadLock is long obsolete |
| all of `i386/` | mixed | nothing — 32-bit x86 support dropped |
