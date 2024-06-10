#include <stdint.h>


//  JH7110 registers
#define UART	0x10000000	// uart0
#define UART_THR 0x00  		//THR - rtansmitter holding register
#define UART_LSR 0x14  		//LSR - line status register
#define UART_LSR_EMPTY_MASK 0x40 // LSR bit 6 - Transmitter empty; both the THR and LSR are empty


#define ACCESS(x) (*(__typeof__(*x) volatile *)(x))
#define REGW(base, offset) (ACCESS((unsigned  int*)(base + offset)))
#define REGB(base, offset) (ACCESS((unsigned char*)(base + offset)))



void uart_putc(char ch) {
    while ((REGW(UART, UART_LSR) & UART_LSR_EMPTY_MASK) == 0);
    REGW(UART, UART_THR) = ch;
}

void lib_putc(char ch) {
    if (ch == '\n') uart_putc('\r');
    uart_putc(ch);
}

void _putchar(char character){
    if (character == '\n') uart_putc('\r');
    uart_putc(character);   
}

void lib_puts(char *s) {
    while (*s) lib_putc(*s++);

}