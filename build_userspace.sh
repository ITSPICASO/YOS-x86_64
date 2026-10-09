#!/bin/bash
set -e

CFLAGS="-target x86_64-elf -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -mno-red-zone -Wall -fno-pic -fno-pie -mcmodel=small -Iuser/libc/include"

echo "[*] Compilation de la libc userspace..."
clang $CFLAGS -c user/libc/src/libc.c -o user/libc/libc.o
nasm -f elf64 user/libc/src/crt0.s -o user/libc/crt0.o

echo "[*] Compilation de sh.c..."
clang $CFLAGS -c user/bin/sh.c -o user/bin/sh.o

echo "[*] Link statique de hello.elf (base 0x400000)..."
ld.lld -m elf_x86_64 --image-base=0x400000 --entry=_start user/libc/crt0.o user/bin/sh.o user/libc/libc.o -o user/hello.elf

echo "[*] Mise a jour du ramdisk initrd.tar..."
mkdir -p /tmp/yos_tar_staging
cd /tmp/yos_tar_staging
tar -xf /mnt/d/os/YOS-x86_64/isodir/boot/initrd.tar
cp /mnt/d/os/YOS-x86_64/user/hello.elf ./hello.elf
tar -cf /mnt/d/os/YOS-x86_64/isodir/boot/initrd.tar *
cd /mnt/d/os/YOS-x86_64
rm -rf /tmp/yos_tar_staging

echo "[+] Userspace C & Shell prets!"
