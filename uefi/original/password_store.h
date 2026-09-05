// password_store.h

#ifndef PASSWORD_STORE_H
#define PASSWORD_STORE_H

#include <efi.h>
#include <efilib.h>

#include "ntfs.h"
#include "config.h"

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