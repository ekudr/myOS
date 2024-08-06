
#include <common.h>
#include <spinlock.h>
#include <sbi_ecall_interface.h>
#include <sched.h>

// the UART control registers.
// some have different meanings for
// read vs write.
// see http://byterunner.com/16550.html
#define UART_RHR 0                 // receive holding register (for input bytes)
#define UART_THR 0                 // transmit holding register (for output bytes)
#define UART_IER 1                 // interrupt enable register
#define UART_IER_RX_ENABLE (1<<0)
#define UART_IER_TX_ENABLE (1<<1)
#define UART_IER_UNIT_ENABLE (1<<6)  // Unit Enable bit
#define UART_FCR 2                 // FIFO control register
#define UART_FCR_FIFO_ENABLE (1<<0)
#define UART_FCR_FIFO_CLEAR (3<<1) // clear the content of the two FIFOs
#define UART_ISR 2                 // interrupt status register
#define UART_ISR_INT_PEN  (1<<0)    // interrupt pending
#define UART_LCR 3                 // line control register
#define UART_LCR_EIGHT_BITS (3<<0)
#define UART_LCR_BAUD_LATCH (1<<7) // special mode to set baud rate
#define UART_MSR 4                 // Modem Control Register
#define UART_MSR_OUT2 (1<<3)
#define UART_LSR 5                 // line status register
#define UART_LSR_RX_READY (1<<0)   // input is waiting to be read from RHR
#define UART_LSR_TX_IDLE (1<<5)    // THR can accept another character to send


#define UART_LSR_BUF_EMPTY_MASK 0x20  // LSR bit 5 - Transmit Buffer Empty; the UART sent data from the THR to the OSR
#define UART_LSR_EMPTY_MASK 0x40 // LSR bit 6 - Transmitter empty; both the THR and LSR are empty


#ifndef UART_INIT_IER
#define UART_INIT_IER 0x00
#endif


#define ACCESS(x) (*(__typeof__(*x) volatile *)(x))
#define REGW(base, offset) (ACCESS((unsigned  int*)(base + (offset << UART0_REG_SHIFT))))
#define REGB(base, offset) (ACCESS((unsigned char*)(base + (offset << UART0_REG_SHIFT))))


#define UART_TX_BUF_SIZE 32
char uart_tx_buf[UART_TX_BUF_SIZE];
uint64_t uart_tx_w; // write next to uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE]
uint64_t uart_tx_r; // read next from uart_tx_buf[uart_tx_r % UART_TX_BUF_SIZE]

// the transmit output buffer.
struct spinlock uart_tx_lock;

uint64_t uart_inited;

void uart_start();

void ns16550_uart_init(void) {

DEBUG("[UART] MSR reg 0x%X\n", REGB(UART0, UART_MSR));

  // wait transmitter empty
  while ((REGW(UART0, UART_LSR) & UART_LSR_EMPTY_MASK) == 0);

  // disable interrupts.
  REGB(UART0, UART_IER) = 0x00;



  // and set word length to 8 bits, no parity.
  REGB(UART0, UART_LCR) = UART_LCR_EIGHT_BITS;

  // special mode to set baud rate.
  REGB(UART0, UART_LCR) = UART_LCR_BAUD_LATCH;

  // LSB for baud rate of 115.2K.
  REGB(UART0, 0) = UART0_DIV;

  // MSB for baud rate of 115.2K.
  REGB(UART0, 1) = 0x00;

  // leave set-baud mode,
  // and set word length to 8 bits, no parity.
  REGB(UART0, UART_LCR) = UART_LCR_EIGHT_BITS;

#if defined(__SPACEMIT_K1__) 
  // Enabling interrupts
  // OUT2 Signal Control.
  // OUT2 connects the UART interrupt output to the interrupt controller unit. When <Loopback Mode> is clear.
  REGB(UART0, UART_MSR) |= UART_MSR_OUT2;
#endif
  // reset and enable FIFOs.
  REGB(UART0, UART_FCR) = UART_FCR_FIFO_ENABLE;
  REGB(UART0, UART_FCR) = UART_FCR_FIFO_ENABLE | UART_FCR_FIFO_CLEAR;

	/*
	 * Clear the interrupt registers.
	 */
	(void) REGB(UART0, UART_LSR);
	(void) REGB(UART0, UART_RHR);
	(void) REGB(UART0, UART_ISR);
	(void) REGB(UART0, UART_MSR);

  // enable unit.
  REGB(UART0, UART_IER) = UART_INIT_IER;

    // enable transmit and receive interrupts.
  REGB(UART0, UART_IER) = REGB(UART0, UART_IER) | /*UART_IER_TX_ENABLE |*/ UART_IER_RX_ENABLE;

  initlock(&uart_tx_lock, "uart");

	/*
	 * Clear the interrupt registers.
	 */
	(void) REGB(UART0, UART_LSR);
	(void) REGB(UART0, UART_RHR);
	(void) REGB(UART0, UART_ISR);
	(void) REGB(UART0, UART_MSR);

// printf("[UART] irq reg 0x%X", REGB(UART0, UART_IER) );
  uart_inited = 0x55555555;
}

void sbi_console_putc(char ch);


void uart_init(void) {
#ifdef __SIFIVE_U__   
    sifive_uart_init();
    uart_inited = 0x55555555;
#endif   
#if (defined(__JH7110__) || defined(__SPACEMIT_K1__))
    ns16550_uart_init();
    uart_inited = 0x55555555;
#endif 
}

// alternate version of uartputc() that doesn't 
// use interrupts, for use by kernel printf() and
// to echo characters. it spins waiting for the uart's
// output register to be empty.
void uart_putc_sync(int c) {

  push_off();

//  if(panicked){
//    for(;;)
//      ;
//  }

  // wait for Transmit Holding Empty to be set in LSR.
  while ((REGW(UART0, UART_LSR) & UART_LSR_BUF_EMPTY_MASK) == 0);
  REGB(UART0, UART_THR) = c;

  pop_off();
}


void ns16550_uart_putc(char ch) {

  if(ch == '\n') _ns16550_uart_putc('\r');
  _ns16550_uart_putc(ch);
}

void _ns16550_uart_putc(char ch) {
   
  acquire(&uart_tx_lock);

  while ((REGW(UART0, UART_LSR) & UART_LSR_BUF_EMPTY_MASK) == 0);
  REGB(UART0, UART_THR) = ch;
/*

  while(uart_tx_w == uart_tx_r + UART_TX_BUF_SIZE){
    // buffer is full.
    // wait for uartstart() to open up space in the buffer.
    sleep(&uart_tx_r, &uart_tx_lock);
  }

  uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE] = ch;
  uart_tx_w += 1;
  uart_start();
 */

  release(&uart_tx_lock);
}

// if the UART is idle, and a character is waiting
// in the transmit buffer, send it.
// caller must hold uart_tx_lock.
// called from both the top- and bottom-half.
void uart_start(void) {

  while(1){
    if(uart_tx_w == uart_tx_r){
      // transmit buffer is empty.
      return;
    }
    
    if((REGB(UART0, UART_LSR) & UART_LSR_TX_IDLE) == 0){
      // the UART transmit holding register is full,
      // so we cannot give it another byte.
      // it will interrupt when it's ready for a new byte.
      return;
    }
    
    int c = uart_tx_buf[uart_tx_r % UART_TX_BUF_SIZE];
    uart_tx_r += 1;
    
    // maybe uartputc() is waiting for space in the buffer.
    wakeup(&uart_tx_r);
    
    REGB(UART0, UART_THR) = c;
  }
}


// read one input character from the UART.
// return -1 if none is waiting.
int ns16550_uart_getc(void) {
  if(REGB(UART0, UART_LSR) & 0x01){
    // input data is ready.
    return REGB(UART0, UART_RHR);
  } else {
    return -1;
  }
}


 
void lib_putc(char ch) {
    if (uart_inited == 0x55555555) {
#ifdef __SIFIVE_U__   
    sifive_uart_putc(ch);
#endif   
#if (defined(__JH7110__) || defined(__SPACEMIT_K1__))
    ns16550_uart_putc(ch);
#endif 
    } else {
        sbi_console_putc(ch);
    }
    

}

void _putchar(char character){
  if (uart_inited == 0x55555555) {
#ifdef __SIFIVE_U__   
    sifive_uart_putc(character);
#endif   
#if (defined(__JH7110__) || defined(__SPACEMIT_K1__))
    ns16550_uart_putc(character);
#endif 
  } else {
    sbi_console_putc(character); 
  }
    
}

void lib_puts(char *s) {
    while (*s) lib_putc(*s++);

}

// handle a uart interrupt, raised because input has
// arrived, or the uart is ready for more output, or
// both. called from devintr().
void uart_intr(void) {

  // read and process incoming characters.
  while(1){
    int c = ns16550_uart_getc();
    if(c == -1)
      break;
    console_intr(c);
  }
 
  // send buffered characters.
  acquire(&uart_tx_lock);
  uart_start();
  release(&uart_tx_lock);
}

void uart_int_pending(void) {
  printf("UART IRQ STATUS 0x%X\n",REGB(UART0, UART_ISR));
}