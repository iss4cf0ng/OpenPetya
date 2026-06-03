// ntfs_crypt.h

#ifndef NTFS_CRYPT_H
#define NTFS_CRYPT_H

#include "types.h"

#define SALT_SECTOR 62
#define SALT_SIZE 16

#define MFT_ENCRYPT_SECTORS 256

#define KDF_ITERATIONS 1000

#define VALIDATE_SECTOR 61
#define TAG_SIZE 32

#define VALIDATE_MAGIC_0 0xAB
#define VALIDATE_MAGIC_1 0xCD
#define VALIDATE_MAGIC_2 0xEF
#define VALIDATE_MAGIC_3 0x12

/// @brief Encrypt MFT
/// @param password Salsa20 encryption password
/// @param partition_lba LBA of partition
/// @return 
int ntfs_mft_encrypt(const char *password, uint32_t partition_lba);

/// @brief Decrypt MFT
/// @param password Salsa20 decryption password
/// @param partition_lba LBA of partition
/// @return 
int ntfs_mft_decrypt(const char *password, uint32_t partition_lba);

/// @brief Generate salt for MFT encryption
/// @param  
/// @return 
int ntfs_generate_salt(void);

/// @brief 
/// @param key 
/// @return 
int validate_save_tag(const uint8_t key[32]);

/// @brief 
/// @param key 
/// @return 
int validate_check_key(const uint8_t key[32]);

#endif