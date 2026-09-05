//kdf.c

#include "kdf.h"
#include "salsa20.h"

void kdf_derive(uint8_t key[32], const char *password, const uint8_t salt[16], uint32_t iterations)
{
    uint8_t state[64] = { 0 };

    int i = 0;
    while (*password && i < 32)
    {
        state[i % 32] ^= (uint8_t)*password;
        i++;
    }

    for (int j = 0; j < 16; j++)
        state[j] ^= salt[j];

    Salsa20_Ctx ctx;
    uint8_t nonce[8] = { 0 };

    for (uint32_t iter = 0; iter < iterations; iter++)
    {
        salsa20_init(&ctx, state, nonce, iter);
        salsa20_encrypt(&ctx, state, state, 64);  // full 64 bytes, no OOB

        state[0] ^= (uint8_t)iter;
        state[1] ^= (uint8_t)(iter >> 8);
        state[2] ^= (uint8_t)(iter >> 16);
        state[3] ^= (uint8_t)(iter >> 24);
    }

    for (int j = 0; j < 32; j++)
        key[j] = state[j];
}