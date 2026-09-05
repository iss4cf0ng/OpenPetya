CC      := gcc
LD      := ld
OBJCOPY := objcopy

EFI_PREFIX := /usr/lib

EFI_INCLUDES := \
	-I/usr/include/efi \
	-I/usr/include/efi/x86_64

EFI_CRT := $(EFI_PREFIX)/crt0-efi-x86_64.o
EFI_LDS := $(EFI_PREFIX)/elf_x86_64_efi.lds

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

LDFLAGS := \
	-nostdlib \
	-znocombreloc \
	-shared \
	-Bsymbolic \
	-T $(EFI_LDS) \
	--defsym=EFI_SUBSYSTEM=10

all: petya.efi

petya.o: petya.c
	$(CC) $(CFLAGS) -c petya.c -o petya.o

petya.so: petya.o
	$(LD) \
		$(LDFLAGS) \
		$(EFI_CRT) \
		petya.o \
		-o petya.so \
		/usr/lib/libgnuefi.a \
		/usr/lib/libefi.a

petya.efi: petya.so
	$(OBJCOPY) \
		-j .text \
		-j .sdata \
		-j .data \
		-j .rodata \
		-j .reloc \
		--target=efi-app-x86_64 \
		petya.so \
		petya.efi

clean:
	rm -f petya.o petya.so petya.efi