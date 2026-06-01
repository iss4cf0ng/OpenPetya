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

EFI_STATUS disk_read(UINT32 lba, UINT32 count, void *buffer)
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

EFI_STATUS disk_write(UINT32 lba, UINT32 count, void *buffer)
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
