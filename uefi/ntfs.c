// ntfs.c

#include "ntfs.h"

EFI_STATUS init_disk(EFI_HANDLE image)
{
    EFI_STATUS status;
    EFI_HANDLE *handles;
    UINTN count;

    // bind all Block IO devices
    status = uefi_call_wrapper(BS->LocateHandleBuffer, 5, ByProtocol, &BlockIoProtocol, NULL, &count, &handles);
    if (EFI_ERROR(status))
    {
        Print(L"uefi_call_wrapper failed!\n");
        return status;
    }

    for (UINTN i = 0; i < count; i++)
    {
        EFI_BLOCK_IO *blkio;
        status = uefi_call_wrapper(BS->HandleProtocol, 3, handles[i], &BlockIoProtocol, (void **)&blkio);
        if (EFI_ERROR(status))
        {
            Print(L"uefi_call_wrapper failed!\n");
            continue;
        }

        if (blkio->Media->RemovableMedia)
        {
            Print(L"Is removable.\n");
            continue;
        }

        g_blkio = blkio;
        break;
    }

    FreePool(handles);

    return g_blkio ? EFI_SUCCESS : EFI_NOT_FOUND;
}

EFI_STATUS ntfs_read(UINT32 lba, UINT32 count, void *buffer)
{
    EFI_STATUS status = uefi_call_wrapper(
        g_blkio->ReadBlocks,
        5,
        g_blkio,
        g_blkio->Media->MediaId,
        (EFI_LBA)lba,
        (UINTN)(count * 512),
        buffer
    );

    if (EFI_ERROR(status))
    {
        Print(L"disk_read error!\n");
        return status;
    }

    return status;
}

EFI_STATUS ntfs_write(UINT32 lba, UINT32 count, void *buffer)
{
    EFI_STATUS status = uefi_call_wrapper(
        g_blkio->WriteBlocks,
        5,
        g_blkio,
        g_blkio->Media->MediaId,
        (EFI_LBA)lba,
        (UINTN)(count * 512),
        buffer
    );

    uefi_call_wrapper(g_blkio->FlushBlocks, 1, g_blkio);

    return status;
}

int get_mft_lba(uint32_t partition_lba, uint32_t *mft_lba_out)
{
    static uint8_t vbr[512];
    if (ntfs_read(partition_lba, 1, vbr) != 0)
        return -1;

    if (vbr[3] != 'N' || vbr[4] != 'T' || vbr[5] != 'F' || vbr[6] != 'S')
    {
        Print(L"Not NTFS!\n");
        return -1;
    }

    uint8_t sectors_per_cluster = vbr[13];
    uint64_t mft_cluster = 0;
    for (int i = 0; i < 8; i++)
        mft_cluster |= (uint64_t)vbr[i+48] << (i*8);

    *mft_lba_out = (uint32_t)(partition_lba + mft_cluster * sectors_per_cluster);

    return 0;
}

uint32_t ntfs_find_first_partitionLBA(void)
{
    uint8_t mbr_buffer[512];
    EFI_STATUS status = ntfs_read(0, 1, mbr_buffer);
    if (EFI_ERROR(status))
    {
        Print(L"Cannot read MBR");
        return status;
    }

    if (mbr_buffer[510] != 0x55 || mbr_buffer[511] != 0xAA)
    {
        Print(L"Invalid MBR signature!\n");
        return EFI_NOT_READY;
    }

    uint32_t best_lba = 0;
    uint32_t best_size = 0;

    for (int i = 0 ; i < 4; i++)
    {
        uint8_t *entry = mbr_buffer + 0x1BE + i * 16;
        uint8_t type = entry[4];
        uint32_t lba = entry[8] | ((uint32_t)entry[9] << 8) | ((uint32_t)entry[10] << 16) | ((uint32_t)entry[11] << 24);
        uint32_t size = entry[12] | ((uint32_t)entry[13] << 8) | ((uint32_t)entry[14] << 16) | ((uint32_t)entry[15] << 24);

        if (type != 0x07) // 0x07 = NTFS/exFAT
            continue;

        Print(L"\tFound NTFS partition: LBA=%d size=%d\n", lba, size);

        if (size > best_size)
        {
            best_size = size;
            best_lba = lba;
        }
    }

    if (best_lba == 0)
    {
        Print(L"No NTFS partition found!\n");
    }
    else
    {
        Print(L"Selected partition LBA=%d\n\n", best_lba);
    }

    return best_lba;
}
