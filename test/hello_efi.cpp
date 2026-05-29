// hello_efi.cpp

#include <efi.h>
#include <efilib.h>

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
    InitializeLib(image, systab);

    Print(L"Hello from custom UEFI bootloader!\n");
    Print(L"Press any key to continue...\n");

    WaitForSingleEvent(systab->ConIn->WaitForKey, 0);

    return EFI_SUCCESS;
}