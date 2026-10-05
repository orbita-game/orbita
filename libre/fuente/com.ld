/* Linker script for tiny-model DOS .COM files (code starts at CS:0100h).
   MIT License, Copyright (c) 2026 Órbita. */
OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)
SECTIONS
{
  . = 0x100;
  .text : { *(.text.entry) *(.text .text.*) }
  .rodata : { *(.rodata .rodata.*) }
  .data : { *(.data .data.*) }
  .bss (NOLOAD) : { __bss_start = .; *(.bss .bss.*) *(COMMON) __bss_end = .; }
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
