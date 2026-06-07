NASM := nasm
CC   := gcc
LD   := ld
QEMU := qemu-system-i386

MBR_DIR := mbr
BL_DIR  := bootloader
EFI_DIR := uefi
BUILD   := build

MBR_BIN  := $(BUILD)/mbr.bin
STAGE2   := $(BUILD)/stage2.bin
DISK_IMG := $(BUILD)/disk.img

SRCS     := $(wildcard $(BL_DIR)/*.c)
OBJS     := $(patsubst $(BL_DIR)/%.c, $(BUILD)/%.o, $(SRCS))
ALL_OBJS := $(BUILD)/entry.o $(OBJS)

DISK_SIZE_MB := 200
PART_START   := 1MiB
PART_END     := 199MiB

CFLAGS  := -m32 -ffreestanding -nostdlib -nostartfiles -fno-stack-protector -fno-pic -O2 -Wall -Wextra
LDFLAGS := -m elf_i386 -T $(BL_DIR)/linker.ld

.PHONY: all build img clean run run-debug dirs img-info write-drive backup-mbr restore-mbr

all: build
	@echo ""
	@echo "[+] Build completed successfully."
	@echo "[*] Next steps:"
	@echo "\tsudo make img                     -> Create NTFS disk image"
	@echo "\tmake run                          -> Launch QEMU test environment"
	@echo "\tmake write-drive DRIVE=/dev/sdX   -> Deploy bootloader to physical drive"

# -----------------------------------------------------------------------------
# Core Build Pipeline (No sudo required)
# -----------------------------------------------------------------------------
build: dirs $(MBR_BIN) $(STAGE2)
	@echo "[+] Bootloader build pipeline finished."

dirs:
	@mkdir -p $(BUILD)
	@echo "[*] Created build workspace: $(BUILD)"

$(MBR_BIN): $(MBR_DIR)/mbr.asm
	@echo "[*] Assembling MBR stage..."
	$(NASM) -f bin -o $@ $<
	@[ $$(wc -c < $@) -eq 512 ] && echo "[+] Valid MBR generated (512 bytes)." || (echo "[-] MBR size validation failed!" && exit 1)

$(BUILD)/entry.o: $(BL_DIR)/entry.asm
	@echo "[*] Assembling Stage2 entry point..."
	$(NASM) -f elf32 -o $@ $<

$(BUILD)/%.o: $(BL_DIR)/%.c
	@echo "[*] Compiling $< ..."
	$(CC) $(CFLAGS) -c -o $@ $<

$(STAGE2): $(ALL_OBJS)
	@echo "[*] Linking Stage2 payload..."
	$(LD) -m elf_i386 -T $(BL_DIR)/linker.ld -o $@ --oformat binary $^

	@size=$$(stat -c%s $@); \
	if [ $$size -lt 20480 ]; then \
		echo "[*] Padding Stage2 to 8192 bytes..."; \
		dd if=/dev/zero bs=1 count=$$((20480-$$size)) >> $@ 2>/dev/null; \
	fi

	@echo "[+] Stage2 payload ready: $$(stat -c%s $@) bytes"

# =============================================================================
# Disk Image Generation (Requires sudo)
# =============================================================================
img: $(MBR_BIN) $(STAGE2)
	@[ "$$(id -u)" = "0" ] || (echo "[-] Root privileges required. Use: sudo make img" && exit 1)

	@echo ""
	@echo "[*] Initializing NTFS disk image build sequence..."

	@echo "[*] [1/5] Allocating $(DISK_SIZE_MB)MB raw disk image..."
	dd if=/dev/zero of=$(DISK_IMG) bs=1M count=$(DISK_SIZE_MB) 2>/dev/null

	@echo "[*] [2/5] Writing MBR partition table..."
	parted -s $(DISK_IMG) mklabel msdos
	parted -s $(DISK_IMG) mkpart primary ntfs $(PART_START) $(PART_END)

	@echo "[*] [3/5] Formatting NTFS partition..."
	@LOOP=$$(losetup --find --show --partscan $(DISK_IMG)); \
	echo "[*] Attached loop device: $$LOOP"; \
	sleep 1; \
	mkfs.ntfs -Q -L "BOOTDISK" $${LOOP}p1; \
	EXIT=$$?; \
	losetup -d $$LOOP; \
	[ $$EXIT -eq 0 ] && echo "[+] NTFS filesystem created successfully." || (echo "[-] NTFS formatting failed!" && exit 1)

	@echo "[*] [4/5] Injecting MBR boot code..."
	dd if=$(MBR_BIN) of=$(DISK_IMG) bs=446 count=1 conv=notrunc 2>/dev/null

	@echo "[*] [5/5] Deploying Stage2 loader to sector 1..."
	dd if=$(STAGE2) of=$(DISK_IMG) seek=1 bs=512 conv=notrunc 2>/dev/null

	@echo "[*] Writing state sector (sector 60) for QEMU..."
	python3 -c "import struct,sys; s=bytearray(512); struct.pack_into('<I',s,0,0x424F4F54); s[4]=0x00; struct.pack_into('<Q',s,8,200*1024*2); sys.stdout.buffer.write(bytes(s))" | dd of=$(DISK_IMG) seek=60 bs=512 count=1 conv=notrunc 2>/dev/null

	@echo "[*] Writing password sector (sector 59) for QEMU testing..."
	python3 -c "import struct,sys; s=bytearray(512); struct.pack_into('<I',s,0,0x50415353); pw=b'123456'; s[4]=len(pw); s[5:5+len(pw)]=pw; sys.stdout.buffer.write(bytes(s))" | dd of=$(DISK_IMG) seek=59 bs=512 count=1 conv=notrunc 2>/dev/null

	@echo ""
	@echo "[+] Disk image successfully generated: $(DISK_IMG)"
	@parted -s $(DISK_IMG) print

# =============================================================================
# QEMU Execution
# =============================================================================
run: $(DISK_IMG)
	@echo "[*] Launching QEMU virtual machine..."
	$(QEMU) -drive file=$(DISK_IMG),format=raw,if=ide -m 128M -no-reboot -no-shutdown -display gtk -serial stdio

run-debug: $(DISK_IMG)
	@echo "[*] Launching QEMU in debug mode..."
	$(QEMU) -drive file=$(DISK_IMG),format=raw,if=ide -m 128M -no-reboot -no-shutdown -display gtk -serial stdio -s -S

# =============================================================================
# Physical Drive Deployment
# Usage: make write-drive DRIVE=/dev/sdb
# =============================================================================
write-drive: $(MBR_BIN) $(STAGE2)
ifndef DRIVE
	@echo ""
	@echo "[-] ERROR: No target drive specified."
	@echo "[*] Example usage: make write-drive DRIVE=/dev/sdb"
	@echo ""
	@echo "[*] Available block devices:"
	@lsblk -d -o NAME,SIZE,MODEL | grep -v loop
	@exit 1
endif

	@echo ""
	@echo "[*] Preparing deployment to $(DRIVE)..."
	@echo ""
	@echo "[*] Current partition layout:"
	@sudo parted $(DRIVE) print

	@echo ""
	@echo "[-] WARNING:"
	@echo "\tThis operation overwrites:"
	@echo "\t\t- MBR boot code (first 446 bytes)"
	@echo "\t\t- Stage2 loader (sector 1)"
	@echo ""
	@echo "[+] Existing NTFS partitions and Windows data remain untouched."
	@echo ""

	@read -p "[?] Confirm deployment to $(DRIVE)? (yes/no): " ans; [ "$$ans" = "yes" ] || (echo "[-] Deployment aborted." && exit 1)

	@echo ""
	@echo "[*] [1/2] Writing MBR boot code..."
	sudo dd if=$(MBR_BIN) of=$(DRIVE) bs=446 count=1 conv=notrunc
	sudo sync

	@echo "[*] [2/2] Writing Stage2 payload..."
	sudo dd if=$(STAGE2) of=$(DRIVE) seek=1 bs=512 conv=notrunc
	sudo sync

	@echo ""
	@echo "[+] Deployment completed successfully."

	@echo "[*] Verifying MBR signature (expected: 55 aa)..."
	@sudo xxd -s 510 -l 2 $(DRIVE)

	@echo "[*] Verifying partition table integrity..."
	@sudo parted $(DRIVE) print

# =============================================================================
# MBR Backup / Restore
# =============================================================================
backup-mbr:
ifndef DRIVE
	@echo "[-] Usage: make backup-mbr DRIVE=/dev/sdb"
	@exit 1
endif
	@echo "[*] Backing up MBR from $(DRIVE)..."
	sudo dd if=$(DRIVE) of=$(BUILD)/original_mbr.bin bs=512 count=1
	@echo "[+] Backup saved to: $(BUILD)/original_mbr.bin"

restore-mbr:
ifndef DRIVE
	@echo "[-] Usage: make restore-mbr DRIVE=/dev/sdb"
	@exit 1
endif
	@[ -f $(BUILD)/original_mbr.bin ] || (echo "[-] Backup file not found! Run backup-mbr first." && exit 1)

	@echo "[*] Restoring MBR to $(DRIVE)..."
	sudo dd if=$(BUILD)/original_mbr.bin of=$(DRIVE) bs=512 count=1 conv=notrunc
	sudo sync
	@echo "[+] MBR restoration completed."

# =============================================================================
# Disk Image Diagnostics
# =============================================================================
img-info: $(DISK_IMG)
	@echo "[*] Inspecting disk image metadata..."
	@echo "================================================="
	@parted -s $(DISK_IMG) print

	@echo ""
	@echo "[*] MBR signature (offset 510):"
	@xxd -s 510 -l 2 $(DISK_IMG)

	@echo ""
	@echo "[*] Stage2 header (sector 1, first 16 bytes):"
	@xxd -s 512 -l 16 $(DISK_IMG)

clean:
	@echo "[*] Cleaning build artifacts..."
	rm -rf $(BUILD)
	@echo "[+] Workspace cleaned."
