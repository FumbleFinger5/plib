#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "pdef.h"
#include "flopen.h"
#include "str.h"
#include "log.h"

#define SE_SJHLOG		(-324)

char	logpath[FNAMSIZ];


void sjhlog(const char *fmt,...)
{
if (!fmt) {logpath[0]=0;return;}
char w[1024];
if (!logpath[0])
	{
//	strcpy(logpath,"/home/steve/SJH.LOG");
	strfmt(logpath,"/home/%s/SJH.LOG", getenv("USER"));
	if (!fmt[0]) return;						// If first call would just write blank line don't bother
	}
va_list va; va_start(va,fmt);
_strfmt(w,fmt,va);

extern int flopen_status;
flopen_status=2;
HDL f=flopen(logpath,"a");
if (!f) throw(SE_SJHLOG);
flputln(w,f);
flclose(f);
}


