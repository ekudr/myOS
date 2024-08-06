#include <common.h>
#include <spinlock.h>


#define BACKSPACE 0x100
#define C(x)  ((x)-'@')  // Control-x

struct {
  struct spinlock lock;
  
  // input
#define INPUT_BUF_SIZE 128
  char buf[INPUT_BUF_SIZE];
  uint32_t r;  // Read index
  uint32_t w;  // Write index
  uint32_t e;  // Edit index
} cons;


//
// send one character to the uart.
// called by printf(), and to echo input characters,
// but not from write().
//
void
console_putc(int c)
{
  if(c == BACKSPACE){
    // if the user typed backspace, overwrite with a space.
    uart_putc_sync('\b'); uart_putc_sync(' '); uart_putc_sync('\b');
  } else {
    uart_putc_sync(c);
  }
}


//
// user write()s to the console go here.
//
/*
int console_write(int user_src, uint64 src, int n) {

  int i;

  for(i = 0; i < n; i++){
    char c;
    if(either_copyin(&c, user_src, src+i, 1) == -1)
      break;
    uart_putc(c);
  }

  return i;
}
*/
//
// the console input interrupt handler.
// uartintr() calls this for input character.
// do erase/kill processing, append to cons.buf,
// wake up consoleread() if a whole line has arrived.
//
void console_intr(int c) 
{
  acquire(&cons.lock);

  switch(c){
  case C('P'):  // Print process list.
//    procdump();
      plic_info();
    break;
  case C('U'):  // Kill line.
    while(cons.e != cons.w &&
          cons.buf[(cons.e-1) % INPUT_BUF_SIZE] != '\n'){
      cons.e--;
      console_putc(BACKSPACE);
    }
    break;
  case C('H'): // Backspace
  case '\x7f': // Delete key
    if(cons.e != cons.w){
      cons.e--;
      console_putc(BACKSPACE);
    }
    break;
  default:
    if(c != 0 && cons.e-cons.r < INPUT_BUF_SIZE){
      c = (c == '\r') ? '\n' : c;

      // echo back to the user.
      console_putc(c);

      // store for consumption by consoleread().
      cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;

      if(c == '\n' || c == C('D') || cons.e-cons.r == INPUT_BUF_SIZE){
        // wake up consoleread() if a whole line (or end-of-file)
        // has arrived.
        cons.w = cons.e;
        wakeup(&cons.r);
      }
    }
    break;
  }
  
  release(&cons.lock);
}

void console_init(void) {
    printf("[CONSOLE] init ... ");
    initlock(&cons.lock, "cons");

    board_uart_init();
	  printf("Done.\n");

  // connect read and write system calls
  // to consoleread and consolewrite.
  //devsw[CONSOLE].read = consoleread;
  //devsw[CONSOLE].write = consolewrite;
}