# build.sh

gcc -I/usr/include/efi \
    -I/usr/include/efi/x86_64 \
    -fpic -ffreestanding -fno-stack-protector \
    -fno-stack-check -fshort-wchar -mno-red-zone \
    -maccumulate-outgoing-args \
    -c hello_efi.c -o hello_efi.o

ld -nostdlib -znocombreloc \
    -shared -Bsymbolic \
    -T /usr/lib/elf_x86_64_efi.lds \
    /usr/lib/crt0-efi-x86_64.o \
    hello_efi.o \
    -o hello_efi.so \
    -L/usr/lib -lefi -lgnuefi \
    --defsym=EFI_SUBSYSTEM=10

objcopy -j .text -j .sdata -j .data -j .dynamic \
        -j .dynsym -j .rel -j .rela -j .reloc \
        --target=efi-app-x86_64 \
        hello_efi.so hello_efi.efi