// state.h

#ifndef STATE_H
#define STATE_H

#include <efi.h>
#include <efilib.h>

#include "config.h"

typedef struct {
    uint32_t magic;
    uint8_t state;
    uint8_t reserved[3];
    uint64_t disk_total_sectors;
    uint8_t padding[496];
} __attribute__ ((packed)) stBootState;

uint8_t state_read(void);

int state_write(uint8_t new_state);

uint64_t state_read_disk_size(void);

#endif