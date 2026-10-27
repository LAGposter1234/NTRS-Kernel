#!/bin/bash
set -e
mkdir -p build
nasm -f bin boot.asm -o build/boot.bin
gcc -m64 -ffreestanding -fno-stack-protector -mgeneral-regs-only -fno-builtin -c src/*.c -o build/kernel.o
gcc -m64 -ffreestanding -fno-stack-protector -mgeneral-regs-only -fno-builtin -ffunction-sections -I./nrtsstdlib -c init.c -o build/init.o
ld -m elf_x86_64 -T linkity.ld --oformat binary -e kernel_main build/kernel.o -o build/kernel.bin
ld -m elf_x86_64 -T linkityprogram.ld --oformat binary -e main build/init.o -o disk/bin/sh
stat -c '%s bytes' build/kernel.bin
truncate -s 32768 build/kernel.bin

truncate -s 16M build/fat16.img
mkfs.fat -F 16 -S 512 -s 1 -n SMALLOSDATA build/fat16.img
if [ -d disk ]; then
    mcopy -i build/fat16.img -s disk/* ::/
fi

cat build/boot.bin build/kernel.bin > smallOSNRT.img
qemu-system-x86_64 -drive file=smallOSNRT.img,format=raw -drive file=build/fat16.img,format=raw,if=ide,index=1 -machine pc -audiodev pa,id=audio0 -device sb16,audiodev=audio0 -m 256M
