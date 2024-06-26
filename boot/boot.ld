OUTPUT_ARCH( "riscv" )

ENTRY( _start )


OUTPUT_ARCH("riscv")



PHDRS
{
    headers PT_PHDR PHDRS;
    text PT_LOAD FILEHDR PHDRS;
    data PT_LOAD;
}

SECTIONS
{
    . = 0x80200000;
    _start = .;

    .text : { *(.text .text.*)
        . = ALIGN(0x1000);
        _trampoline = .;
        *(trampsec)
        . = ALIGN(0x1000);
        ASSERT(. - _trampoline == 0x1000, "error: trampoline larger than one page");
     } :text
    
    .data :  ALIGN(8)  { 
        *(.data)  
    } :data

    .sdata :ALIGN(8) {
	    __global_pointer$ = . + 0x800;
        *(.sdata .sdata.* .sdata2.*)
    } :data  

    .bss : { 
        _bss_start = .;
        *(.bss) 
        _bss_end = .;
    } :data
    
    .got : { *(.got*) } :data

     /* Page tables here, align to 4K boundary */

    .pgtables (NOLOAD) : ALIGN(0x1000) {
        _pgtable_start = .;
        *(.pgtables)
        . = ALIGN(4);
        _pgtable_end = .;
    } :data

    /* Stack top */

    .stack (NOLOAD): ALIGN(64) {
        _stack_top = .;
        . = . + 0x1000;
        _estack = .;
    } :data

}