/* Linker script for STM32F103C8T6 */
/* ELF 入口 */
ENTRY(Reset_Handler)

/* 内存布局 */
MEMORY
{
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 64K
  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

/* 代码和数据段 */
SECTIONS
{
  /* 程序入口，存放在 FLASH 起始位置 */
  .isr_vector ORIGIN(FLASH):
  {
    . = ALIGN(4);           /* 确保中断向量表地址四字节对齐 */
    KEEP(*(.isr_vector))    /* 中断向量表 */
  } > FLASH

  /* 程序代码段 */
  .text :
  {
    . = ALIGN(4);           /* 确保代码段地址四字节对齐 */
    *(.text .text.*)        /* 所有 .text 和 .text.* 段 */
    *(.rodata .rodata.*)    /* 所有只读数据段 */
    *(.glue_7) *(.glue_7t)  /* ARM 7 相关的 glue 代码 */
    . = ALIGN(4);           /* 确保代码段结束地址四字节对齐 */
    _etext = .;             /* 定义 _etext 符号，指向代码段结束位置 */
  } > FLASH

  .ARM.exidx :
  {
    . = ALIGN(4);           /*  */
    __exidx_start = .;
    *(.ARM.exidx* .ARM.extab*)
    __exidx_end = .;
    . = ALIGN(4);
  } > FLASH

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data .data.*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT > FLASH

  _sidata = LOADADDR(.data);

  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM

  . = ALIGN(8);
  _end = .;
  end = .;

}