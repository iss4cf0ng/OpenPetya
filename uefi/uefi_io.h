// uefi_io.h

#ifndef UEFI_IO_H
#define UEFI_IO_H

#include <efi.h>
#include <efilib.h>

void uefi_io_init(EFI_BLOCK_IO *bio, UINT32 media_id, EFI_SYSTEM_TABLE *systab);
void uefi_halt(void);
void uefi_set_color(UINTN fg, UINTN bg);
void uefi_clear(void);
void uefi_sleep_ms(UINTN ms);
void uefi_reboot(void);

int uefi_read_sector(UINT32 lba, void *buffer);
int uefi_write_sector(UINT32 lba, const void *buffer);
int uefi_print(const char *s);
void uefi_print_hex(UINT32 n);
void uefi_print_dec(UINT32 n);

#endif