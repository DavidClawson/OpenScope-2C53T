/*
 * Linker script for OpenScope 2C53T Application Firmware
 * (when used with USB HID bootloader)
 *
 * Flash layout:
 *   0x08000000 - 0x08003FFF  Bootloader (16KB, not touched by app)
 *   0x08004000 - 0x08005FFF  App vector table / boot header
 *   0x08006000 - 0x08006FFF  Stock saved-settings page (preserved)
 *   0x08007000 - 0x080EFFFF  Application code/data
 *
 * RAM: 224KB with EOPB0=0xFE
 * Note: 0x20037FF0 is reserved for bootloader magic word
 */

ENTRY(Reset_Handler)

_estack = 0x20037FE0;    /* leave top 32 bytes for DFU magic at 0x20037FE0 */

_Min_Heap_Size = 0x200;
_Min_Stack_Size = 0x400;

MEMORY
{
  FLASH_VEC (rx)  : ORIGIN = 0x08004000, LENGTH = 8K
  FLASH_APP (rx)  : ORIGIN = 0x08007000, LENGTH = 992K
  RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 224K
  SPIM (rx)       : ORIGIN = 0x08400000, LENGTH = 16384K
}

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
    __isr_vector_end = .;
  } >FLASH_VEC

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP (*(.init))
    KEEP (*(.fini))
    . = ALIGN(4);
    _etext = .;
  } >FLASH_APP

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH_APP

  .ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH_APP
  .ARM : {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH_APP

  .preinit_array :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH_APP
  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH_APP
  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH_APP

  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH_APP

  _spim_init_base = LOADADDR(.spim);
  _spim_init_length = SIZEOF(.spim);

  .spim :
  {
    . = ALIGN(4);
    _spim_start = .;
    *(.spim)
    *(.spim*)
    . = ALIGN(4);
    _spim_end = .;
  } >SPIM

  . = ALIGN(4);
  .bss :
  {
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  /* Preserved across reset — not zeroed by startup code */
  .noinit (NOLOAD) :
  {
    . = ALIGN(4);
    *(.noinit)
    . = ALIGN(4);
  } >RAM

  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}

ASSERT(__isr_vector_end <= ORIGIN(FLASH_VEC) + LENGTH(FLASH_VEC),
       "OpenScope vector table overlaps the stock saved-settings preserve gap")
