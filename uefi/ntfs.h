// ntfs.h

#ifndef NTFS_H
#define NTFS_H

#include <efi.h>
#include <efilib.h>

static EFI_BLOCK_IO *g_blkio = NULL;

EFI_STATUS init_disk(EFI_HANDLE image);
EFI_STATUS ntfs_read(UINT32 lba, UINT32 count, void *buffer);
EFI_STATUS ntfs_write(UINT32 lba, UINT32 count, void *buffer);
EFI_STATUS ntfs_find_first_partitionLBA(void);

#endif