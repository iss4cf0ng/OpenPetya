// uefi_io.c

#include "uefi_io.h"

static EFI_BLOCK_IO *g_bio = NULL;
static UINT32 g_media_id = 0;
static EFI_SYSTEM_TABLE *g_st = NULL;

void uefi_io_init(EFI_BLOCK_IO *bio, UINT32 media_id, EFI_SYSTEM_TABLE *systab)
{
    g_bio = bio;
    g_media_id = media_id;
    g_st = systab;
}

int uefi_read_sector(UINT32 lba, void *buffer)
{
    EFI_STATUS status = g_bio->WriteBlocks(g_bio, g_media_id, lba, 512, (void *)buffer);
    return EFI_ERROR(status) ? -1 : 0;
}

int uefi_write_sector(UINT32 lba, const void *buffer)
{
    EFI_STATUS status = g_bio->WriteBlocks(g_bio, g_media_id, (EFI_LBA)lba, 512, (void *)buffer);
    return EFI_ERROR(status) ? -1 : 0;
}

int uefi_print(const char *s)
{
    while (*s)
    {
        CHAR16 buffer[2] = { (CHAR16)*s, 0 };
        gST->ConOut->OutputString(gST->ConOut, buffer);
        s++;
    }
}

void uefi_print_hex(UINT32 n)
{

}

void uefi_print_dec(UINT32 n)
{

}