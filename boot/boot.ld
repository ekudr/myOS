OUTPUT_ARCH( "riscv" )

ENTRY( _start )

PHDRS
{
    headers PT_PHDR PHDRS;
    text PT_LOAD FILEHDR PHDRS;
    data PT_LOAD;
}

SECTIONS
{
    . = 0x40200000;
    _start = .;

    .text : { *(.text .text.*) } :text
    
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
        *(.pgtables)
        . = ALIGN(4);
    } :data

    /* Stack top */

    .stack_top : {
        . = ALIGN(32);
        _ebss = ABSOLUTE(.);
    } :data

}