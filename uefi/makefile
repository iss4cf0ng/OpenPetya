# Makefile for UEFI bootloader

CC      := gcc
LD      := ld
OBJCOPY := objcopy

EFI_INCLUDES := -I/usr/include/efi -I/usr/include/efi/x86_64
EFI_LIBS     := -L/usr/lib -lefi -lgnuefi
EFI_LDS      := /usr/lib/elf_x86_64_efi.lds
EFI_CRT      := /usr/lib/crt0-efi-x86_64.o

CFLAGS := $(EFI_INCLUDES) \
          -fpic -ffreestanding -fno-stack-protector \
          -fno-stack-check -fshort-wchar -mno-red-zone \
          -maccumulate-outgoing-args

LDFLAGS := -nostdlib -znocombreloc \
           -shared -Bsymbolic \
           -T $(EFI_LDS) \
           --defsym=EFI_SUBSYSTEM=10

TARGET  := bootloader
SRCS    := bootloader.c ntfs.c salsa20.c kdf.c
OBJS    := $(SRCS:.c=.o)

IMG     := uefi_test.img
MOUNT   := /tmp/efi_mount
QEMU    := qemu-system-x86_64
OVMF    := /usr/share/ovmf/OVMF.fd

.PHONY: all build img clean run install-deps

all: build
	@echo "[+] Build complete. Run: sudo make img && make run"

build: $(TARGET).efi

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).so: $(OBJS)
	$(LD) $(LDFLAGS) $(EFI_CRT) $^ -o $@ $(EFI_LIBS)

$(TARGET).efi: $(TARGET).so
	$(OBJCOPY) \
		-j .text -j .sdata -j .data -j .dynamic \
		-j .dynsym -j .rel -j .rela -j .reloc \
		--target=efi-app-x86_64 \
		$< $@
	@echo "[+] Built: $@"

img: $(TARGET).efi
	@[ "$$(id -u)" = "0" ] || (echo "[-] Run: sudo make img" && exit 1)
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
	echo "[+] ESP ready."

	@python3 -c "\
	import struct,sys; \
	s=bytearray(512); \
	struct.pack_into('<I',s,0,0x424F4F54); \
	s[4]=0x00; \
	struct.pack_into('<Q',s,8,64*1024*2); \
	sys.stdout.buffer.write(bytes(s))" \
	| dd of=$(IMG) seek=60 bs=512 count=1 conv=notrunc 2>/dev/null

	@echo "[+] Disk image ready: $(IMG)"

run: $(IMG)
	$(QEMU) \
		-bios $(OVMF) \
		-drive file=$(IMG),format=raw \
		-m 256M \
		-display gtk \
		-serial null \
		-monitor none

install-deps:
	sudo apt install -y ovmf gnu-efi parted

clean:
	rm -f $(OBJS) $(TARGET).so $(TARGET).efi $(IMG)
	@echo "[+] Cleaned."