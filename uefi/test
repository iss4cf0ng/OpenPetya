# test.sh
# Install OVMF (UEFI firmware for QEMU)

# Remove the comment if ovmf has never been installed
# sudo apt install ovmf

# Create a small FAT32 disk image to hold the EFI app
dd if=/dev/zero of=uefi_test.img bs=1M count=64
mkfs.fat -F 32 uefi_test.img

# Mount and copy your EFI app
mkdir -p /tmp/efi_mount
sudo mount uefi_test.img /tmp/efi_mount
sudo mkdir -p /tmp/efi_mount/EFI/BOOT
sudo cp hello_efi.efi /tmp/efi_mount/EFI/BOOT/BOOTX64.EFI
sudo umount /tmp/efi_mount

# Run in QEMU with UEFI
qemu-system-x86_64 \
    -bios /usr/share/ovmf/OVMF.fd \
    -drive file=uefi_test.img,format=raw \
    -m 256M \
    -display gtk