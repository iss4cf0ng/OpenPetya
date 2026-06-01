// uefi_compat.h

#ifndef UEFI_COMPAT_H
#define UEFI_COMPAT_H

#include <efi.h>
#include <efilib.h>

static inline void vga_puts(const char *s)
{
    CHAR16 buffer[256];
    int i = 0;
    while (*s && i < 255)
        buffer[i++] = (CHAR16)*s++;
    
    buffer[i] = 0;
    Print(L"%s", buffer);
}

static inline void vga_putchar(char c)
{
    
}

#endif