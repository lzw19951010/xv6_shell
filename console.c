// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "console.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#define MAXCURSORPOS ((NPROC + 2)*80)
int consoleMode = 0;
int cursorPos = -2;
int realPos = -2;
int changeFlag = 0;
char* bufMemoryPoint = 0;
int memorySize = 0;
static void consputc(int);

static int panicked = 0;

 struct consLock cons;
struct inputStruct input;
static void
printint(int xx, int base, int sign)
{
  static char digits[] = "0123456789abcdef";
  char buf[16];
  int i;
  uint x;

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    consputc(buf[i]);
}
//PAGEBREAK: 50

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char *fmt, ...)
{
  int i, c, locking;
  uint *argp;
  char *s;

  locking = cons.locking;
  if(locking)
    acquire(&cons.lock);

  if (fmt == 0)
    panic("null fmt");

  argp = (uint*)(void*)(&fmt + 1);
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      printint(*argp++, 10, 1);
      break;
    case 'x':
    case 'p':
      printint(*argp++, 16, 0);
      break;
    case 's':
      if((s = (char*)*argp++) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }

  if(locking)
    release(&cons.lock);
}

void
panic(char *s)
{
  int i;
  uint pcs[10];
  
  cli();
  cons.locking = 0;
  cprintf("cpu%d: panic: ", cpu->id);
  cprintf(s);
  cprintf("\n");
  getcallerpcs(&s, pcs);
  for(i=0; i<10; i++)
    cprintf(" %p", pcs[i]);
  panicked = 1; // freeze other CPU
  for(;;)
    ;
}

//PAGEBREAK: 50
#define BACKSPACE 0x100
#define KEY_UP    0xE2
#define KEY_DN    0xE3
#define KEY_LF    0xE4
#define KEY_RT    0xE5
 ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

static void
cgaputc(int c)
{
  ushort CharTempt = 0;
  int pos = cursorPos;
  int i =0;
  // Cursor position: col + 80*row.

  
  if(realPos == -2)
  {
	    outb(CRTPORT, 14);
		pos = inb(CRTPORT+1) << 8;
		outb(CRTPORT, 15);
		pos |= inb(CRTPORT+1);
		realPos = cursorPos = pos;
  }
  if(consoleMode == 0)
  {
  switch(c)
  {
  case '\n':
	   pos += 80 - pos%80;
	   cursorPos = realPos = pos; 
	   break;
  case BACKSPACE: case KEY_LF:
       if(pos > 0) --pos;
	   cursorPos = pos;
	   break;
  case KEY_UP:
	  if(pos / 80 > 0)
	  {
		  pos -= 80;
	  }
	  cursorPos = pos;
	  break;
  case KEY_DN:
	  if(pos + 80 <= realPos)
	  {
		  pos += 80;
		  cursorPos = pos;
	  }
	  break;
  case KEY_RT:
	  if(cursorPos < realPos)
	  {
		 pos ++;
		  cursorPos = pos;
	  }
	  break;
  default:
	  memmove(crt + pos + 1, crt + pos, sizeof(crt[0]) * (realPos - cursorPos));
       crt[pos++] = (c&0xff) | 0x0700;  // black on white
	   cursorPos = pos;
	   realPos++;
	   break;
  }
  CharTempt = crt[pos];
  if((pos/80) >= 24){  // Scroll up.
    memmove(crt, crt+80, sizeof(crt[0])*23*80);
    pos -= 80;
    memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
  }
  
  outb(CRTPORT, 14);
  outb(CRTPORT+1, pos>>8);
  outb(CRTPORT, 15);
  outb(CRTPORT+1, pos);
  crt[pos] = CharTempt;
  }
  else if(consoleMode == 1)
  {
   switch(c)
  {
       case '\n':case '\r':
	   realPos += 80 - realPos %80;
	   break;
      case KEY_DN:
	  if(cursorPos < realPos - 80)
	  {
	   cursorPos += 80;
	   if(cursorPos >= 160)
	   {
		   for(i = cursorPos - 80; i < cursorPos; i++)
		   {
			   crt[i]  = crt[i] & 0xff;
			   crt[i] = crt[i] | 0x0700;
		   }
           for(i = cursorPos; i < cursorPos + 80; i++)
		   {
			   crt[i] = crt[i] & 0xff;
			   crt[i] = crt[i] | 0x0900;
		   }
	   }
           }
	   break;
  case KEY_UP:
	  if(cursorPos / 80 > 2)
	  {
		  cursorPos -= 80;
		  for(i = cursorPos + 80; i < cursorPos + 160; i++)
		  {
			  crt[i] = crt[i] & 0xff;
			  crt[i] = crt[i] | 0x0700;
		  }
	  for(i = cursorPos; i < cursorPos + 80; i++)
	  {
		  crt[i] = crt[i] & 0xff;
		  crt[i] = crt[i] | 0x0900;
	  }
	  }
	  break;

  default:
       crt[realPos++] = (c&0xff) | 0x0700;  // black on white
	   break;
  }
  }
}

void
consputc(int c)
{
  if(panicked){
    cli();
    for(;;)
      ;
  }
  if(c == BACKSPACE){
    uartputc('\b'); uartputc(' '); uartputc('\b');
  } 
 else
    uartputc(c);
  cgaputc(c);
}



#define C(x)  ((x)-'@')  // Control-x

void
consoleintr(int (*getc)(void))
{
  int c;

  acquire(&input.lock);
  if(consoleMode == 0)
  {
  while((c = getc()) >= 0){
    switch(c){
    case C('P'):  // Process listing.
      procdump();
      break;
    case C('U'):  // Kill line.
      while(input.e != input.w &&
            input.buf[(input.e-1) % INPUT_BUF] != '\n'){
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    case C('H'): case '\x7f':  // Backspace
      if(input.e != input.w){
        input.e--;
        consputc(BACKSPACE);
      }
	  break;
	case KEY_UP:
	  consputc(KEY_UP);
      break;
	case KEY_DN:
	  consputc(KEY_DN);
      break;
	case KEY_RT:
	  consputc(KEY_RT);
      break;
	case KEY_LF:
	  consputc(KEY_LF);
      break;
    default:
      if(c != 0 && input.e-input.r < INPUT_BUF){
        c = (c == '\r') ? '\n' : c;
        input.buf[input.e++ % INPUT_BUF] = c;
        consputc(c);
        if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
          input.w = input.e;
          wakeup(&input.r);
        }
      }
      break;
    }
  }
  }
  else if(consoleMode == 1)
  {
     while((c = getc()) >= 0)
     {
	  switch(c)
	  {
	   case KEY_UP:
		  input.buf[input.e++ %INPUT_BUF] = KEY_UP;
		  input.w = input.e;
		  wakeup(&input.r);
      break;
	   case KEY_DN: 
		  input.buf[input.e++ %INPUT_BUF] = KEY_DN;
		  input.w = input.e;
		  wakeup(&input.r);
		  break;
    case 'k': 
      input.buf[input.e++ %INPUT_BUF] = 'k';
      input.w = input.e;
      wakeup(&input.r);
      break;
	  }
     }
  }
  release(&input.lock);
}

int
consoleread(struct inode *ip, char *dst, int n)
{
  uint target;
  int c;
  iunlock(ip);
  target = n;
  acquire(&input.lock);
  while(n > 0){
    while(input.r == input.w){
      if(proc->killed){
        release(&input.lock);
        ilock(ip);
        return -1;
      }
      sleep(&input.r, &input.lock);
    }
    c = input.buf[input.r++ % INPUT_BUF];
    if(c == C('D')){  // EOF
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        input.r--;
      }
      break;
    }
    *dst++ = c;
    --n;
    if(c == '\n')
      break;
  }
  release(&input.lock);
  ilock(ip);

  return target - n;
}

int
consolewrite(struct inode *ip, char *buf, int n)
{
  int i;

  iunlock(ip);
  acquire(&cons.lock);
  for(i = 0; i < n; i++)
    consputc(buf[i] & 0xff);
  release(&cons.lock);
  ilock(ip);

  return n;
}

void
consoleinit(void)
{
  initlock(&cons.lock, "console");
  initlock(&input.lock, "input");

  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read = consoleread;
  cons.locking = 1;

  picenable(IRQ_KBD);
  ioapicenable(IRQ_KBD, 0);
}

