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
    . = ALIGN(4);
    _etext = .;             /* 定义 _etext 符号，指向代码段结束位置 */
  } > FLASH

  /* ARM 异常索引表 */
  .ARM.exidx :
  {
    . = ALIGN(4);           
    __exidx_start = .;          /* 定义 __exidx_start 符号，指向异常索引表开始位置 */
    *(.ARM.exidx* .ARM.extab*)  /* 所有 ARM 异常索引表和扩展表 */
    __exidx_end = .;            /* 定义 __exidx_end 符号，指向异常索引表结束位置 */
    . = ALIGN(4);
  } > FLASH

  /* 数据段，存放在 RAM 中，但在 FLASH 中有初始值 */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;           /* 定义 _sdata 符号，指向数据段开始位置 */
    *(.data .data.*)      /* 所有 .data 和 .data.* 段 */
    . = ALIGN(4);
    _edata = .;           /* 定义 _edata 符号，指向数据段结束位置 */
  } > RAM AT > FLASH

  /* 数据段的初始值，存放在 FLASH 中 */
  _sidata = LOADADDR(.data);

  /* BSS 段，存放在 RAM 中，不需要初始值 */
  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    _sbss = .;        /* 定义 _sbss 符号，指向 BSS 段开始位置 */
    *(.bss .bss.*)    /* 所有 .bss 和 .bss.* 段 */
    *(COMMON)         /* 所有 COMMON 段 */
    . = ALIGN(4);
    _ebss = .;        /* 定义 _ebss 符号，指向 BSS 段结束位置 */
  } > RAM

  . = ALIGN(8);
  _end = .;         /* 定义 _end 符号，指向整个内存布局结束位置 */
  end = .;

}