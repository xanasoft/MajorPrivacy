/*
 * Argon2 reference source code package - reference C implementations
 *
 * Copyright 2015
 * Daniel Dinu, Dmitry Khovratovich, Jean-Philippe Aumasson, and Samuel Neves
 *
 * You may use this work under the terms of a Creative Commons CC0 1.0
 * License/Waiver or the Apache Public License 2.0, at your option. The terms of
 * these licenses can be found at:
 *
 * - CC0 1.0 Universal : https://creativecommons.org/publicdomain/zero/1.0
 * - Apache 2.0        : https://www.apache.org/licenses/LICENSE-2.0
 *
 * You should have received a copy of both of these licenses along with this
 * software. If not, they may be obtained at the above URLs.
 */

#ifndef PORTABLE_BLAKE2_IMPL_H
#define PORTABLE_BLAKE2_IMPL_H

#ifdef _WIN32
#define BLAKE2_INLINE __inline
#elif defined(__GNUC__) || defined(__clang__)
#define BLAKE2_INLINE __inline__
#else
#define BLAKE2_INLINE
#endif

/* Argon2 Team - Begin Code */
/*
   Not an exhaustive list, but should cover the majority of modern platforms
   Additionally, the code will always be correct---this is only a performance
   tweak.
*/
#if (defined(__BYTE_ORDER__) &&                                                \
     (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)) ||                           \
    defined(__LITTLE_ENDIAN__) || defined(__ARMEL__) || defined(__MIPSEL__) || \
    defined(__AARCH64EL__) || defined(__amd64__) || defined(__i386__) ||       \
    defined(_M_IX86) || defined(_M_X64) || defined(_M_AMD64) ||                \
    defined(_M_ARM) || defined(_M_ARM64)
#define NATIVE_LITTLE_ENDIAN
#endif
/* Argon2 Team - End Code */

static BLAKE2_INLINE u32 load32(const void *src) {
#if defined(NATIVE_LITTLE_ENDIAN)
    u32 w;
    memcpy(&w, src, sizeof w);
    return w;
#else
    const u8 *p = (const u8 *)src;
    u32 w = *p++;
    w |= (u32)(*p++) << 8;
    w |= (u32)(*p++) << 16;
    w |= (u32)(*p++) << 24;
    return w;
#endif
}

static BLAKE2_INLINE u64 load64(const void *src) {
#if defined(NATIVE_LITTLE_ENDIAN)
    u64 w;
    memcpy(&w, src, sizeof w);
    return w;
#else
    const u8 *p = (const u8 *)src;
    u64 w = *p++;
    w |= (u64)(*p++) << 8;
    w |= (u64)(*p++) << 16;
    w |= (u64)(*p++) << 24;
    w |= (u64)(*p++) << 32;
    w |= (u64)(*p++) << 40;
    w |= (u64)(*p++) << 48;
    w |= (u64)(*p++) << 56;
    return w;
#endif
}

static BLAKE2_INLINE void store32(void *dst, u32 w) {
#if defined(NATIVE_LITTLE_ENDIAN)
    memcpy(dst, &w, sizeof w);
#else
    u8 *p = (u8 *)dst;
    *p++ = (u8)w;
    w >>= 8;
    *p++ = (u8)w;
    w >>= 8;
    *p++ = (u8)w;
    w >>= 8;
    *p++ = (u8)w;
#endif
}

static BLAKE2_INLINE void store64(void *dst, u64 w) {
#if defined(NATIVE_LITTLE_ENDIAN)
    memcpy(dst, &w, sizeof w);
#else
    u8 *p = (u8 *)dst;
    *p++ = (u8)w;
    w >>= 8;
    *p++ = (u8)w;
    w >>= 8;
    *p++ = (u8)w;
    w >>= 8;
    *p++ = (u8)w;
    w >>= 8;
    *p++ = (u8)w;
    w >>= 8;
    *p++ = (u8)w;
    w >>= 8;
    *p++ = (u8)w;
    w >>= 8;
    *p++ = (u8)w;
#endif
}

static BLAKE2_INLINE u64 load48(const void *src) {
    const u8 *p = (const u8 *)src;
    u64 w = *p++;
    w |= (u64)(*p++) << 8;
    w |= (u64)(*p++) << 16;
    w |= (u64)(*p++) << 24;
    w |= (u64)(*p++) << 32;
    w |= (u64)(*p++) << 40;
    return w;
}

static BLAKE2_INLINE void store48(void *dst, u64 w) {
    u8 *p = (u8 *)dst;
    *p++ = (u8)w;
    w >>= 8;
    *p++ = (u8)w;
    w >>= 8;
    *p++ = (u8)w;
    w >>= 8;
    *p++ = (u8)w;
    w >>= 8;
    *p++ = (u8)w;
    w >>= 8;
    *p++ = (u8)w;
}

static BLAKE2_INLINE u32 rotr32(const u32 w, const unsigned c) {
    return (w >> c) | (w << (32 - c));
}

static BLAKE2_INLINE u64 rotr64(const u64 w, const unsigned c) {
    return (w >> c) | (w << (64 - c));
}

void clear_internal_memory(void *v, size_t n);

#endif
