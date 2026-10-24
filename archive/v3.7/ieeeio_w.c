/*
 * Personal488 IEEEIO.C helper library, ported to OpenWatcom (stdarg.h, ANSI ieeeprtf).
 */

#include <fcntl.h>
#include <dos.h>
#include <io.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "ieeeio.h"

int ieee; /* Driver488 handle used by the ieee* routines below */

/*
 * segment()/offset() come from ieeeio.h. Small/medium models need segment() as a
 * function (below) to fetch DS; large-data models get both as macros.
 */

#if defined(M_I86SM) || defined(M_I86MM)    /* small or medium model */
/******************** SMALL DATA MODEL *************************************/
int segment(ptr)
  void near *ptr;
{ static struct SREGS segs = { 0, 0, 0, 0 };
  if (segs.ds==0) segread(&segs);
  return segs.ds;
}
#endif

/*
 * ioctl_io() reads (iocall 0x4402) or writes (0x4403) the driver's IOCTL control
 * channel, like read()/write() but bypassing the device data path. Driver488 takes
 * "BREAK" this way and reports its internal state (Personal488 manual, ch. 6).
 */

extern int errno;

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
  regs.x.dx=(unsigned int)chars;
  segs.ds=segment(chars);
  intdosx(&regs,&regs,&segs);
  if (regs.x.cflag) {
    errno=regs.x.ax;
    return -1;
  } else {
    return regs.x.ax;
  }
}

/*
 * Driver488 can report events as a light-pen interrupt. Before each write,
 * _ieeewt() calls (*ieee_cki)() (cklpint or _false_) and, if it returns true,
 * runs (*ieee_isr)() (no_op unless the program installs a handler).
 */

/* True if a light-pen interrupt is pending. */
int cklpint()
{ union REGS regs;
  char buf;
  unsigned int firstresponse;

  if (ioctl_io(ieee,&buf,1,0x4402) != 1) return 0;
  if (buf != '0') return 0;
  regs.x.ax=0x0400; /* INT 10h fn 4: read light pen status */
  int86(0x10,&regs,&regs);
  firstresponse=regs.h.ah;
  regs.x.ax=0x0400;
  int86(0x10,&regs,&regs);
  return (regs.h.ah | firstresponse);
}

int _false_()
{ return 0; }

int (*ieee_cki)() = _false_;

void no_op()
{ }

void (*ieee_isr)() = no_op;

/*
 * _ieeewt(handle,chars) writes a NUL-terminated string; _ieeerd(handle,chars,size)
 * reads up to size bytes. Both print the errno on failure and return -1.
 */

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

/*
 * ieeeprtf()/ieeescnf() are printf/scanf to and from Personal488.
 * ieeeprtf formats into 128 bytes; ieeescnf takes at most 5 arguments.
 */

int ieeeprtf(char *format, ...)
{ 
  char buffer[128];
  va_list arg_ptr;
  int result;

  va_start(arg_ptr, format);
  vsprintf(buffer, format, arg_ptr);
  va_end(arg_ptr);
  
  return _ieeewt(ieee, buffer);
}

int ieeescnf(format,a,b,c,d,e)
char *format,*a,*b,*c,*d,*e;
{ char buffer[128];
  int  readst;
  if ( (readst=_ieeerd(ieee,buffer,256)) < 0 ) { return readst; }
  buffer[256]=(char)0;
  return sscanf(buffer,format,a,b,c,d,e);
}

/*
 * Put a device handle in raw mode so DOS stops interpreting control characters
 * (Ctrl-Z would otherwise end a read of driver data). Returns -1 on error.
 */

int rawmode(handle)
  int handle;
{ union REGS regs;

  regs.x.ax=0x4400;
  regs.x.bx=handle;
  intdos(&regs,&regs);
  if (regs.x.cflag) {
    errno=regs.x.ax;
    printf("0x4400 rawmode error: %d",regs.x.ax);
    return -1;
  }

  regs.x.dx |= 0x20;	/* Set raw mode in device attributes */
  regs.x.dx &= 0xFF;	/* Set DH=0 */

  regs.x.ax=0x4401;
  regs.x.bx=handle;
  intdos(&regs,&regs);
  if (regs.x.cflag) {
    errno=regs.x.ax;
    printf("0x4401 rawmode error: %d",regs.x.ax);
    return -1;
  }
 return 0;
}

/*
 * Open the "ieee" device in raw mode, IOCTL BREAK for the driver's attention, then
 * RESET; EOL OUT LF so commands need only end in \n, EOL IN $0 so replies arrive
 * NUL-terminated, FILL ERROR. Returns -1 on error.
 */

int ieeeinit()
{ if ( ( (ieee=open("ieee",O_RDWR | O_BINARY))	== -1) ||
         (rawmode(ieee)             		== -1) ||
         (ioctl_io(ieee,"break",5,0x4403)  	== -1) ||
         (_ieeewt(ieee,"reset\r\n")       	== -1) ||
         (_ieeewt(ieee,"eol out lf\r\n")  	== -1) ||
         (_ieeewt(ieee,"eol in $0\n")     	== -1) ||
         (_ieeewt(ieee,"fill error\n")      	== -1)) return -1;
  return 0;
}