#include <fcntl.h>

extern	int ieee,	/* File descriptor (handle) for IEEE routines */

	    errno,	/* Error code, if any */ 

	    ioctl_io(),	/* IOCTL read and write */

	    cklpint(),	/* Check for light-pen interrupts, return 0 if none */
	    _false_(),	/* Default check, always return 0 */
	    (*ieee_cki)();	/* Pointer to check routine,
				   initially points to _false_ */

extern	void no_op(),	/* Function that does nothing at all */
	     (*ieee_isr)();	/* Pointer to Personal488 Interrupt Service
				   Routine (ISR).  Initally points to no_op */

extern	int _ieeewt(),	/* IEEE Write and Read routines.  Use macro below. */
	    _ieeerd(),

	    rawmode(),	/* Set a file into Raw Mode */

	    ieeeinit();	/* Initialize Personal488 */ 


#define ioctl_rd(handle,chars,size) ioctl_io(handle,chars,size,0x4402)
#define ioctl_wt(handle,chars,size) ioctl_io(handle,chars,size,0x4403)
 
#define ieeewt(chars) _ieeewt(ieee,chars)
#define ieeerd(chars) _ieeerd(ieee,chars,sizeof(chars))



#if defined(M_I86SM) || defined(M_I86MM)    /* small or medium model */
/******************** SMALL DATA MODEL *************************************/
extern int segment();
#define offset(ptr) (int)(ptr)

#else
/****************** LARGE DATA MODEL ***************************************/
#define offset(fp)	((unsigned)(fp))
#define segment(fp)	((unsigned)((unsigned long)(fp) >> 16))

#endif

