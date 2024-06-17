#include <stdint.h>
#include <spinlock.h>
#include <jh7110_memmap.h>

// the UART control registers.
// some have different meanings for
// read vs write.
// see http://byterunner.com/16550.html
#define UART_RHR 0                 // receive holding register (for input bytes)
#define UART_THR 0                 // transmit holding register (for output bytes)
#define UART_IER 1                 // interrupt enable register
#define UART_IER_RX_ENABLE (1<<0)
#define UART_IER_TX_ENABLE (1<<1)
#define UART_FCR 2                 // FIFO control register
#define UART_FCR_FIFO_ENABLE (1<<0)
#define UART_FCR_FIFO_CLEAR (3<<1) // clear the content of the two FIFOs
#define UART_ISR 2                 // interrupt status register
#define UART_LCR 3                 // line control register
#define UART_LCR_EIGHT_BITS (3<<0)
#define UART_LCR_BAUD_LATCH (1<<7) // special mode to set baud rate
#define UART_LSR 5                 // line status register
#define UART_LSR_RX_READY (1<<0)   // input is waiting to be read from RHR
#define UART_LSR_TX_IDLE (1<<5)    // THR can accept another character to send



#define UART_LSR_EMPTY_MASK 0x40 // LSR bit 6 - Transmitter empty; both the THR and LSR are empty


#define ACCESS(x) (*(__typeof__(*x) volatile *)(x))
#define REGW(base, offset) (ACCESS((unsigned  int*)(base + (offset << UART0_REG_SHIFT))))
#define REGB(base, offset) (ACCESS((unsigned char*)(base + (offset << UART0_REG_SHIFT))))


// the transmit output buffer.
struct spinlock uart_tx_lock;

void uart_init(void) {
  // disable interrupts.
  REGB(UART0, UART_IER) = 0x00;

  // special mode to set baud rate.
  REGB(UART0, UART_LCR) = UART_LCR_BAUD_LATCH;

  // LSB for baud rate of 38.4K.
  REGB(UART0, 0) = 0x03;

  // MSB for baud rate of 38.4K.
  REGB(UART0, 1) = 0x00;

  // leave set-baud mode,
  // and set word length to 8 bits, no parity.
  REGB(UART0, UART_LCR) = UART_LCR_EIGHT_BITS;

  // reset and enable FIFOs.
  REGB(UART0, UART_FCR) = UART_FCR_FIFO_ENABLE | UART_FCR_FIFO_CLEAR;

  // enable transmit and receive interrupts.
  REGB(UART0, UART_IER) = UART_IER_TX_ENABLE | UART_IER_RX_ENABLE;

  initlock(&uart_tx_lock, "uart");
}


void uart_putc(char ch) {
    acquire(&uart_tx_lock);
    while ((REGW(UART0, UART_LSR) & UART_LSR_EMPTY_MASK) == 0);
    REGW(UART0, UART_THR) = ch;
    release(&uart_tx_lock);
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