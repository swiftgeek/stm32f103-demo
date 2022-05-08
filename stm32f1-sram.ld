MEMORY
{
  /* SRAM boot */
	rom (rx) :  ORIGIN = 0x20000000, LENGTH = 16K
	ram (rwx) : ORIGIN = 0x20004000, LENGTH = 4K
}


/* Include the common ld script. */
INCLUDE cortex-m-generic.ld

tim5_isr = 0xF108F85F;

