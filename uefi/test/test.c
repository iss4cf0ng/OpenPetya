#include <efi.h>
#include <efilib.h>

static EFI_SYSTEM_TABLE *g_st = NULL;
static EFI_BOOT_SERVICES *g_bs = NULL;
static EFI_RUNTIME_SERVICES *g_rs = NULL;
static EFI_BLOCK_IO_PROTOCOL *g_bio = NULL;
static UINT32 g_media_id = 0;

static EFI_STATUS find_block_io(EFI_HANDLE image)
{
    EFI_STATUS status;
    EFI_LOADED_IMAGE *loaded_image = NULL;

    status = uefi_call_wrapper(
        g_bs->HandleProtocol,
        3,
        image,
        &LoadedImageProtocol,
        (void **)&loaded_image
    );

    if (EFI_ERROR(status))
        return status;

    status = uefi_call_wrapper(
        g_bs->HandleProtocol,
        3,
        loaded_image->DeviceHandle,
        &BlockIoProtocol,
        (void **)&g_bio
    );

    if (EFI_ERROR(status))
    {
        g_bio = NULL;
        return status;
    }

    if (g_bio == NULL || g_bio->Media == NULL)
    {
        g_bio = NULL;
        return EFI_NOT_FOUND;
    }

    g_media_id = g_bio->Media->MediaId;

    return EFI_SUCCESS;
}

static int uefi_read_sector(UINT32 lba, void *buffer)
{
    if (g_bio == NULL || buffer == NULL)
        return -1;

    EFI_STATUS status = uefi_call_wrapper(
        g_bio->ReadBlocks,
        5,
        g_bio,
        g_media_id,
        (EFI_LBA)lba,
        512,
        buffer
    );

    return EFI_ERROR(status) ? -1 : 0;
}

static int uefi_write_sector(UINT32 lba, const void *buffer)
{
    
}

void uefi_halt(void)
{
    while (1)
    {
        g_bs->Stall(1000000);
    }
}

void uefi_print(const char *s)
{
    if (s == NULL)
        return;

    while (*s)
    {
        char c = *s;
        if (c == '\n')
            Print(L"\r\n");
        else
        {
            CHAR16 tmp[2];

            tmp[0] = (CHAR16)(unsigned char)c;
            tmp[1] = L'\0';

            Print(tmp);
        }

        s++;
    }
}

void uefi_print_hex(UINT32 n)
{
    const char *hex = "0123456789ABCDEF";
    char buffer[11];

    buffer[0] = '0';
    buffer[1] = 'x';

    for (int i = 0; i < 8; i++)
    {
        buffer[9 - i] = hex[n & 0xF];
        n >>= 4;
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
        buffer[i++] = (char)('0' + (n % 10));
        n /= 10;
    }

    buffer[i] = '\0';

    for (int j = 0; j < i / 2; j++)
    {
        char tmp = buffer[j];

        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = tmp;
    }

    uefi_print(buffer);
}

void uefi_clear(void)
{
    uefi_call_wrapper(
        g_st->ConOut->ClearScreen,
        1,
        g_st->ConOut
    );
}

void uefi_read_password(char *buffer, int max_len)
{
    int i = 0;
    if (buffer == NULL || max_len <= 0)
        return;

    while (i < max_len - 1)
    {
        EFI_INPUT_KEY key;
        EFI_STATUS status;

        status = uefi_call_wrapper(
            g_bs->WaitForEvent,
            3,
            1,
            &g_st->ConIn->WaitForKey,
            NULL
        );

        if (EFI_ERROR(status))
            continue;

        status = uefi_call_wrapper(
            g_st->ConIn->ReadKeyStroke,
            2,
            g_st->ConIn,
            &key
        );

        if (EFI_ERROR(status))
            continue;

        if (key.UnicodeChar == L'\r')
        {
            uefi_print("\r\n");
            break;
        }

        if (key.UnicodeChar == L'\b')
        {
            if (i > 0)
            {
                i--;
                uefi_print("\b \b");
            }

            continue;
        }

        if (key.UnicodeChar < 32 || key.UnicodeChar > 126)
            continue;

        buffer[i++] = (char)key.UnicodeChar;
        
        uefi_print("*");
    }

    buffer[i] = '\0';
}

void uefi_set_color(UINTN fore_color, UINTN back_color)
{
    uefi_call_wrapper(
        g_st->ConOut->SetAttribute,
        2,
        g_st->ConOut,
        EFI_TEXT_ATTR(fore_color, back_color)
    );
}

void uefi_sleep_ms(UINTN ms)
{
    uefi_call_wrapper(g_bs->Stall, 1, ms * 1000);
}

void uefi_reboot(void)
{
    uefi_call_wrapper(
        g_rs->ResetSystem,
        4,
        EfiResetCold,
        EFI_SUCCESS,
        0,
        NULL
    );

    while (1)
        ;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
    InitializeLib(image, systab);

    g_st = systab;
    g_bs = systab->BootServices;
    g_rs = systab->RuntimeServices;

    Print(L"1: InitializeLib OK\r\n");

    Print(L"2: EFI context assigned\r\n");

    Print(L"3: Before uefi_print\r\n");

    uefi_print("Hello from uefi_print()\n");

    Print(L"4: After uefi_print\r\n");

    Print(L"5: HEX = ");

    uefi_print_hex(0x1234ABCD);

    Print(L"\r\n");

    Print(L"6: DEC = ");

    uefi_print_dec(123456789);

    Print(L"\r\n");

    Print(L"7: Sleeping 1 second...\r\n");

    uefi_sleep_ms(1000);

    Print(L"8: Sleep OK\r\n");

    Print(L"9: Before clear\r\n");

    uefi_clear();

    Print(L"10: After clear\r\n");

    Print(L"11: Before set color\r\n");

    uefi_set_color(EFI_WHITE, EFI_LIGHTRED);

    Print(L"12: Set color OK\r\n");

    char password[64];
    Print(L"13: Enter password: ");
    uefi_read_password(password, sizeof(password));

    Print(L"14: Password input OK\r\n");

    Print(L"15: Finding Block I/O...\r\n");

    EFI_STATUS status = find_block_io(image);
    if (EFI_ERROR(status))
    {
        Print(L"16: Block I/O FAILED\r\n");
        uefi_print_hex((UINT32)status);
        Print(L"\r\n");

        while (1)
            g_bs->Stall(1000000);
    }

    Print(L"16: Block I/O OK\r\n");

    Print(L"17: Media ID = ");
    uefi_print_dec(g_media_id);
    Print(L"\r\n");

    UINT8 sector[512];
    Print(L"18: Before ReadBlocks\r\n");

    int read_result = uefi_read_sector(0, sector);
    if (read_result != 0)
    {
        Print(L"19: ReadBlocks FAILED\r\n");
        uefi_halt();
    }

    Print(L"19: ReadBlocks OK\r\n");

    while (1)
        g_bs->Stall(1000000);

    return EFI_SUCCESS;
}