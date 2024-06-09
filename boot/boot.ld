OUTPUT_ARCH( "riscv" )

ENTRY( _start )

PHDRS
{
    headers PT_PHDR PHDRS;
    text PT_LOAD FILEHDR PHDRS;
    data PT_LOAD;
    rodata PT_LOAD;
    dynamic PT_DYNAMIC;
}

SECTIONS
{
    . = 0x40200000;
    _start = .;

    .text : { *(.text .text.*) } :text
    .data : { *(.data) } :data
    .bss : { *(.bss) } :data

    .sdata : {
	__global_pointer$ = . + 0x800;
	*(.sdata*)
    } :data
    
}