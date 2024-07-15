OUTPUT_ARCH( "riscv" )

ENTRY( _start )

OUTPUT_ARCH("riscv")


SECTIONS
{
    . = 0x10000000;
    _start = .;

    .text : { *(.text .text.*)
        . = ALIGN(0x1000);
        _trampoline = .;
        *(trampsec)
        . = ALIGN(0x1000);
        ASSERT(. - _trampoline == 0x1000, "error: trampoline larger than one page");
     }
    
    .data :  ALIGN(8)  { 
        *(.data)  
    }

    .sdata :ALIGN(8) {
	    __global_pointer$ = . + 0x800;
        *(.sdata .sdata.* .sdata2.*)
    } 

    .bss : ALIGN(8) { 
        _bss_start = .;
        *(.bss) 
        _bss_end = .;
    }
    
    .got : ALIGN(8) { *(.got*) }

     /* Page tables here, align to 4K boundary */

    .pgtables (NOLOAD) : ALIGN(0x1000) {
        _pgtable_start = .;
        *(.pgtables)
        . = ALIGN(4);
        _pgtable_end = .;
    }

    /* Stack top */

    .stack (NOLOAD): ALIGN(64) {
        _stack_top = .;
        . = . + 0x1000;
        _estack = .;
    }

    /DISCARD/ : { *(.comment .note .eh_frame) }

}