#!/bin/bash
set -e

echo "============================================="
echo "   YOS 64-bit : Generation Release v1.0      "
echo "============================================="

# 1. Nettoyage
echo "[*] Nettoyage des anciens builds..."
make clean || true
rm -rf build user/bin/*.o user/libc/*.o user/hello.elf yos.iso

# 2. Build Userspace & libc
echo "[*] Compilation userspace & libc..."
./build_userspace.sh

# 3. Build Kernel & Objets
echo "[*] Compilation du noyau YOS x86_64..."
make

# 4. Generation ISO Hybride GRUB
echo "[*] Creation de l image ISO finale..."
touch isodir/boot/initrd.tar
make yos.iso

# 5. Verification de l ISO
if [ -f yos.iso ]; then
    SIZE=$(stat -c%s yos.iso 2>/dev/null || stat -f%z yos.iso)
    echo "============================================="
    echo "[+] SUCCESS: yos.iso generee avec succes!"
    echo "[+] Taille de l image : $SIZE octets"
    echo "[+] Architecture : x86_64 Higher-Half Multiboot2"
    echo "[+] Support : BIOS Legacy & UEFI Compatible"
    echo "============================================="
else
    echo "[-] ERREUR: yos.iso introuvable."
    exit 1
fi
