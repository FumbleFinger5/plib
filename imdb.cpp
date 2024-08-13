#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <utime.h>
#include <stdlib.h>

#include <time.h>
#include <string.h>
#include <cstdarg>

#include "pdef.h"
#include "db.h"
#include "cal.h"
#include "drinfo.h"
#include "flopen.h"
#include "log.h"
#include "memgive.h"
#include "str.h"
#include "smdb.h"
#include "omdb1.h"
#include "dirscan.h"
#include "parm.h"
#include "exec.h"

#include "imdb.h"
#include "my_json.h"
#include "qblob.h"
//#include <json-c/json.h>

#ifdef multistringing
DYNAG *multistring2dynag(char *av, int avsz)
{
DYNAG *t=new DYNAG(0);
while (avsz>0)
    {
    t->put(av);
    int len=strlen(av)+1;
    av+=len;
    avsz-=len;
    }
return(t);
}

static void list_multistring(char *a, int sz)
{
while (sz>0)
    {
    sjhlog("[%s]",a);
    int len=strlen(a)+1;
    sz-=len;
    a+=len;
    }
}
#endif


IMDB_API::IMDB_API(const char *override_fn)
{
char fn[256];
if (override_fn) strancpy(fn,override_fn,sizeof(fn));
else get_conf_path(fn, "imdb.api");
if (access(fn, F_OK ))		// mode=F_OK=0, where non-zero return value means file doesn't exist AT ALL
	{						// mode could be either or both (R_OK|W_OK) for "User has Read / Write access"
	Xecho("Creating database %s\r\n",fn);
	if (!dbist(fn) || (db=dbopen2(fn))==NULLHDL) goto err;
	memset(&hdr,0,sizeof(hdr));
	hdr.ver=1;
	hdr.im_rhdl=btrist(db,DT_ULONG,sizeof(kyx));  // XTNDed Key ... (short) LastUpd + (long) RHDL2
	dbsetanchor(db,recadd(db,&hdr,sizeof(hdr)));
	dbsafeclose(db);
	}
db_open(fn);
memset(&kyx,0,sizeof(kyx));
return;
err:
m_finish("Error creating %s",fn);
}

void IMDB_API::db_open(char *fn)
{
db=dbopen2(fn);
recget(db,dbgetanchor(db),&hdr,sizeof(hdr));						// Read anchor record
if ( !hdr.ver || (im_btr=btropen(db,hdr.im_rhdl))==NULLHDL)
	m_finish("Error reading %s",fn);
}

bool IMDB_API::del(int32_t imno)
{
RHDL rh;
kyx.imno=imno;
if (!bkydel(im_btr,&rh,&kyx)) return(false);
recdel(db,rh);
return(true);
}

void IMDB_API::log(void)
{
sjhlog("tt%07d  Updated:%s rh2:%d",kyx.imno,dmy_stri(kyx.bd),kyx.rh2);
}

// return alloc'd string value of metric 'name' (empty string if n/f)
// If name==NULL return ptr -> "class-local" copy of ENTIRE api returned buffer (or NULL if no record)
const char *IMDB_API::get(int32_t imno, const char *name)
{
RHDL rh;
if (imno!=kyx.imno)
    {
    kyx.imno=imno;
    memset(cachebuf,0,sizeof(cachebuf));
    if (!bkysrch(im_btr,BK_EQ,&rh,&kyx))
        return(NULL); // all 10 bytes of 'kyx' get updated
    int sz=zrecsizof(db,rh);
    if (sz<=100 || sz>sizeof(cachebuf)) throw(87);
    if (zrecget(db,rh,cachebuf,sz)!=sz) throw(88);
    }
if (name==NULL) return(cachebuf);
//return(strquote(cachebuf, name));   // return ptr-> (alloc'd) copy of named metric string
json_object *parser = json_tokener_parse(cachebuf);
char *ptr=jstr4(parser, name);
json_object_put(parser); // Clean up
return(ptr);   // return ptr-> (alloc'd) copy of named metric string
}

DYNAG *IMDB_API::get_tbl(void)	// return table of all imdb numbers in imdb.api file
{
int ct=btrnkeys(im_btr);
DYNAG *di=new DYNAG(sizeof(int32_t),ct);
int again=0;
int32_t imno;
while (bkyscn_all(im_btr,NULL,&imno,&again))
    di->put(&imno);
return(di);
}

void IMDB_API::put(int32_t imno, const char *buf)
{
int len;
kyx.imno=imno;
kyx.bd=short_bd(calnow());
kyx.rh2=NULLRHDL;
if (buf==NULL || (len=strlen(buf))<100) throw(87);
IA_KEY prv={imno};
RHDL rh;
if (bkysrch(im_btr,BK_EQ,&rh,&prv))
    {
    rh=zrecupd(db,rh, buf,len+1);
    bkyupd(im_btr,rh,&kyx);
    }
else
    bkyadd(im_btr,zrecadd(db,buf,len+1),&kyx);
}


IMDB_API::~IMDB_API() {dbsafeclose(db);jstr4(0,0);}


// Call API with ImdbNo. Populate passed 'buf' with the values returned by api
bool api_all_from_number(int32_t imno, char *buf8k)
{
char cmd[256];
strfmt(cmd,"%s%s&i=tt%07d' %s", "curl 'http://www.omdbapi.com/?apikey=", apikey(), imno, STD_ERRNULL);
int err=exec_cmd(cmd, buf8k, 8192);
//sjhlog("%s:%d\n%s","Api_all_from_number",err,cmd);
//sjhlog("%s",buf8k);
if (err) {sjhLog("Usherette EXEC FAILED: %s",cmd); sjhLog("FAIL buffer [%s]",buf8k); throw(83);}
if (stridxs("Request limit reached!",buf8k)!=NOTFND)
    {sjhLog("API limit reached for key %s",apikey()); return(false);}
return(true);
}

bool api_all_from_name(const char *name, int year, char *buf8k)
{
char cmd[256];
strfmt(cmd,"%s%s%s%s","curl 'https://www.omdbapi.com/?apikey=", apikey(), "&t=\"", name);
strendfmt(cmd,"\"&y=%d' %s",year, STD_ERRNULL);
int err=exec_cmd(cmd, buf8k, 8192);
//sjhlog("%s:%d\n%s","Api_all_from_name",err,cmd);
// IF buffer contains "Incorrect IMDb ID." THEN call themoviedb instead
//sjhlog("%s",buf8k);
return(err==0);
}

