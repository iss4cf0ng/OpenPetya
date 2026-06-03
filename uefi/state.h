// state.h

#ifndef STATE_H
#define STATE_H

#include <efi.h>
#include <efilib.h>

#define STATE_SECTOR 60
#define STATE_MAGIC 0x424F4F54UL

#define STATE_NOT_SETUP 0x00
#define STATE_ENCRYPTED 0x01

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