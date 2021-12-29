#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pdef.h"
#include "memgive.h"
#include "str.h"
#include "flopen.h"
#include "drinfo.h"
#include "db.h"


void global_init(const char *main_pth)
{
//leak_tracker(YES);
//set_search_path(main_pth);
//tiv=new TI_VAR();

//buff=(char*)memgive(65000*4);
dbstart(32); //32);

//if (*ti_var("DBSAFE",wrk,TIV_UPPER)=='N') dbsafe_catch=0; else
//dbsafe_catch=DB_IS_DIRTY;
//setup_sy(YES);
//sy_init();
}

int global_closedown(void)
{
//setup_sy(NO);
dbstop();
//clear_callbacks();
//XStatusCallback(NOTFND,NOTFND);
//Scrap(curv);
//Scrap(buff);
//set_search_path(0);
int err=NO;
//Xecho(0);
//err=leak_tracker(NO);

#ifdef DLLLOG
extern int	dlllog;
if ((dlllog>0 && !nolog_this_dll()) || err)
	{
	delete new DllLog(0,0);
	extern int first_leak;
	char str[256];
	strcpy(str,"global_closedown ");
	if (err) strendfmt(str,"%d memory leaks! First=%d",err,first_leak);
	else strcat(str,"OK");
	sjhLog("%s:%s",caller_name,str);
	}
#endif // DLLLOG

//sjhLog(0);	// clears 'current log file' path
return(err);
}

