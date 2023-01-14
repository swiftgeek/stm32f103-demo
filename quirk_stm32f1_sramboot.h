/*
 * In STM32F103 SRAM boot first instruction executed is at SRAM_BASE+0x0108.
 * It just so happens that 0x0108 corresponds to IRQ50/tim5/tim5_isr.
 * Each IRQ vector takes 4 bytes and it's enough to fit an instruction.
 * Instruction providing workaround is set in stm32f1-sram.ld, tim5_isr.
 */

#undef NVIC_TIM5_IRQ
#define NVIC_TIM5_IRQ "IRQ_RESERVED_FOR_SRAM_BOOT"
#define SRAM_BASE (0x20000000U)

__attribute__((constructor)) static void quirk_stm32f1_sramboot(void) {
  // Confirm whether this function is located in SRAM
  if ( ( (int)quirk_stm32f1_sramboot & SRAM_BASE ) != SRAM_BASE )
    return;
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
  if (SCB_CFSR != (SCB_CFSR_PRECISERR | SCB_CFSR_BFARVALID) )
    return;
  // BOOTx is configured for SRAM boot, as reading 0x10 caused BusFault
  // Clear BusFault we caused from CFSR
  SCB_CFSR = SCB_CFSR_PRECISERR | SCB_CFSR_BFARVALID;
  if (SCB_VTOR != 0 )
    return;
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
  /* Load address of reset_handler() from vector_table and run it */
  (*(void (**)())(SRAM_BASE + 4))();
}

