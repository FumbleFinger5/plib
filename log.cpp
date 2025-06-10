#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "pdef.h"
#include "flopen.h"
#include "str.h"
#include "log.h"
#include "memgive.h"

int sjh_print;

#define SE_SJHLOG		(-324)

char	logpath[FNAMSIZ];


void sjhlog(const char *fmt,...)
{
if (sjh_print == 1 || sjh_print == 2)
   {
   va_list args_for_console;
   va_start(args_for_console, fmt);
   vprintf(fmt, args_for_console); // Print to console
   va_end(args_for_console);       // Clean up
//   printf("\n");                   // Add newline for console clarity
   }
if (sjh_print == 0 || sjh_print == 2)
   {
   if (!fmt) {logpath[0]=0;return;}
   char w[2048], *_w=w, *ww=NULL;
   if (!logpath[0])
      {
      strfmt(logpath,"/home/%s/SJH.LOG", getenv("USER"));
      if (!fmt[0]) return;						// If first call would just write blank line don't bother
      }
   va_list va; va_start(va,fmt);

   va_list va_copy;                       // Create a copy of the va_list
   va_copy(va_copy, va);

   int sz = vsnprintf(nullptr, 0, fmt, va_copy);
   if (sz>=sizeof(w)) _w=ww=(char*)memgive(sz+1);
   _strfmt(_w,fmt,va);

   extern int flopen_status;
   flopen_status=2;
   HDL f=flopen(logpath,"a");
   if (!f) {printf("Can't open logfile [%s]",logpath); throw(SE_SJHLOG);}
   if (ww!=NULL) flputln(strfmt(w,"Allocated %d+1 bytes for next line...",sz),f);
   flputln(_w,f);
   flclose(f);
   memtake(ww);		// does nothing if pointer was never allocated
   }
}


