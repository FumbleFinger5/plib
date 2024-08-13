#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "pdef.h"
#include "flopen.h"
#include "str.h"
#include "log.h"
#include "memgive.h"

#define SE_SJHLOG		(-324)

char	logpath[FNAMSIZ];


void sjhlog(const char *fmt,...)
{
if (!fmt) {logpath[0]=0;return;}
char w[2048], *_w=w, *ww=NULL;
if (!logpath[0])
	{
	strfmt(logpath,"/home/%s/SJH.LOG", getenv("USER"));
	if (!fmt[0]) return;						// If first call would just write blank line don't bother
	}
va_list va; va_start(va,fmt);
//_strfmt(w,fmt,va);

// Create a copy of the va_list
va_list va_copy;
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


