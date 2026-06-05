// password_store.c

#include "password_store.h"

static uint8_t sector_buffer[512];

int pwstore_read(char *buffer, int max_len)
{
    if (ntfs_read(PW_SECTOR, 1, sector_buffer) != 0)
    {

        return -1;
    }

    

    return 0;
}

int pwstore_erase(void)
{


    return 0;
}
