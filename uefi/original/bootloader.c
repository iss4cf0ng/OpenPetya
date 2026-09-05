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
    uefi_print("SecureBoot, 1st stage.\n\n");
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
    UINT32 partition_lba;
    UINT64 disk_size;
    char input[65] = {0};
    int attempts  = 0;

    uefi_clear();
    uefi_set_color(EFI_WHITE, EFI_BLUE);
    uefi_print("SecureBoot Login\n\n");
    uefi_set_color(EFI_WHITE, EFI_BLACK);

    partition_lba = ntfs_find_first_partitionLBA();
    if (partition_lba == 0)
    {
        uefi_set_color(EFI_RED, EFI_BLACK);
        uefi_print("ERROR: No NTFS partition found!\n");
        uefi_halt();
    }

    while (attempts < 3)
    {
        uefi_print("Password: ");
        uefi_read_password(input, sizeof(input));

        if (ntfs_mft_decrypt(input, partition_lba) == 0) {
            zero_buffer(input, sizeof(input));

            uefi_set_color(EFI_GREEN, EFI_BLACK);
            uefi_print("\nAccess granted!\n");
            uefi_set_color(EFI_WHITE, EFI_BLACK);

            disk_size = state_read_disk_size();
            hidden_store_init(disk_size);

            uefi_print("Restoring MFT...\n");
            if (hidden_restore_mft(partition_lba) != 0) {
                uefi_set_color(EFI_RED, EFI_BLACK);
                uefi_print("ERROR: MFT restore failed!\n");
                uefi_halt();
            }

            UINT8 zero[512] = {0};
            for (int s = 59; s <= 63; s++)
                uefi_write_sector(s, zero);

            uefi_print("Booting Windows...\n");
            uefi_sleep_ms(500);
            return;
        }

        zero_buffer(input, sizeof(input));
        attempts++;
        
        uefi_set_color(EFI_RED, EFI_BLACK);
        uefi_print("Wrong password.\n\n");
        uefi_set_color(EFI_WHITE, EFI_BLACK);
    }

    uefi_print("Too many attempts. Halting.\n");
    uefi_halt();
}

/// @brief main function
/// @param image 
/// @param systab 
/// @return 
EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
    EFI_STATUS status;
    EFI_BOOT_SERVICES  *bs = systab->BootServices;
    EFI_BLOCK_IO *bi = NULL;
    UINT32 media_id = 0;

    InitializeLib(image, systab);

    uefi_io_init(NULL, systab->RuntimeServices, bs, 0, systab);

    // Find our disk by state sector magic
    UINTN handle_count = 0;
    EFI_HANDLE *handles = NULL;
    EFI_GUID bio_guid = EFI_BLOCK_IO_PROTOCOL_GUID;

    status = bs->LocateHandleBuffer(
        ByProtocol, &bio_guid, NULL, &handle_count, &handles);

    if (EFI_ERROR(status)) {
        uefi_print("ERROR: Cannot locate block devices!\n");
        bs->Stall(5000000);
        return status;
    }

    for (UINTN i = 0; i < handle_count; i++) {
        EFI_BLOCK_IO *bio = NULL;
        status = bs->HandleProtocol(handles[i], &bio_guid, (void **)&bio);

        if (EFI_ERROR(status))
            continue;
        if (!bio->Media->MediaPresent)
            continue;
        if (bio->Media->LogicalPartition)
            continue;

        UINT8 sec[512] = {0};
        bio->ReadBlocks(bio, bio->Media->MediaId, STATE_SECTOR, 512, sec);
        if (*(UINT32 *)sec == STATE_MAGIC)
        {
            bi = bio;
            media_id = bio->Media->MediaId;
            break;
        }
    }

    bs->FreePool(handles);

    if (!bi)
    {
        uefi_print("ERROR: Disk not found (no state magic at sector 60)\n");
        bs->Stall(5000000);

        return EFI_NOT_FOUND;
    }

    uefi_io_init(bi, systab->RuntimeServices, bs, media_id, systab);
    uefi_print("Disk found. Reading state...\n");

    UINT8 state_raw[512] = {0};
    uefi_read_sector(STATE_SECTOR, state_raw);
    UINT8 state = state_raw[4];

    if (state == STATE_NOT_SETUP)
        do_encryption();
    else
        do_login();

    EFI_GUID lip_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE *loaded   = NULL;
    bs->HandleProtocol(image, &lip_guid, (void **)&loaded);

    CHAR16 *orig = L"\\EFI\\Microsoft\\Boot\\bootmgfw_original.efi";
    EFI_DEVICE_PATH *dp = FileDevicePath(loaded->DeviceHandle, orig);

    EFI_HANDLE new_image = NULL;
    status = bs->LoadImage(FALSE, image, dp, NULL, 0, &new_image);

    if (EFI_ERROR(status))
    {
        uefi_print("ERROR: Cannot load bootmgfw_original.efi!\n");
        bs->Stall(5000000);
        return status;
    }

    return bs->StartImage(new_image, NULL, NULL);
}