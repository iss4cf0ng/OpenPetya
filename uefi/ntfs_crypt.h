// ntfs_crypt.h

#ifndef NTFS_CRYPT_H
#define NTFS_CRYPT_H

#include <efi.h>
#include <efilib.h>

#include "config.h"
#include "ntfs.h"
#include "salsa20.h"
#include "kdf.h"

int ntfs_mft_encrypt(const char *password, uint32_t partition_lba);

int ntfs_mft_decrypt(const char *password, uint32_t partition_lba);

int ntfs_generate_salt(void);

int validate_save_tag(const uint8_t key[32]);

int validate_check_key(const uint8_t key[32]);

#endif