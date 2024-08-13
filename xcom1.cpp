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
leak_tracker(YES);
//set_search_path(main_pth);
//tiv=new TI_VAR();

//buff=(char*)memgive(65000*4);
//Dbstart(32); //32);

//if (*ti_var("DBSAFE",wrk,TIV_UPPER)=='N') dbsafe_catch=0; else
//dbsafe_catch=DB_IS_DIRTY;
//setup_sy(YES);
//sy_init();
}

int global_closedown(void)
{
//setup_sy(NO);
//Dbstop();
//clear_callbacks();
//XStatusCallback(NOTFND,NOTFND);
//Scrap(curv);
//Scrap(buff);
//set_search_path(0);
int err=NO;
//Xecho(0);
int lk=leak_tracker(NO);

//sjhLog(0);	// clears 'current log file' path
return(lk!=0);
}

