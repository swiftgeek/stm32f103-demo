#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <stdio.h>
#include <sys/cdefs.h>
#include <errno.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/cortex.h>

static void clock_setup(void)
{
  rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);
  // Enable peripheral clocks
  rcc_periph_clock_enable(RCC_GPIOB);
  rcc_periph_clock_enable(RCC_GPIOC);
  rcc_periph_clock_enable(RCC_USART1);
  rcc_periph_clock_enable(RCC_AFIO);
}

static void gpio_setup(void)
{
  // Configure LED: GPIO13 (in GPIO port B) to 'output push-pull'
  gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ,
      GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
}

static void usart_setup(void)
{
  // AFIO remap
  gpio_primary_remap(AFIO_MAPR_SWJ_CFG_FULL_SWJ, AFIO_MAPR_USART1_REMAP);
  // Setup PB6 as output
  gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ,
      GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_USART1_RE_TX);
  // PB7 as input
  gpio_set_mode(GPIOB, GPIO_MODE_INPUT,
        GPIO_CNF_INPUT_FLOAT, GPIO_USART1_RE_RX);

  // Setup UART parameters. */
  usart_set_baudrate(USART1, 115200);
  usart_set_databits(USART1, 8);
  usart_set_stopbits(USART1, USART_STOPBITS_1);
  usart_set_mode(USART1, USART_MODE_TX_RX);
  usart_set_parity(USART1, USART_PARITY_NONE);
  usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);

  // Finally enable the USART.
  usart_enable(USART1);
}

static int picolibc_putc(char c, FILE *file)
{
  usart_send_blocking(USART1, c);
  (void) c;
  (void) file;
  return c;
}

//FILE picolibc_stdio = FDEV_SETUP_STREAM(picolibc_putc, picolibc_getc, picolibc_flush, _FDEV_SETUP_RW;
FILE picolibc_stdio = FDEV_SETUP_STREAM(picolibc_putc, NULL, NULL, _FDEV_SETUP_WRITE);

#define STDIO_ALIAS(x) FILE *const x = &picolibc_stdio;
//STDIO_ALIAS(stdin);
STDIO_ALIAS(stdout);
//STDIO_ALIAS(stderr);

/*
 * In STM32F103 SRAM boot first instruction executed is at SRAM_BASE+0x0108.
 * It just so happens that 0x0108 corresponds to IRQ50/tim5/tim5_isr.
 * Each IRQ vector takes 4 bytes and it's enough to fit an instruction.
 * Instruction providing workaround is set in stm32f1-sram.ld, tim5_isr.
 */

#undef NVIC_TIM5_IRQ
#define NVIC_TIM5_IRQ "IRQ_RESERVED_FOR_SRAM_BOOT"
#define SRAM_BASE (0x20000000U)

int main(void)
{
  if ( ( (int)main & SRAM_BASE ) == SRAM_BASE ) {
    // Disable faults and set BFHFNMIGN to continue on a BusFault
    cm_disable_faults();
    SCB_CCR = SCB_CCR | SCB_CCR_BFHFNMIGN;
    // Read but discard value, we are only interested in BusFault
    // In case of SRAM boot 0x10 is the first address beyond bogus MaskROM,
    // and nothing is mapped at that address.
    // If flash boot was used, then range 0x0000 0000 - 0x0800 0000 would be
    // aliased to flash instead.
    MMIO32(0x10);
    SCB_CCR = SCB_CCR ^ SCB_CCR_BFHFNMIGN;
    cm_enable_faults();
    if (SCB_CFSR == SCB_CFSR_PRECISERR | SCB_CFSR_BFARVALID) {
      // BOOTx is configured for SRAM boot, as reading 0x10 caused BusFault
      // Clear BusFault we caused from CFSR
      SCB_CFSR = SCB_CFSR_PRECISERR | SCB_CFSR_BFARVALID;
      if (SCB_VTOR == 0 ) {
        // First run after reset
        // Mind that we only have bogus 16 byte vector table from MaskROM,
        // so modifying any code happening including and before this
        // is not recommended.
        // In case of any issues, retry without SRAM boot workaround on flash,
        // with BOOTx pins set to flash boot.
        SCB_VTOR = SRAM_BASE;
        /* Initialize master stack pointer. */
         __asm__ volatile("msr msp, %0"::"g"
            (*(volatile uint32_t *)SRAM_BASE));
        /* Jump to reset_handler() */
        (*(void (**)())(SRAM_BASE + 4))();
      }
    }
  }

//  nvic_enable_irq(NVIC_TIM5_IRQ);

  static uint8_t data = 'U';
  clock_setup();
  gpio_setup();
  usart_setup();
  printf("stm32f103 demo\r\n");

  while (1) {
    gpio_toggle(GPIOC, GPIO13);
    data = usart_recv_blocking(USART1);
    printf("Received data: 0x%X\r\n", data);
  }

  return 0;
}



