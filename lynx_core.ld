/*
 * Atari Lynx (handy-go) — RAM_EMU + ITCM hot code.
 *
 * Hot CPU / Mikey / Susie .text runs from ITCM (zero-wait). Data stays out
 * of ITCM: .rodata/.data/.bss remain in RAM_EMU; session WRAM uses DTCM via
 * dtc_* at runtime (see ram.h / main_lynx.cpp). main_lynx.o stays in
 * RAM_EMU (entry / ABI-facing glue), matching cores/pce/pce_core.ld.
 *
 * Object paths must match BUILD_DIR (Makefile: build/core/…).
 */

INCLUDE gnw_ram_emu.ld
INCLUDE gnw_itcm_core.ld

MEMORY
{
  RAM_EMU (xrw) : ORIGIN = __RAM_EMU_START__, LENGTH = __RAM_EMU_LENGTH__
  ITCM    (xrw) : ORIGIN = __ITCM_CORE_START__, LENGTH = __ITCM_CORE_LENGTH__
}

SECTIONS
{
  .core_entry __RAM_EMU_START__ :
  {
    KEEP(*(.core_entry))
    KEEP(*(.gw_core_bridge_probe))
    . = ALIGN(4);
    /* Exact paths — do not use *system.o (would also match nothing useful
     * here, but mirrors the PCE pitfall with *pce.o vs main_pce.o). */
    EXCLUDE_FILE(
      build/core/system.o
      build/core/mikie.o
      build/core/susie.o
    ) *(.text*)
    *(.rodata*)
    *(.glue_7)
    *(.glue_7t)

    . = ALIGN(4);
    __CORE_INIT_ARRAY_START__ = .;
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    __CORE_INIT_ARRAY_END__ = .;
  } >RAM_EMU

  .data :
  {
    *(.data*)
  } >RAM_EMU

  __CORE_CODE_END__ = .;

  .bss (NOLOAD) :
  {
    . = ALIGN(8);
    *(.bss*)
    *(COMMON)
    . = ALIGN(8);
  } >RAM_EMU

  __CORE_BSS_END__ = .;

  ASSERT(__CORE_BSS_END__ <= __RAM_EMU_END__,
         "Error: Lynx core RAM_EMU segment (code+data+bss) overflows RAM_EMU")

  /* ITCM: hot Handy engines only — code, no data/bss. */
  .core_itcm __ITCM_CORE_START__ :
  {
    __CORE_ITCM_CODE_START__ = .;
    build/core/system.o(.text*)
    build/core/mikie.o(.text*)
    build/core/susie.o(.text*)
    . = ALIGN(4);
    __CORE_ITCM_CODE_END__ = .;
  } >ITCM AT>RAM_EMU

  __CORE_ITCM_BSS_END__ = __CORE_ITCM_CODE_END__;

  ASSERT(__CORE_ITCM_BSS_END__ <= __ITCM_CORE_START__ + __ITCM_CORE_LENGTH__,
         "Error: Lynx core ITCM segment overflows the 64K ITCM region")

  __exidx_start = .;
  __exidx_end = .;

  /DISCARD/ :
  {
    *(.fini_array*)
    *(.preinit_array*)
    *(.ARM.exidx*)
    *(.ARM.extab*)
    *(.comment)
    *(.ARM.attributes)
  }
}
