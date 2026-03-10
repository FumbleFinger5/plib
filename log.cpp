#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "pdef.h"
#include "flopen.h"
#include "str.h"
#include "log.h"
#include "memgive.h"

extern int flopen_status;

#define LOG_TO_FILE 1
#define LOG_TO_CONSOLE 2

#define SE_SJHLOG		(-324)

char	logpath[FNAMSIZ];

// New internal helper function that takes va_list
static void _sjhlog_internal(int logflag, const char *fmt, va_list args_original)
{  // A va_list can only be traversed once.
if (logflag&LOG_TO_CONSOLE)
   {
   va_list args_for_console;
   va_copy(args_for_console, args_original); // Copy for vprintf
   vprintf(fmt, args_for_console);
   va_end(args_for_console);
   }
if (logflag&LOG_TO_FILE)
   {
   if (!fmt) { logpath[0] = 0; return; }     // don't really need now logpath is static char[], not heap allocated 
   char wrk[2048];         // default buffer on stack
   char *buffer = NULL;    // temporary dynamically allocated buffer if stack buffer 'wrk' not big enough
   char *ptr = wrk;        // ptr to ACTUAL buffer used
   if (!logpath[0])
      {strfmt(logpath, "/home/%s/SJH.LOG", getenv("USER")); if (!fmt[0]) return;} // If first call would just write blank line don't bother
   va_list args_null;                  // each va_list can only be used ONCE. This one is for the vsnprintf(NULL,...) call
   va_copy(args_null, args_original);  // ...used to find out how many characters will ACTUALLY be written to output
   int sz = vsnprintf(nullptr, 0, fmt, args_null);
   va_end(args_null); // Clean up after use
   if (sz < 0) {printf("Error in vsnprintf for logfile: returned %d\n", sz); return;}
   if (sz >= sizeof(wrk)) ptr = buffer = (char*)memgive(sz + 1); // +1 for null terminator
   va_list args_for_strfmt;                     // Now we've made sure the o/p buffer is big enough...
   va_copy(args_for_strfmt, args_original);     // ...make another va_list for the actual formatting
   _strfmt(ptr, fmt, args_for_strfmt); // Assuming _strfmt takes va_list
   va_end(args_for_strfmt); // Clean up after use
   flopen_status = 2;
   HDL f = flopen(logpath, "a");
   if (!f) {printf("Can't open logfile [%s]\n", logpath); throw(SE_SJHLOG);}
//   if (buffer != NULL) {char tmp[256]; snprintf(tmp, sizeof(tmp), "Allocated %d+1 bytes for next line...", sz); flputln(tmp, f);}
   flputln(ptr, f);
   flclose(f);
   memtake(buffer); // does nothing if pointer was never allocated
   }
}


// Original sjhlog, now a wrapper for _sjhlog_internal
void sjhlog(const char *fmt,...)
{
va_list args;
va_start(args, fmt);
_sjhlog_internal(LOG_TO_FILE, fmt, args);
va_end(args);
}

// SJHLOG wrapper, now correctly passes va_list to _sjhlog_internal
void SJHLOG(const char *fmt,...)
{
va_list args;
va_start(args, fmt);
_sjhlog_internal(LOG_TO_FILE|LOG_TO_CONSOLE, fmt, args); // Call the internal function with the va_list
va_end(args);
printf("\n");
}
