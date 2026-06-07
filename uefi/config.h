// config.h

#ifndef CONFIG_H
#define CONFIG_H

#define MFT_SECTOR_COUNT 256

#define MAX_ATTEMPTS 3
#define MAX_PW_LEN 32

// ----------[ Hidden storage ]----------

#define HIDDEN_MAGIC 0x484F4C44UL // "HOLD"

// ----------[ State ]----------

#define STATE_SECTOR 60
#define STATE_MAGIC 0x424F4F54UL

#define STATE_NOT_SETUP 0x00
#define STATE_ENCRYPTED 0x01

// ----------[ Password ]----------

#define PW_SECTOR 59
#define PW_MAX_LEN 64

#define PW_MAGIC 0x50415353UL // "PASS"

// ----------[ NTFS cryptography ]----------

#define VALIDATION_SECTOR 61
#define TAG_SIZE 32

#define SALT_SECTOR 62
#define SALT_SIZE 16

#define MFT_ENCRYPT_SECTORS 256

#define KDF_ITERATIONS 1000

#define VALIDATE_MAGIC_0 0xAB
#define VALIDATE_MAGIC_1 0xCD
#define VALIDATE_MAGIC_2 0xEF
#define VALIDATE_MAGIC_3 0x12

#endif