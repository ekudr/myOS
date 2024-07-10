#include <common.h>
#include <spinlock.h>
#include <sys/riscv.h>
#include <linux/errno.h>


#define UART_TXDATA 0x00 // Transmit data register
#define UART_RXDATA 0x04 // Receive data register
#define UART_TXCTRL 0x08 // Transmit control register
#define UART_RXCTRL 0x0C // Receive control register
#define UART_IE     0x10 // UART interrupt enable
#define UART_IP     0x14 // UART interrupt pending
#define UART_DIV    0x18 // Baud rate divisor

#define UART_TXFIFO_FULL	0x80000000 // Transmit FIFO full
#define UART_RXFIFO_EMPTY	0x80000000 // Receive FIFO empty
#define UART_RXFIFO_DATA	0x000000ff
#define UART_TXCTRL_TXEN	0x1 // Transmit enable
#define UART_RXCTRL_RXEN	0x1 // Receive enable

/* IP register */
#define UART_IP_RXWM            0x2 //Receive watermark interrupt pending



// the transmit output buffer.
struct spinlock sf_uart_tx_lock;


static inline void 
uart_writel(uintreg_t base, int reg, uint32_t val) {
    putreg32(val, (uintreg_t)(base+reg));
}

static inline uint32_t 
uart_readl(uintreg_t base, int reg){
    return getreg32((uintreg_t)(base+reg));
}

static int 
_sifive_uart_putc(const char c) {

	if (uart_readl(UART0, UART_TXDATA) & UART_TXFIFO_FULL)
		return -EAGAIN;

	uart_writel(UART0, UART_TXDATA, c);

	return 0;
}

static int 
_sifive_uart_getc() {

	int ch = uart_readl(UART0, UART_RXDATA);

	if (ch & UART_RXFIFO_EMPTY)
		return -EAGAIN;
	ch &= UART_RXFIFO_DATA;

	return ch;
}

int 
sifive_uart_getc() {
	int c;

	while ((c = _sifive_uart_getc()) == -EAGAIN) ;

	return c;
}

int 
sifive_uart_putc(const char ch) {
	int rc;

	while ((rc = _sifive_uart_putc(ch)) == -EAGAIN) ;

	return rc;
}

void 
sifive_uart_init(void) {
  // Transmit enable 
	uart_writel(UART0, UART_TXCTRL, UART_TXCTRL_TXEN);
  // Receive enable
	uart_writel(UART0, UART_RXCTRL, UART_RXCTRL_RXEN);
  // disable interrupts.
	uart_writel(UART0, UART_IE, 0);
  
  initlock(&sf_uart_tx_lock, "uart");

  uart_writel(UART0, UART_DIV, 4340);

}