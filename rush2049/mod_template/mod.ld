/* Mods are loaded above the patches library's RAM. RecompModTool keeps every section it needs; only drop these. */
RAMBASE = 0x81000000;

MEMORY {
    extram(ARWX) : ORIGIN = RAMBASE, LENGTH = 64M
}

SECTIONS {
    /DISCARD/ : { *(.got) *(.MIPS.abiflags) *(.reginfo) *(.pdr) *(.comment) }
}
