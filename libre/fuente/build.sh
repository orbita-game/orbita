#!/bin/sh
# Builds BLOQUES.COM and MONOS.COM with the host gcc (-m16) and binutils.
# MIT License, Copyright (c) 2026 Órbita.
set -e
cd "$(dirname "$0")"
CFLAGS="-m16 -march=i386 -ffreestanding -nostdlib -fno-pic -fno-pie -Os \
 -fno-asynchronous-unwind-tables -fno-delete-null-pointer-checks \
 -fno-tree-loop-distribute-patterns -fno-stack-protector -Wall -Wno-unused-function"
for g in bloques monos; do
  [ -f $g.c ] || continue
  gcc $CFLAGS -c $g.c -o $g.o
  ld -m elf_i386 --no-warn-rwx-segments -T com.ld -o $g.elf $g.o
  objcopy -O binary -j .text -j .rodata -j .data $g.elf $(echo $g | tr a-z A-Z).COM
  end=$(nm $g.elf | awk '/ __bss_end$/{print $1}')
  # code + data + bss must leave room for the stack below FFFEh
  if [ $((0x$end)) -gt $((0xE000)) ]; then echo "$g: too big (bss end 0x$end)"; exit 1; fi
  echo "$(echo $g | tr a-z A-Z).COM: $(wc -c < $(echo $g | tr a-z A-Z).COM) bytes, memory end 0x$end"
  rm -f $g.o $g.elf
done
