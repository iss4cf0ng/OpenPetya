// kdf.h

#ifndef KDF_H
#define KDF_H

#include <efi.h>
#include <efilib.h>

void kdf_derive(uint8_t key[32], const char *password, const uint8_t salt[16], uint32_t iterations);

#endif