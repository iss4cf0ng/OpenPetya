// uefi_io.h

#ifndef UEFI_IO_H
#define UEFI_IO_H

#include <efi.h>
#include <efilib.h>

void uefi_io_init(EFI_BLOCK_IO *bio, EFI_RUNTIME_SERVICES *rs, EFI_BOOT_SERVICES *bs, UINT32 media_id, EFI_SYSTEM_TABLE *systab);
void uefi_halt(void);

/// @brief 
/// @param fore_color 
/// @param back_color 
void uefi_set_color(UINTN fore_color, UINTN back_color);

/// @brief 
/// @param  
void uefi_clear(void);

/// @brief 
/// @param ms 
void uefi_sleep_ms(UINTN ms);

/// @brief 
/// @param  
void uefi_reboot(void);

/// @brief 
/// @param lba 
/// @param buffer 
/// @return 
int uefi_read_sector(UINT32 lba, void *buffer);

/// @brief 
/// @param lba 
/// @param buffer 
/// @return 
int uefi_write_sector(UINT32 lba, const void *buffer);

/// @brief 
/// @param s 
/// @return 
int uefi_print(const char *s);

/// @brief 
/// @param n 
void uefi_print_hex(UINT32 n);

/// @brief 
/// @param n 
void uefi_print_dec(UINT32 n);

/// @brief 
/// @param  
void uefi_reboot(void);

/// @brief 
/// @param buffer 
/// @param max_len 
void uefi_read_password(char *buffer, int max_len);

#endif