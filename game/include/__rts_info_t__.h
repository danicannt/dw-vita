/*
 *	__rts_info_t__.h - Linker generated symbols
 *
 *	Copyright (c) 1995 metrowerks inc. All Rights Reserved.
 *
 */

/* Note: On systems having a .data and .sdata sections, the linker 		*/
/* generated symbols must be in the .data section otherwise we would	*/
/* not be able to load GP!												*/

#ifdef __MWERKS__
#define SECTION __declspec(data) 
#else
#define SECTION __attribute__((section(".data")))
#endif

SECTION extern void (*__static_init[])(void);
SECTION extern unsigned long _gp;
SECTION extern unsigned long _sbss_size;
SECTION extern unsigned long _bss_size;
SECTION extern unsigned long _sdata_size;
SECTION extern unsigned long _data_size;
SECTION extern unsigned long _rdata_size;
SECTION extern unsigned long _fsbss; 		// 1st location of .sbss
SECTION extern unsigned long _fbss;			// 1st location of .bss
SECTION extern unsigned long _fsdata;		// 1st location of .sdata
SECTION extern unsigned long _fdata;		// 1st location of .data
SECTION extern unsigned long _frdata;		// 1st location of .rdata
SECTION extern unsigned long _start_etbl;
SECTION extern unsigned long _nbfunctions;
SECTION extern unsigned long _ftext;		// 1st location of .text
SECTION extern unsigned long _stack_addr;
SECTION extern unsigned long _nbtvectors;
SECTION extern unsigned long _stack_size;
SECTION extern unsigned long _heap_size;
SECTION extern unsigned long _etext;		// 1st location after .text
SECTION extern unsigned long _edata;		// 1st location after .sdata
SECTION extern unsigned long _end;			// 1st location after .bss

/* ROM Support															*/
SECTION extern unsigned long _fsdata_ROM;	// 1st location of .sdata (rom copy)
SECTION extern unsigned long _sdata_ROM_size;
SECTION extern unsigned long _fdata_ROM;	// 1st location of .data  (rom copy)	
SECTION extern unsigned long _data_ROM_size;

/* -------------------------------------------------------------------- */
/* PlayStation only														*/
/* -------------------------------------------------------------------- */
SECTION extern unsigned long _scratchpad_size;	// size of scratch pad

/* Array containing the load address of each overlay section.  The 		*/
/* array is indexed by the overlay ID (overlayID).  Index 0 will 		*/
/* contain the address of the root.										*/
SECTION extern unsigned long _overlay_section_addreses[];