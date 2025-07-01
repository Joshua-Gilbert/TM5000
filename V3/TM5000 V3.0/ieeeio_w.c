/*
 * IEEEIO.C - Modified for OpenWatcom compatibility
 * Changed from varargs.h to stdarg.h for ANSI C compliance
 */

#include <fcntl.h>
#include <dos.h>
#include <io.h>
#include <stdio.h>
#include <stdarg.h>    /* Changed from varargs.h */
#include <string.h>    /* Added for strlen */
#include "ieeeio.h"    /* Include header file for macros and declarations */

int ieee; /* holds file descriptor (handle) for IEEE I/O routines */

/***************************************************************************
 *
 *  The segment and offset macros/functions are defined in ieeeio.h
 *  For small or medium models, segment() is a function (defined below)
 *  and offset is a macro.
 *  For large models, both are macros defined in the header.
 *   
 ***************************************************************************/

#if defined(M_I86SM) || defined(M_I86MM)    /* small or medium model */
/******************** SMALL DATA MODEL *************************************/
int segment(ptr)
  void near *ptr;
{ static struct SREGS segs = { 0, 0, 0, 0 };
  if (segs.ds==0) segread(&segs);
  return segs.ds;
}
#endif

/****************************************************************************
 *
 * ioctl_rd(fd,buf,bufsize) and ioctl_wt(fd,buf,bufsize) are just like
 * the unbuffered 'read' and 'write' routines, except that they read
 * and write to the I/O control path (the "back door") of the device.
 * See the Aztec C library manual for descriptions of 'read' and 'write'.
 *
 * These routines are used to control special features of devices by
 * providing an additional method of communicating with their driver
 * that does not transfer data to or from the device.
 *
 * ioctl_wt is used with Driver488 to send the "BREAK" command, and
 * ioctl_rd is used to read the internal state of Driver488.  See
 * Chapter 6 of the Personal488 manual for more details.
 *
 ***************************************************************************/

extern int errno;	/* Holds error code, if any */ 

int ioctl_io(handle,chars,size,iocall)
  int handle,
      size,
      iocall;
  char chars[];
{ union REGS regs;
  struct SREGS segs;
  int len;

  regs.x.ax=iocall;
  regs.x.bx=handle;
  regs.x.cx=size;
  regs.x.dx=(unsigned int)chars;  /* Direct cast instead of offset macro */
  segs.ds=segment(chars);
  intdosx(&regs,&regs,&segs);
  if (regs.x.cflag) {
    errno=regs.x.ax;
    return -1;
  } else {
    return regs.x.ax;
  }
}

/****************************************************************************
 *
 * cklpint() is a function that is true if light pen interrupt is pending.
 *
 * _false_() is a function that does returns zero (false).
 *
 * (*ieee_cki)() is a pointer to a function that is used during IEEE I/O to
 * check for the light pen interrupt.  It is usually set to cklpint or 
 * _false_.
 *
 * no_op() is a function that does nothing.
 *
 * (*ieee_isr)() is a pointer to a function that is executed when the
 * IEEE I/O routines detect an interrupt by checking (*ieee_cki)().
 * ieee_isr is usually set to no_op, but it may be set by the programmer
 * to point to the desired interrupt service routine.
 *
 ****************************************************************************/

int cklpint()
{ union REGS regs;
  char buf;
  unsigned int firstresponse;

  if (ioctl_io(ieee,&buf,1,0x4402) != 1) return 0;  /* Direct call instead of macro */
  if (buf != '0') return 0;
  regs.x.ax=0x0400; /* Function 4: check light pen status */
  int86(0x10,&regs,&regs);
  firstresponse=regs.h.ah;
  regs.x.ax=0x0400; /* Function 4: check light pen status */
  int86(0x10,&regs,&regs);
  return (regs.h.ah | firstresponse);
}

int _false_()
{ return 0; }

int (*ieee_cki)() = _false_;

void no_op()
{ }

void (*ieee_isr)() = no_op;

/****************************************************************************
 *
 * ieeewt(chars) writes the zero-terminated string to the file 'ieee' and
 * checks for errors.
 *
 * ieeerd(chars) reads chars from the file 'ieee'. 'chars' must be an array
 * so that sizeof(chars) will give the number of bytes in chars.
 *
 ***************************************************************************/ 

int _ieeewt(handle,chars)
  int handle;
  char chars[];
{ int written,
      errcode;

  if ( (*ieee_cki)() ) (*ieee_isr)();
  written=write(handle,chars,strlen(chars));
  if (written==-1) {
    errcode=errno;
    printf("\n\n_ieeewt error: 0x%x, writing \"%s\"\n\n",errcode,chars);
  }

  return written;
}

int _ieeerd(handle,chars,size)
  int handle,
      size;
  char chars[];
{ int red,
      errcode;

  red=read(handle,chars,size);
  if (red==-1) {
    errcode=errno;
    printf("\n\n_ieeerd error: 0x%x\n\n",errcode);
  }
  return red;
}

/****************************************************************************
 *
 * ieeeprtf(format,vars) is equivalent to printf(format,vars) except that
 *   it writes to Personal488.
 *
 * ieeescnf(format,&vars) is equivalent to scanf(format,&vars) except that
 *   it reads from Personal488 and is limited to 5 arguments.
 *
 ***************************************************************************/

/* ANSI C style variable arguments for OpenWatcom */
int ieeeprtf(char *format, ...)
{ 
  char buffer[256];
  va_list arg_ptr;
  int result;

  va_start(arg_ptr, format);
  vsprintf(buffer, format, arg_ptr);
  va_end(arg_ptr);
  
  return _ieeewt(ieee, buffer);  /* Call _ieeewt directly */
}

/* Keep the 5-argument version as-is since it works */
int ieeescnf(format,a,b,c,d,e)
char *format,*a,*b,*c,*d,*e;
{ char buffer[257];
  int  readst;
  if ( (readst=_ieeerd(ieee,buffer,256)) < 0 ) { return readst; }
  buffer[256]=(char)0;
  return sscanf(buffer,format,a,b,c,d,e);
}

/*****************************************************************************
 *
 * int rawmode(handle) -- sets file described by 'handle' into raw mode.
 *
 * In raw mode control characters returned from the file are not checked.
 * In particular, control-Z does not mark the end-of-file.  This is very
 * useful when trying to read binary data from files (including the IEEE
 * driver). Returns -1 if an error occurred.
 *
 ****************************************************************************/

int rawmode(handle)
  int handle;
{ union REGS regs;

  regs.x.ax=0x4400;	/* Get device data */
  regs.x.bx=handle;
  intdos(&regs,&regs);
  if (regs.x.cflag) {
    errno=regs.x.ax;
    printf("0x4400 rawmode error: %d",regs.x.ax);
    return -1;
  }

  regs.x.dx |= 0x20;	/* Set raw mode in device attributes */
  regs.x.dx &= 0xFF;	/* Set DH=0 */

  regs.x.ax=0x4401;	/* Set device data */
  regs.x.bx=handle;
  intdos(&regs,&regs);
  if (regs.x.cflag) {
    errno=regs.x.ax;
    printf("0x4401 rawmode error: %d",regs.x.ax);
    return -1;
  }
 return 0;
}

/****************************************************************************
 *
 * int ieeeinit() -- initializes Personal488 for I/O.
 *
 * Opens the external file 'ieee' for communication with Personal488,
 * sets the file into raw mode, sends IOCTL "BREAK" to get the attention
 * of Personal488, sends the "RESET" command to reset the system.  Sets
 * EOL OUT to line feed so that command strings need only be follwed by
 * \n, and sets EOL IN to NULL ($000) so that returned data is automatically
 * made into a valid string.
 *
 * Returns -1 if an error occurred.
 *
 ****************************************************************************/

int ieeeinit()
{ if ( ( (ieee=open("ieee",O_RDWR | O_BINARY))	== -1) ||
         (rawmode(ieee)             		== -1) ||
         (ioctl_io(ieee,"break",5,0x4403)  	== -1) ||  /* Direct call instead of macro */
         (_ieeewt(ieee,"reset\r\n")       	== -1) ||  /* Direct call */
         (_ieeewt(ieee,"eol out lf\r\n")  	== -1) ||  /* Direct call */
         (_ieeewt(ieee,"eol in $0\n")     	== -1) ||  /* Direct call */
         (_ieeewt(ieee,"fill error\n")      	== -1)) return -1; /* Direct call */
  return 0;
}