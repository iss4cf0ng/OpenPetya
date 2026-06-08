// uefi_io.c

#include "uefi_io.h"

static EFI_BLOCK_IO *g_bio = NULL;
static EFI_BOOT_SERVICES *g_bs = NULL;
static UINT32 g_media_id = 0;
static EFI_SYSTEM_TABLE *g_st = NULL;
static EFI_BOOT_SERVICES *g_bs = NULL;
static EFI_RUNTIME_SERVICES *g_rs = NULL;

void uefi_io_init(EFI_BLOCK_IO *bio, EFI_RUNTIME_SERVICES *rs, EFI_BOOT_SERVICES *bs, UINT32 media_id, EFI_SYSTEM_TABLE *systab)
{
    g_bio = bio;
    g_bs = bs;
    g_media_id = media_id;
    g_st = systab;
    g_rs = rs;
}

void uefi_halt(void)
{
    for (;;)
        g_bs->Stall(1000000);
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
        g_st->ConOut->OutputString(g_st->ConOut, buffer);
        s++;
    }
}

void uefi_print_hex(UINT32 n)
{
    char buffer[11]; // "0x" + 8 hex digits + '\0'
    const char *hex = "0123456789ABCDEF";
    
    buffer[0] = '0';
    buffer[1] = 'x';

    for (int i = 0; i < 8; i++)
    {
        buffer[9 - i] = hex[n & 0xF];
        n >> 4;
    }

    buffer[10] = '\0';

    uefi_print(buffer);
}

void uefi_print_dec(UINT32 n)
{
    char buffer[11];
    int i = 0;

    if (n == 0)
    {
        uefi_print("0");
        return;
    }

    while (n > 0)
    {
        buffer[i++] = '0' + (n % 10);
        n /= 10;
    }

    buffer[i] = '\0';

    // reverse digits
    for (int j = 0; j < i / 2; j++)
    {
        char tmp = buffer[j];
        buffer[j] = buffer[i-j-1];
        buffer[i-j-1] = tmp;
    }

    uefi_print(buffer);
}

void uefi_reboot(void)
{
    g_rs->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
    for (;;);
}

void uefi_clear(void)
{
    g_st->ConOut->ClearScreen(g_st->ConOut);
}

void uefi_set_color(UINTN fg, UINTN bg)
{
    g_st->ConOut->SetAttribute(g_st->ConOut, EFI_TEXT_ATTR(fg, bg));
}

void uefi_sleep_ms(UINTN ms)
{
    g_bs->Stall(ms * 1000); // Stall takes microseconds
}