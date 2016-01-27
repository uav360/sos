#include <libc.h>
#include <memory_map.h>

#include "mmio.h"
#include "uart.h"
#include "shell.h"

#define UART0
#define UART_IO_SIZE 256

u32  uart_recv_buf_index = 0;
char uart_recv_buf[UART_IO_SIZE] = {0};

char uart_send_buf[UART_IO_SIZE] = {0};
char format_buf[UART_IO_SIZE] = {0};

void bcm2835_delay(u32 n)
{
     volatile u32 i = 0;
      for(i = 0; i < n; i++);
}

 
void uart_wait_fifo_empty()
{
    while(1) {
        if ((readl(UART0_FR) & (1 << 7))) {
            break;
        }
    }
}

/*
 * Transmit a byte via UART0.
 * u8 Byte: byte to send.
 */
void uart_putc(u8 byte) {
#ifdef UART0
    // wait for UART to become ready to transmit
    while (1) {
        if (!(readl(UART0_FR) & (1 << 5))) {
            break;
        }
    }
    writel(UART0_DR, byte);
    uart_wait_fifo_empty();
#else
    while((AUX_MU_LSR_REG & 0x20) == 0);
    AUX_MU_IO_REG = byte;
#endif
}
 
/*
 * print a string to the UART one character at a time
 * const char *str: 0-terminated string
 */
void uart_puts(const char *str) {
    while (*str) {
        if (*str == '\n') {
            uart_putc('\r');
            uart_putc('\n');
            str++;
        }
        else {
            uart_putc(*str++);
        }
    }
}

s8 uart_recv()
{
    while (1) {
        if (!(readl(UART0_FR) & (1 << 4))) {
            break;
        }
    }
    return readl(UART0_DR);
}

void uart_irq_handler(u32 irq_nr)
{
    u8 ch;

    /* handle one cmd */
    uart_puts("\nsos>");

    while((ch = uart_recv())) {

        if (ch == '\n') {   /* sscom will send '\r\n' */
            continue;
        }
        if (uart_recv_buf_index == (UART_IO_SIZE - 1) && ch != '\r') {
            uart_puts("cmd too long!\n");
            uart_recv_buf_index = 0;
            return;

        }

        if (ch == '\r') {
            uart_recv_buf[uart_recv_buf_index] = '\0';  /* terminate the string. */
            shell(uart_recv_buf);
            uart_recv_buf_index = 0;
            return;
        } else {
            uart_recv_buf[uart_recv_buf_index] = ch;
            uart_recv_buf_index++;
        }


        /* echo */
        if (ch == '\r') {
            uart_putc('\r');
            uart_putc('\n');
        } else {
            uart_putc(ch);
        }
    }
    return;
}

static char *ltoa(char *p, long num, unsigned radix) 
{
  long divisor = 0;
  int i;
  char *q;
  long l, ll;

  l = num;
  if (divisor == 0) {
    ll = num;
  } else {
    ll = divisor;
  }

  q = p + 11;
  do {
    i = (int)(l % radix);
    i += '0';
    if (i > '9')
      i += 'A' - '0' - 10;
    *--q = i;
    l /= radix;
  } while ((ll /= radix) != 0);

  i = (int)(p + 11 - q);
  do
    *p++ = *q++;
  while (--i);

  return p;
}

void uart_printf(const char* fmt,...)
{
#if 0
    va_list args;
    va_start(args,fmt);
    vsnprintf(format_buf,sizeof(format_buf), fmt, args);
    va_end(args);
    uart_puts(format_buf);
    memset(format_buf, 0, sizeof(format_buf));
#else
  va_list ap;
  char *p, *s, c, filler;
  int i, precision, width;
  s32 is_long, left_align;
  long l;
  char tmpbuf[11 + 1];

  va_start(ap, fmt);
  while (1) {
    c = *fmt++;
    if (c == 0) {
      va_end(ap);
      return;
    }
    if (c != '%') {
        if (c == '\n') {
            uart_putc('\r');
            uart_putc('\n');
        } else {
            uart_putc(c);
        }
      continue;
    }
    p = tmpbuf;
    s = tmpbuf;
    left_align = 0;
    if (*fmt == '-') {
      fmt++;
      left_align = 1;
    }
    filler = ' ';
    if (*fmt == '.') {
      fmt++;
      filler = '0';
    }
    width = 0;
    while (1) {
      c = *fmt++;
      if (c >= '0' && c <= '9')
        c -= '0';
      else if (c == '*')
        c = va_arg(ap, int);
      else
        break;
      width = width * 10 + c;
    }
    precision = 0;
    if (c == '.') {
      while (1) {
        c = *fmt++;
        if (c >= '0' && c <= '9')
          c -= '0';
        else if (c == '*')
          c = va_arg(ap, int);
        else
          break;
        precision *= 10;
        precision += c;
      }
    }
    /* Long modifier.*/
    if (c == 'l' || c == 'L') {
      is_long = 1;
      if (*fmt)
        c = *fmt++;
    }
    else
      is_long = (c >= 'A') && (c <= 'Z');

    /* Command decoding.*/
    switch (c) {
    case 'c':
      filler = ' ';
      *p++ = va_arg(ap, int);
      break;
    case 's':
      filler = ' ';
      if ((s = va_arg(ap, char *)) == 0)
        s = "(null)";
      if (precision == 0)
        precision = 32767;
      for (p = s; *p && (--precision >= 0); p++)
        ;
      break;
    case 'D':
    case 'd':
      if (is_long)
        l = va_arg(ap, long);
      else
        l = va_arg(ap, int);
      if (l < 0) {
        *p++ = '-';
        l = -l;
      }
      p = ltoa(p, l, 10);
      break;
    case 'X':
    case 'x':
      c = 16;
      goto unsigned_common;
    case 'U':
    case 'u':
      c = 10;
      goto unsigned_common;
    case 'O':
    case 'o':
      c = 8;
unsigned_common:
      if (is_long)
        l = va_arg(ap, long);
      else
        l = va_arg(ap, int);
      p = ltoa(p, l, c);
      break;
    default:
      *p++ = c;
      break;
    }
    i = (int)(p - s);
    if ((width -= i) < 0)
      width = 0;
    if (left_align == 0)
      width = -width;
    if (width < 0) {
      if (*s == '-' && filler == '0') {
        uart_putc((u8)*s++);
        i--;
      }
      do
        uart_putc((u8)filler);
      while (++width != 0);
    }
    while (--i >= 0)
      uart_putc((u8)*s++);

    while (width) {
      uart_putc((u8)filler);
      width--;
    }
  }
#endif
}

/*
 * Initialize UART0.
 */
void uart_init() {
#ifdef UART0
    set_gpio_function(14, ALT_FUNC_5);
    set_gpio_function(15, ALT_FUNC_5);

    // Disable UART0.
    writel(UART0_CR, 0x00000000);
    // Setup the GPIO pin 14 && 15.
 
    // Disable pull up/down for all GPIO pins & delay for 150 cycles.
    writel(GPPUD, 0x00000000);
    delay(150);
 
    // Disable pull up/down for pin 14,15 & delay for 150 cycles.
    writel(GPPUDCLK0, (1 << 14) | (1 << 15));
    delay(150);
 
    // Write 0 to GPPUDCLK0 to make it take effect.
    writel(GPPUDCLK0, 0x00000000);
 
    // Clear pending interrupts.
    writel(UART0_ICR, 0x7FF);
 
    // Set integer & fractional part of baud rate.
    // Divider = UART_CLOCK/(16 * Baud)
    // Fraction part register = (Fractional part * 64) + 0.5
    // UART_CLOCK = 3000000; Baud = 115200.
 
    // Divider = 3000000/(16 * 115200) = 1.627 = ~1.
    // Fractional part register = (.627 * 64) + 0.5 = 40.6 = ~40.
    writel(UART0_IBRD, 1);
    writel(UART0_FBRD, 40);
 
    // Enable FIFO & 8 bit data transmissio (1 stop bit, no parity).
    writel(UART0_LCRH, (1 << 4) | (1 << 5) | (1 << 6));
 
    // Mask all interrupts.
#if 0
    writel(UART0_IMSC, (1 << 1) | (1 << 4) | (1 << 5) |
            (1 << 6) | (1 << 7) | (1 << 8) |
            (1 << 9) | (1 << 10));
#else
    writel(UART0_IMSC, (1 << 1) | (1 << 4) |
            (1 << 6) | (1 << 7) | (1 << 8) |
            (1 << 9) | (1 << 10));
#endif
 
    // Enable UART0, receive & transfer part of UART.
    writel(UART0_CR, (1 << 0) | (1 << 8) | (1 << 9));

    request_irq(IRQ_UART, uart_irq_handler);
    enable_irq(IRQ_UART);
#else

/* the mini uart */
#define BCM2835_CLOCK_FREQ 250000000
#define BAUD_RATE_COUNT(baudrate) ((BCM2835_CLOCK_FREQ / (8 * (baudrate))) - 1)
#define REG(x) (*(volatile u32 *)(x))
#define BIT(n) (1 << (n))
#define AUX_ENABLES     REG(0x20215004)

// --- Mini UART Registers -----
#define AUX_MU_IO_REG   REG(0x20215040)
#define AUX_MU_IER_REG  REG(0x20215044)
#define AUX_MU_IIR_REG  REG(0x20215048)
#define AUX_MU_LCR_REG  REG(0x2021504C)
#define AUX_MU_MCR_REG  REG(0x20215050)
#define AUX_MU_LSR_REG  REG(0x20215054)
#define AUX_MU_MSR_REG  REG(0x20215058)
#define AUX_MU_SCRATCH  REG(0x2021505C)
#define AUX_MU_CNTL_REG REG(0x20215060)
#define AUX_MU_STAT_REG REG(0x20215064)
#define AUX_MU_BAUD_REG REG(0x20215068)

#define AUX_MU_IER_TX_IRQEN  BIT(1)

#define AUX_MU_IIR_RX_IRQ     ((AUX_MU_IIR_REG & 0x07) == 0x04)
#define AUX_MU_IIR_TX_IRQ     ((AUX_MU_IIR_REG & 0x07) == 0x02)

#define AUX_MU_LSR_RX_RDY     (AUX_MU_LSR_REG & BIT(0))
#define AUX_MU_LSR_TX_RDY     (AUX_MU_LSR_REG & BIT(5))

// *****************************************************************************
//                        Interrupts
// *****************************************************************************

#define IRQ_BASIC         REG(0x2000B200)
#define IRQ_PEND1         REG(0x2000B204)
#define IRQ_PEND2         REG(0x2000B208)
#define IRQ_FIQ_CONTROL   REG(0x2000B210)
#define IRQ_ENABLE1       REG(0x2000B210)
#define IRQ_ENABLE2       REG(0x2000B214)
#define IRQ_ENABLE_BASIC  REG(0x2000B218)
#define IRQ_DISABLE1      REG(0x2000B21C)
#define IRQ_DISABLE2      REG(0x2000B220)
#define IRQ_DISABLE_BASIC REG(0x2000B220)
#define GPFN_IN     0x00
#define GPFN_OUT    0x01
#define GPFN_ALT0   0x04
#define GPFN_ALT1   0x05
#define GPFN_ALT2   0x06
#define GPFN_ALT3   0x07
#define GPFN_ALT4   0x03
#define GPFN_ALT5   0x02

#define GPPUDD           REG(0x20200094)
#define GPPUDCLK0       REG(0x20200098)
#define GPPUDCLK1       REG(0x2020009C)


    IRQ_DISABLE1 = BIT(29);

    AUX_ENABLES = 1;

    AUX_MU_IER_REG  = 0x00;
    AUX_MU_CNTL_REG = 0x00;
    AUX_MU_LCR_REG  = 0x03; // Bit 1 must be set
    AUX_MU_MCR_REG  = 0x00;
    AUX_MU_IER_REG  = 0x05;
    AUX_MU_IIR_REG  = 0xC6; 
    AUX_MU_BAUD_REG = BAUD_RATE_COUNT(115200);

    bcm2835_gpio_fnsel(14, GPFN_ALT5);
    bcm2835_gpio_fnsel(15, GPFN_ALT5);

    GPPUDD = 0;
    bcm2835_delay(150);
    GPPUDCLK0 = (1<<14)|(1<<15);
    bcm2835_delay(150);
    GPPUDCLK0 = 0;

    AUX_MU_CNTL_REG = 0x03;

    IRQ_ENABLE1 = BIT(29);
#endif
}