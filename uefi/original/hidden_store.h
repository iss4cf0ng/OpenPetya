// hidden_store.h

#ifndef HIDDEN_STORE_H
#define HIDDEN_STORE_H

#include <efi.h>
#include <efilib.h>
#include <string.h>

#include "config.h"
#include "uefi_io.h"

typedef struct {
    uint32_t magic;
    uint32_t partition_lba;
    uint32_t mft_lba;
    uint32_t mft_sector_count;
    uint64_t disk_total_sectors;
    uint8_t reserved[484];
} __attribute__((packed)) HiddenHeader;

int hidden_store_init(uint64_t disk_sectors);
int hidden_backup_mft(uint32_t partition_lba);
int hidden_restore_mft(uint32_t partition_lba);
int hidden_wipe(void);
int hidden_read_header(HiddenHeader *hdr);

#endif