CC      := gcc
LD      := ld
OBJCOPY := objcopy

EFI_PREFIX := /usr/lib

EFI_INCLUDES := -I/usr/include/efi -I/usr/include/efi/x86_64

EFI_LIBS := -L$(EFI_PREFIX) -lefi -lgnuefi
EFI_LDS  := $(EFI_PREFIX)/elf_x86_64_efi.lds
EFI_CRT  := $(EFI_PREFIX)/crt0-efi-x86_64.o

CFLAGS := $(EFI_INCLUDES) \
          -DEFI_FUNCTION_WRAPPER \
          -fpic \
          -ffreestanding \
          -fno-stack-protector \
          -fno-stack-check \
          -fshort-wchar \
          -mno-red-zone \
          -maccumulate-outgoing-args \
          -Wall -Wextra \
          -O2

LDFLAGS := -nostdlib \
           -znocombreloc \
           -shared -Bsymbolic \
           -T $(EFI_LDS) \
           --defsym=EFI_SUBSYSTEM=10

TARGET := bootloader

SRCS := bootloader.c uefi_io.c ntfs.c ntfs_crypt.c \
        state.c hidden_store.c password_store.c \
        salsa20.c kdf.c

OBJS := $(SRCS:.c=.o)

IMG   := uefi_test.img
MOUNT := /tmp/efi_mount
QEMU  := qemu-system-x86_64
OVMF  := /usr/share/ovmf/OVMF.fd

.PHONY: all build img run run-debug clean install-deps check

all: build

build: $(TARGET).efi
	@echo "[+] Build complete"

%.o: %.c
	@echo "[*] CC $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).so: $(OBJS)
	@echo "[*] Linking EFI ELF..."
	$(LD) $(LDFLAGS) $(EFI_CRT) $(OBJS) -o $@ $(EFI_LIBS)

$(TARGET).efi: $(TARGET).so
	@echo "[*] Converting to EFI binary..."
	$(OBJCOPY) \
		-j .text -j .sdata -j .data \
		-j .dynamic -j .dynsym \
		-j .rel -j .rela -j .reloc \
		--target=efi-app-x86_64 \
		$< $@
	@echo "[+] EFI built: $@"

img: $(TARGET).efi
	@[ "$$(id -u)" = "0" ] || (echo "Run: sudo make img" && exit 1)

	@echo "[*] Creating disk image..."

	dd if=/dev/zero of=$(IMG) bs=1M count=64 2>/dev/null

	parted -s $(IMG) mklabel gpt
	parted -s $(IMG) mkpart ESP fat32 1MiB 63MiB
	parted -s $(IMG) set 1 esp on

	@LOOP=$$(losetup --find --show --partscan $(IMG)); \
	echo "[*] Loop: $$LOOP"; \
	sleep 1; \
	mkfs.fat -F 32 $${LOOP}p1; \
	mkdir -p $(MOUNT); \
	mount $${LOOP}p1 $(MOUNT); \
	mkdir -p $(MOUNT)/EFI/BOOT; \
	cp $(TARGET).efi $(MOUNT)/EFI/BOOT/BOOTX64.EFI; \
	umount $(MOUNT); \
	losetup -d $$LOOP; \
	echo "[+] ESP ready"

run: $(IMG)
	$(QEMU) \
		-bios $(OVMF) \
		-drive file=$(IMG),format=raw \
		-m 256M \
		-display gtk

run-debug: $(IMG)
	$(QEMU) \
		-bios $(OVMF) \
		-drive file=$(IMG),format=raw \
		-m 256M \
		-s -S \
		-display gtk

install-deps:
	sudo apt install -y ovmf gnu-efi parted dosfstools

check: $(TARGET).efi
	file $(TARGET).efi
	objdump -p $(TARGET).efi | head

clean:
	rm -f $(OBJS) $(TARGET).so $(TARGET).efi $(IMG)
	@echo "[+] Cleaned"