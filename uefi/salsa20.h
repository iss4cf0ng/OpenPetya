// salsa20.h

#ifndef SALSA20_H
#define SALSA20_H

#include <efi.h>
#include <efilib.h>

typedef struct {
    uint32_t state[16];
} Salsa20_Ctx;

void salsa20_init(Salsa20_Ctx *ctx, const uint8_t key[32], const uint8_t nonce[8], uint64_t counter);

void salsa20_encrypt(Salsa20_Ctx *ctx, const uint8_t *in, uint8_t *out, uint32_t len);

#endif