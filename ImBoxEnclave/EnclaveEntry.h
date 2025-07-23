#pragma once
#include ".\crypto_fast\xts_fast.h"
#include ".\crypto_fast\aes_asm.h"

typedef struct KeySetArgs {
	const unsigned char* key;
	int alg;
	xts_key* skey;
} KeySetArgs;