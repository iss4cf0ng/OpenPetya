// state.c

#include "state.h"
#include "ntfs.h"

static uint8_t buffer[512];

uint8_t state_read(void)
{
    if (ntfs_read(STATE_SECTOR, 1, buffer) != 0)
        return STATE_NOT_SETUP;

    stBootState *state = (stBootState *)buffer;
    if (state->magic != STATE_MAGIC)
        return STATE_NOT_SETUP;

    return state->state;
}

int state_write(uint8_t new_state)
{
    static uint8_t buffer[512];

    if (ntfs_read(STATE_SECTOR, 1, buffer) != 0)
    {
        Print(L"state_write: read failed!\n");
        return -1;
    }

    stBootState *state = (stBootState *)buffer;

    Print(L"state_write: before write, disk_sectors=%d\n", (uint32_t)state->disk_total_sectors);

    state->magic = STATE_MAGIC;
    state->state = new_state;

    if (ntfs_write(STATE_SECTOR, 1, buffer) != 0)
    {
        Print(L"state_write: write failed\n");
        return -1;
    }

    // verify the write
    static uint8_t verify[512];
    if (ntfs_read(STATE_SECTOR, 1, verify) != 0)
    {
        Print(L"state_write: verify read failed\n");
        return -1;
    }

    stBootState *ver = (stBootState *)verify;
    Print(L"state_write verify: magic=0x%x state=0x%x\n", ver->magic, ver->state);

    if (ver->magic != STATE_MAGIC || ver->state != new_state)
    {
        Print(L"state_write: verify MISMATCH!\n");
        return -1;
    }

    return 0;
}

uint64_t state_read_disk_size(void)
{
    if (ntfs_read(STATE_SECTOR, 1, buffer) != 0)
    {
        Print(L"NTFS read FAILED!\n");
        return 0;
    }

    stBootState *state = (stBootState *)buffer;

    Print(L"ntfs_read OK, magic=0x%x disk_sectors=%d\n", state->magic, (uint32_t)state->disk_total_sectors);

    if (state->magic != STATE_MAGIC)
        return 0;

    return state->disk_total_sectors;
}
