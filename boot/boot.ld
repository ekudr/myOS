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
}