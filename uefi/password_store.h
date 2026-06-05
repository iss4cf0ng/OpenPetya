// password_store.h

#ifndef PASSWORD_STORE_H
#define PASSWORD_STORE_H

#include <efi.h>
#include <efilib.h>

#include "ntfs.h"

#define PW_SECTOR 59
#define PW_MAX_LEN 64

#define PW_MAGIC 0x50415353UL // "PASS"

typedef struct 
{
    uint32_t magic;
    uint8_t len;
    char password[PW_MAX_LEN];
    uint8_t padding[443];
} __attribute__((packed)) PasswordStore;

int pwstore_read(char *buffer, int max_len);

int pwstore_erase(void);

#endif