MEMORY
{
  /* SRAM boot */
	rom (rx) :  ORIGIN = 0x20000000, LENGTH = 16K
	ram (rwx) : ORIGIN = 0x20004000, LENGTH = 4K
}


/* Include the common ld script. */
INCLUDE cortex-m-generic.ld

/*
 * STM32F103 SRAM boot workaround
 * tim5_isr is located at offset 0x0108
 * Executing it as instruction increases PC by 4
 * Reset vector is located at offset 0x0004
 * rax2 -ke "0x$(  rasm2 -a arm -b 16 'ldr.w pc, [pc, -0x108]'  )"
 */
tim5_isr = 0xF108F85F;

