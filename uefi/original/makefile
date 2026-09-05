# ============================================================
# GNU-EFI x86_64 bootloader Makefile
# ============================================================

CC      := gcc
LD      := ld
OBJCOPY := objcopy

TARGET  := bootloader
IMG     := uefi_test.img
MOUNT   := /tmp/efi_mount

# ------------------------------------------------------------
# GNU-EFI
# ------------------------------------------------------------

EFI_PREFIX := /usr/lib

EFI_INCLUDES := \
	-I/usr/include/efi \
	-I/usr/include/efi/x86_64

EFI_LDS  := $(EFI_PREFIX)/elf_x86_64_efi.lds
EFI_CRT  := $(EFI_PREFIX)/crt0-efi-x86_64.o

EFI_LIBS := \
	-L$(EFI_PREFIX) \
	-lefi \
	-lgnuefi

# ------------------------------------------------------------
# Compiler flags
# ------------------------------------------------------------

CFLAGS := \
	$(EFI_INCLUDES) \
	-DEFI_FUNCTION_WRAPPER \
	-ffreestanding \
	-fpic \
	-fno-stack-protector \
	-fno-stack-check \
	-fshort-wchar \
	-mno-red-zone \
	-maccumulate-outgoing-args \
	-Wall \
	-Wextra \
	-O2

# ------------------------------------------------------------
# Linker flags
# ------------------------------------------------------------

LDFLAGS := \
	-nostdlib \
	-znocombreloc \
	-shared \
	-Bsymbolic \
	-T $(EFI_LDS) \
	--defsym=EFI_SUBSYSTEM=10

# ------------------------------------------------------------
# Source files
# ------------------------------------------------------------

SRCS := \
	bootloader.c \
	uefi_io.c \
	ntfs.c \
	ntfs_crypt.c \
	state.c \
	hidden_store.c \
	password_store.c \
	salsa20.c \
	kdf.c

OBJS := $(SRCS:.c=.o)

# ------------------------------------------------------------
# Tools
# ------------------------------------------------------------

QEMU := qemu-system-x86_64
OVMF := /usr/share/ovmf/OVMF.fd

# ============================================================
# Targets
# ============================================================

.PHONY: all build img run run-debug check clean install-deps

all: build

# ------------------------------------------------------------
# Build
# ------------------------------------------------------------

build: $(TARGET).efi
	@echo
	@echo "[+] ========================================"
	@echo "[+] EFI build complete"
	@echo "[+] Output: $(TARGET).efi"
	@echo "[+] ========================================"
	@echo

# ------------------------------------------------------------
# Compile .c -> .o
# ------------------------------------------------------------

%.o: %.c
	@echo "[*] CC $<"
	$(CC) $(CFLAGS) -c $< -o $@

# ------------------------------------------------------------
# Link .o -> ELF
# ------------------------------------------------------------

$(TARGET).so: $(OBJS)
	@echo "[*] Linking EFI ELF..."

	$(LD) \
		$(LDFLAGS) \
		$(EFI_CRT) \
		$(OBJS) \
		-o $@ \
		$(EFI_LIBS)

# ------------------------------------------------------------
# Convert ELF -> EFI PE/COFF
# ------------------------------------------------------------

$(TARGET).efi: $(TARGET).so
	@echo "[*] Converting ELF -> EFI PE/COFF..."

	$(OBJCOPY) \
		-j .text \
		-j .sdata \
		-j .data \
		-j .rodata \
		-j .reloc \
		--target=efi-app-x86_64 \
		--subsystem=10 \
		$< $@

	@echo "[+] EFI binary created: $@"

# ------------------------------------------------------------
# Create test GPT disk image
# ------------------------------------------------------------

img: $(TARGET).efi
	@echo "[*] Creating test disk image..."

	rm -f $(IMG)

	dd if=/dev/zero of=$(IMG) bs=1M count=64 status=none

	parted -s $(IMG) mklabel gpt
	parted -s $(IMG) mkpart ESP fat32 1MiB 63MiB
	parted -s $(IMG) set 1 esp on

	@set -e; \
	LOOP=$$(sudo losetup --find --show --partscan $(IMG)); \
	echo "[*] Loop device: $$LOOP"; \
	trap 'sudo umount $(MOUNT) 2>/dev/null || true; sudo losetup -d $$LOOP 2>/dev/null || true' EXIT; \
	sleep 1; \
	sudo mkfs.fat -F 32 "$${LOOP}p1"; \
	sudo mkdir -p $(MOUNT); \
	sudo mount "$${LOOP}p1" $(MOUNT); \
	sudo mkdir -p $(MOUNT)/EFI/BOOT; \
	sudo cp $(TARGET).efi $(MOUNT)/EFI/BOOT/BOOTX64.EFI; \
	sudo sync; \
	echo "[+] EFI installed at $(MOUNT)/EFI/BOOT/BOOTX64.EFI"; \
	sudo umount $(MOUNT); \
	sudo losetup -d $$LOOP; \
	trap - EXIT; \
	echo "[+] Test ESP created successfully"

# ------------------------------------------------------------
# Run existing image
# ------------------------------------------------------------

run:
	@test -f $(IMG) || \
		(echo "[-] $(IMG) does not exist. Run 'sudo make img' first."; exit 1)

	@echo "[*] Starting QEMU using existing $(IMG)..."

	$(QEMU) \
		-bios $(OVMF) \
		-drive file=$(IMG),format=raw \
		-m 256M \
		-display gtk

# ------------------------------------------------------------
# Run existing image with GDB
# ------------------------------------------------------------

run-debug:
	@test -f $(IMG) || \
		(echo "[-] $(IMG) does not exist. Run 'sudo make img' first."; exit 1)

	@echo "[*] Starting QEMU in debug mode..."
	@echo "[*] QEMU waiting for GDB on localhost:1234"

	$(QEMU) \
		-bios $(OVMF) \
		-drive file=$(IMG),format=raw \
		-m 256M \
		-s \
		-S \
		-display gtk

# ------------------------------------------------------------
# Inspect EFI binary
# ------------------------------------------------------------

check: $(TARGET).efi
	@echo
	@echo "========== file =========="
	file $(TARGET).efi

	@echo
	@echo "========== PE headers =========="
	objdump -p $(TARGET).efi | head -100

	@echo
	@echo "========== sections =========="
	objdump -h $(TARGET).efi

# ------------------------------------------------------------
# Install dependencies
# ------------------------------------------------------------

install-deps:
	sudo apt update
	sudo apt install -y \
		gcc \
		binutils \
		gnu-efi \
		ovmf \
		qemu-system-x86 \
		parted \
		dosfstools \
		util-linux

# ------------------------------------------------------------
# Clean
# ------------------------------------------------------------

clean:
	rm -f \
		$(OBJS) \
		$(TARGET).so \
		$(TARGET).efi \
		$(IMG)

	@echo "[+] Cleaned"