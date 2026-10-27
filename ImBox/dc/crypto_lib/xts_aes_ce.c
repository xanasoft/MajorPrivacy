/*
    * DiskCryptor crypto_lib - XTS-AES on ARM64 crypto extensions
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
*/

#ifdef _M_ARM64

#include <arm_neon.h>
#if !defined(_KERNEL_MODE) && !defined(_UEFI)
#include <Windows.h>
#endif
#include "aes_key.h"
#include "xts_fast.h"
#include "xts_aes_ce.h"

/* Check for ARM64 Crypto Extensions support */
int _stdcall xts_aes_ce_available(void)
{
#if defined(_KERNEL_MODE) || defined(_UEFI)
    /* ARM64 Crypto Extensions are mandatory on Windows ARM64,
       and firmware has no IsProcessorFeaturePresent to ask. */
    return 1;
#else
    return IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE);
#endif
}

/* Single block AES-256 encrypt using ARM64 CE
 * ARM64 AESE does: AddRoundKey, SubBytes, ShiftRows (in that order)
 * So we need: 13x (AESE+AESMC) + 1x AESE + final XOR
 */
void _stdcall aes256_arm64_encrypt(const unsigned char *in, unsigned char *out, aes256_key *key)
{
    uint8x16_t block = vld1q_u8(in);

    /* Rounds 0-12: AESE (AddRoundKey + SubBytes + ShiftRows) + AESMC (MixColumns) */
    block = vaeseq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->enc_key[0])));
    block = vaesmcq_u8(block);
    block = vaeseq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->enc_key[4])));
    block = vaesmcq_u8(block);
    block = vaeseq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->enc_key[8])));
    block = vaesmcq_u8(block);
    block = vaeseq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->enc_key[12])));
    block = vaesmcq_u8(block);
    block = vaeseq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->enc_key[16])));
    block = vaesmcq_u8(block);
    block = vaeseq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->enc_key[20])));
    block = vaesmcq_u8(block);
    block = vaeseq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->enc_key[24])));
    block = vaesmcq_u8(block);
    block = vaeseq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->enc_key[28])));
    block = vaesmcq_u8(block);
    block = vaeseq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->enc_key[32])));
    block = vaesmcq_u8(block);
    block = vaeseq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->enc_key[36])));
    block = vaesmcq_u8(block);
    block = vaeseq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->enc_key[40])));
    block = vaesmcq_u8(block);
    block = vaeseq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->enc_key[44])));
    block = vaesmcq_u8(block);
    block = vaeseq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->enc_key[48])));
    block = vaesmcq_u8(block);

    /* Round 13: AESE only (no MixColumns for final round) */
    block = vaeseq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->enc_key[52])));

    /* Final AddRoundKey */
    block = veorq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->enc_key[56])));

    vst1q_u8(out, block);
}

/* Single block AES-256 decrypt using ARM64 CE
 * ARM64 AESD does: AddRoundKey, InvSubBytes, InvShiftRows (in that order)
 * So we need: 13x (AESD+AESIMC) + 1x AESD + final XOR
 */
void _stdcall aes256_arm64_decrypt(const unsigned char *in, unsigned char *out, aes256_key *key)
{
    uint8x16_t block = vld1q_u8(in);

    /* Rounds 0-12: AESD (AddRoundKey + InvSubBytes + InvShiftRows) + AESIMC (InvMixColumns) */
    block = vaesdq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->dec_key[0])));
    block = vaesimcq_u8(block);
    block = vaesdq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->dec_key[4])));
    block = vaesimcq_u8(block);
    block = vaesdq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->dec_key[8])));
    block = vaesimcq_u8(block);
    block = vaesdq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->dec_key[12])));
    block = vaesimcq_u8(block);
    block = vaesdq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->dec_key[16])));
    block = vaesimcq_u8(block);
    block = vaesdq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->dec_key[20])));
    block = vaesimcq_u8(block);
    block = vaesdq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->dec_key[24])));
    block = vaesimcq_u8(block);
    block = vaesdq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->dec_key[28])));
    block = vaesimcq_u8(block);
    block = vaesdq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->dec_key[32])));
    block = vaesimcq_u8(block);
    block = vaesdq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->dec_key[36])));
    block = vaesimcq_u8(block);
    block = vaesdq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->dec_key[40])));
    block = vaesimcq_u8(block);
    block = vaesdq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->dec_key[44])));
    block = vaesimcq_u8(block);
    block = vaesdq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->dec_key[48])));
    block = vaesimcq_u8(block);

    /* Round 13: AESD only (no InvMixColumns for final round) */
    block = vaesdq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->dec_key[52])));

    /* Final AddRoundKey */
    block = veorq_u8(block, vreinterpretq_u8_u32(vld1q_u32(&key->dec_key[56])));

    vst1q_u8(out, block);
}

/*
 * T <- T * x in GF(2^128) with the XTS polynomial (x^128 + x^7 + x^2 + x + 1),
 * on the little-endian halves lo = bytes 0-7 and hi = bytes 8-15.
 *
 * Deliberately on general-purpose registers, not NEON. Each block's tweak
 * depends on the previous one, so this chain is strictly serial and sets the
 * pace for all four otherwise independent AES lanes. A NEON version needs the
 * carry out of bit 127 as a scalar (vgetq_lane + vdupq_n): a NEON -> GPR -> NEON
 * round trip on every block, right inside that chain, which held XTS-AES to
 * less than half the throughput these three GPR operations allow.
 */
#define XTS_MUL_X(lo, hi) {                                                    \
    unsigned __int64 _c = (unsigned __int64)((__int64)(hi) >> 63) & 0x87;      \
    (hi) = ((hi) << 1) | ((lo) >> 63);                                         \
    (lo) = ((lo) << 1) ^ _c;                                                   \
}
#define XTS_TWEAK_VEC(lo, hi) \
    vreinterpretq_u8_u64(vcombine_u64(vcreate_u64(lo), vcreate_u64(hi)))

/* XTS-AES-256 encrypt using ARM64 CE - processes 4 blocks at a time */
void _stdcall xts_aes_ce_encrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key)
{
    uint8x16_t t0, t1, t2, t3;
    uint8x16_t b0, b1, b2, b3;
    unsigned __int64 idx[2], tw[2], tw_next[2];
    unsigned __int64 lo, hi;
    unsigned __int64 sector_num;
    int i, more;

    sector_num = offset / XTS_SECTOR_SIZE;

    /* Derive this sector's tweak by encrypting its index */
    idx[0] = ++sector_num;
    idx[1] = 0;
    aes256_arm64_encrypt((const unsigned char *)idx, (unsigned char *)tw, &key->tweak_k.aes);
    lo = tw[0];
    hi = tw[1];

    for (;;) {
        /*
         * That derivation is a full fourteen-round AES with a strictly serial
         * dependency chain and nothing inside its own sector to overlap with.
         * Deriving the next sector's here, ahead of this sector's bulk work,
         * makes it independent of everything around it - but only when there
         * is a next sector, because otherwise the result is just discarded.
         */
        more = (len -= XTS_SECTOR_SIZE) != 0;
        if (more) {
            idx[0] = sector_num + 1;
            aes256_arm64_encrypt((const unsigned char *)idx, (unsigned char *)tw_next, &key->tweak_k.aes);
        }
        sector_num++;

        for (i = 0; i < XTS_BLOCKS_IN_SECTOR / 4; i++) {
            /* Tweaks for these 4 blocks; lo:hi is left at the next block's */
            t0 = XTS_TWEAK_VEC(lo, hi); XTS_MUL_X(lo, hi);
            t1 = XTS_TWEAK_VEC(lo, hi); XTS_MUL_X(lo, hi);
            t2 = XTS_TWEAK_VEC(lo, hi); XTS_MUL_X(lo, hi);
            t3 = XTS_TWEAK_VEC(lo, hi); XTS_MUL_X(lo, hi);

            /* Load and pre-tweak 4 blocks */
            b0 = veorq_u8(vld1q_u8(in + 0),  t0);
            b1 = veorq_u8(vld1q_u8(in + 16), t1);
            b2 = veorq_u8(vld1q_u8(in + 32), t2);
            b3 = veorq_u8(vld1q_u8(in + 48), t3);

            /* Encrypt 4 blocks - inline AES rounds for better performance */
            {
                uint8x16_t rk;

                #define AES_ENC_ROUND(rk_idx) \
                    rk = vreinterpretq_u8_u32(vld1q_u32(&key->crypt_k.aes.enc_key[rk_idx])); \
                    b0 = vaeseq_u8(b0, rk); b0 = vaesmcq_u8(b0); \
                    b1 = vaeseq_u8(b1, rk); b1 = vaesmcq_u8(b1); \
                    b2 = vaeseq_u8(b2, rk); b2 = vaesmcq_u8(b2); \
                    b3 = vaeseq_u8(b3, rk); b3 = vaesmcq_u8(b3);

                /* Rounds 0-12: AESE + AESMC */
                AES_ENC_ROUND(0);
                AES_ENC_ROUND(4);
                AES_ENC_ROUND(8);
                AES_ENC_ROUND(12);
                AES_ENC_ROUND(16);
                AES_ENC_ROUND(20);
                AES_ENC_ROUND(24);
                AES_ENC_ROUND(28);
                AES_ENC_ROUND(32);
                AES_ENC_ROUND(36);
                AES_ENC_ROUND(40);
                AES_ENC_ROUND(44);
                AES_ENC_ROUND(48);

                /* Round 13: AESE only (no MixColumns) */
                rk = vreinterpretq_u8_u32(vld1q_u32(&key->crypt_k.aes.enc_key[52]));
                b0 = vaeseq_u8(b0, rk);
                b1 = vaeseq_u8(b1, rk);
                b2 = vaeseq_u8(b2, rk);
                b3 = vaeseq_u8(b3, rk);

                /* Final AddRoundKey */
                rk = vreinterpretq_u8_u32(vld1q_u32(&key->crypt_k.aes.enc_key[56]));
                b0 = veorq_u8(b0, rk);
                b1 = veorq_u8(b1, rk);
                b2 = veorq_u8(b2, rk);
                b3 = veorq_u8(b3, rk);

                #undef AES_ENC_ROUND
            }

            /* Post-tweak and store */
            vst1q_u8(out + 0,  veorq_u8(b0, t0));
            vst1q_u8(out + 16, veorq_u8(b1, t1));
            vst1q_u8(out + 32, veorq_u8(b2, t2));
            vst1q_u8(out + 48, veorq_u8(b3, t3));

            /* Update pointers */
            in += XTS_BLOCK_SIZE * 4;
            out += XTS_BLOCK_SIZE * 4;
        }

        if (!more) break;
        lo = tw_next[0];
        hi = tw_next[1];
    }
}

/* XTS-AES-256 decrypt using ARM64 CE - processes 4 blocks at a time */
void _stdcall xts_aes_ce_decrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key)
{
    uint8x16_t t0, t1, t2, t3;
    uint8x16_t b0, b1, b2, b3;
    unsigned __int64 idx[2], tw[2], tw_next[2];
    unsigned __int64 lo, hi;
    unsigned __int64 sector_num;
    int i, more;

    sector_num = offset / XTS_SECTOR_SIZE;

    /* Derive this sector's tweak by encrypting its index */
    idx[0] = ++sector_num;
    idx[1] = 0;
    aes256_arm64_encrypt((const unsigned char *)idx, (unsigned char *)tw, &key->tweak_k.aes);
    lo = tw[0];
    hi = tw[1];

    for (;;) {
        /*
         * That derivation is a full fourteen-round AES with a strictly serial
         * dependency chain and nothing inside its own sector to overlap with.
         * Deriving the next sector's here, ahead of this sector's bulk work,
         * makes it independent of everything around it - but only when there
         * is a next sector, because otherwise the result is just discarded.
         */
        more = (len -= XTS_SECTOR_SIZE) != 0;
        if (more) {
            idx[0] = sector_num + 1;
            aes256_arm64_encrypt((const unsigned char *)idx, (unsigned char *)tw_next, &key->tweak_k.aes);
        }
        sector_num++;

        for (i = 0; i < XTS_BLOCKS_IN_SECTOR / 4; i++) {
            /* Tweaks for these 4 blocks; lo:hi is left at the next block's */
            t0 = XTS_TWEAK_VEC(lo, hi); XTS_MUL_X(lo, hi);
            t1 = XTS_TWEAK_VEC(lo, hi); XTS_MUL_X(lo, hi);
            t2 = XTS_TWEAK_VEC(lo, hi); XTS_MUL_X(lo, hi);
            t3 = XTS_TWEAK_VEC(lo, hi); XTS_MUL_X(lo, hi);

            /* Load and pre-tweak 4 blocks */
            b0 = veorq_u8(vld1q_u8(in + 0),  t0);
            b1 = veorq_u8(vld1q_u8(in + 16), t1);
            b2 = veorq_u8(vld1q_u8(in + 32), t2);
            b3 = veorq_u8(vld1q_u8(in + 48), t3);

            /* Decrypt 4 blocks - inline AES rounds for better performance */
            {
                uint8x16_t rk;

                #define AES_DEC_ROUND(rk_idx) \
                    rk = vreinterpretq_u8_u32(vld1q_u32(&key->crypt_k.aes.dec_key[rk_idx])); \
                    b0 = vaesdq_u8(b0, rk); b0 = vaesimcq_u8(b0); \
                    b1 = vaesdq_u8(b1, rk); b1 = vaesimcq_u8(b1); \
                    b2 = vaesdq_u8(b2, rk); b2 = vaesimcq_u8(b2); \
                    b3 = vaesdq_u8(b3, rk); b3 = vaesimcq_u8(b3);

                /* Rounds 0-12: AESD + AESIMC */
                AES_DEC_ROUND(0);
                AES_DEC_ROUND(4);
                AES_DEC_ROUND(8);
                AES_DEC_ROUND(12);
                AES_DEC_ROUND(16);
                AES_DEC_ROUND(20);
                AES_DEC_ROUND(24);
                AES_DEC_ROUND(28);
                AES_DEC_ROUND(32);
                AES_DEC_ROUND(36);
                AES_DEC_ROUND(40);
                AES_DEC_ROUND(44);
                AES_DEC_ROUND(48);

                /* Round 13: AESD only (no InvMixColumns) */
                rk = vreinterpretq_u8_u32(vld1q_u32(&key->crypt_k.aes.dec_key[52]));
                b0 = vaesdq_u8(b0, rk);
                b1 = vaesdq_u8(b1, rk);
                b2 = vaesdq_u8(b2, rk);
                b3 = vaesdq_u8(b3, rk);

                /* Final AddRoundKey */
                rk = vreinterpretq_u8_u32(vld1q_u32(&key->crypt_k.aes.dec_key[56]));
                b0 = veorq_u8(b0, rk);
                b1 = veorq_u8(b1, rk);
                b2 = veorq_u8(b2, rk);
                b3 = veorq_u8(b3, rk);

                #undef AES_DEC_ROUND
            }

            /* Post-tweak and store */
            vst1q_u8(out + 0,  veorq_u8(b0, t0));
            vst1q_u8(out + 16, veorq_u8(b1, t1));
            vst1q_u8(out + 32, veorq_u8(b2, t2));
            vst1q_u8(out + 48, veorq_u8(b3, t3));

            /* Update pointers */
            in += XTS_BLOCK_SIZE * 4;
            out += XTS_BLOCK_SIZE * 4;
        }

        if (!more) break;
        lo = tw_next[0];
        hi = tw_next[1];
    }
}

#endif /* _M_ARM64 */

