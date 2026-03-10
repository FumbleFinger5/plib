void sjhlog(const char *fmt,...);
void SJHLOG(const char *fmt,...);      // wrapper function logs to both file AND console

#define Sjhlog sjhlog		// genuine quick&dirty way of reporting where program crashes
#define sjhLog sjhlog		// debugging call that wouldn't normally be made

//extern int sjh_print;      // used by sjhlog()  -  0=log to file, 1=log to console, 2=log to BOTH


