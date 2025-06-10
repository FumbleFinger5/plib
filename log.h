void sjhlog(const char *fmt,...);

#define Sjhlog sjhlog		// genuine quick&dirty way of reporting where program crashes
#define sjhLog sjhlog		// debugging call that wouldn't normally be made

extern int sjh_print;      // 0=log to file, 1=log to console, 2=log to BOTH



