#include <uart.h>
#include <printf.h>


#define UART_BUFFER_SIZE 32
typedef struct sbi_uart_buffer
{
   char buffer[UART_BUFFER_SIZE];
   int  start;
   int  occupied;
}
sbi_uart_buffer_t;
sbi_uart_buffer_t sbi_global_uart_buffer;


void uart_init(void)
{
   volatile char *uart = (char *)0x10000000;
   uart[3] = 0b11; // word length select, set to select 8 bits.
   uart[2] = 0b1;  // enable FIFO
   uart[1] = 0b1;  // enable UART to interrupt the SBI when data available.

   sbi_global_uart_buffer.start = 0;
   sbi_global_uart_buffer.occupied = 0;

   return;
}

char uart_get(void)
{
   volatile char *uart = (char *)0x10000000;
   char c;
   if(uart[5] & 0b1)
   {
      c = uart[0];
   }
   else
   {
      c = 0b11111111;
   }
   return c;
}

void uart_put(char c)
{
   volatile char *uart = (char *)0x10000000;

   while(!(uart[5] & (0b01000000))) // block while transmitter full
   {
      ;
   }
   uart[0] = c;
   return;
}


/* if buffer is full, ignore the input. Else add it to buffer */
void uart_buffer_push(char c)
{
   if(sbi_global_uart_buffer.occupied >= UART_BUFFER_SIZE)
   {
      return;
   }
   /* else */
   int index = (sbi_global_uart_buffer.start + sbi_global_uart_buffer.occupied)
                       % UART_BUFFER_SIZE;
   sbi_global_uart_buffer.buffer[index] = c;
   sbi_global_uart_buffer.occupied = (sbi_global_uart_buffer.occupied + 1);
                        // no need to mod by buffer size here, since we checked
                        // at the beginning of function.
}

char uart_buffer_pop(void)
{
   if(sbi_global_uart_buffer.occupied == 0)
   {
      return 0xff;
   }
   char c;
   c = sbi_global_uart_buffer.buffer[sbi_global_uart_buffer.start];
   sbi_global_uart_buffer.start = (sbi_global_uart_buffer.start + 1)
                       % UART_BUFFER_SIZE;
   sbi_global_uart_buffer.occupied = sbi_global_uart_buffer.occupied - 1;
   return c;

}

void uart_buffer_print(void)
{
   int remaining = sbi_global_uart_buffer.occupied;
   int charstart = sbi_global_uart_buffer.start;
   for(; remaining > 0; remaining --)
   {
      printf("%c", sbi_global_uart_buffer.buffer[charstart]);
      charstart = (charstart + 1) % UART_BUFFER_SIZE;
   }
   return;
}

void uart_handle_irq(void)
{
   char c = uart_get();
   uart_buffer_push(c);
   // printf("UART places %c (%d) in buffer\n", c, c);
   // uart_buffer_print();
   // printf("\n");
}
