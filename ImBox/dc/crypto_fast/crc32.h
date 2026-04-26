#ifndef _CRC32_H_
#define _CRC32_H_

unsigned long _stdcall crc32(const unsigned char *p, unsigned long len);

unsigned long _stdcall crc32_combine(unsigned long crc1, unsigned long crc2, unsigned long len2);

#endif