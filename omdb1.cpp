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
//#include "csv.h"
#include "dirscan.h"
#include "parm.h"
#include "qblob.h"

#include "imdb.h"



char *fix_colon(char *nam)	// Change any ":" in MovieName to " -" and delete any '?'
{
char *p;
while ((p=strchr(nam,':'))!=NULL) strins(strdel(p,1)," -");
while ((p=strchr(nam,'?'))!=NULL) strdel(p,1);
return(nam);		// Return passed string address as a convenience
}

int32_t tt_number_from_str(const char *s)
{
if (*s=='_') s++;
if (SAME2BYTES(s,"tt")) s+=2;
return(a2l(s,0));
}


int user_not_steve;
static void get_user(void)
{
char *usr=getenv("USER");
if (usr==NULL || strcmp(usr,"steve"))   // read-only mode unless user is ME!
    user_not_steve=YES;     // Accessed by q3:window.cpp AND plib:omdb.cpp
}

static void flset_dttm(const char *dst, const char *src) // set dttm of 'dst' filename to match that of 'src'
{
FILEINFO fi;
if (!drinfo(src, &fi)) throw(99);
utimbuf ut = {0, fi.dttm};              
utime(dst,&ut);
}

void OMDB1::db_open(char *fn)
{
RHDL anchor, rh;
int again=NO, imno;
while (YES)
    {
    if ((db=dbopen2(fn))==NULLHDL) m_finish("Error opening %s",fn);
    recget(db,anchor=dbgetanchor(db),&hdr,sizeof(hdr));						// Read anchor record
    if (!hdr.ver || (om_btr=btropen(db,hdr.om_rhdl))==NULLHDL) m_finish("Error reading %s",fn);
    if (hdr.ver==3) return;
    if (again) m_finish("failed AGAIN!");
    hdr.ver=3;
    recupd(db,anchor,&hdr,sizeof(hdr));
    while (bkyscn_all(om_btr, &rh, &imno, &again))
        {
        OM1_KEY omk;
        zrecget(db,rh,&omk,sizeof(OM1_KEY));
        memset(&omk.mytitle,0,16);
        omk.seen=long_bd(omk.sz);     // in v2 format date Seen was stored here as a short
        omk.sz=0;
        zrecupd(db,rh,&omk,sizeof(OM1_KEY));
        }
    dbsafeclose(db);
sjhlog("db1 format updated to v3");
    }
}

OMDB1::OMDB1(bool _master)		// If 'master', open OmDb.db2, not *.db1
{
char fn[256];
    {
    master=_master;
    if (master)
        get_conf_path(fn,"smdb.mst");   // TODO - filenames!
    else
        get_conf_path(fn,"smdb.usr");
    }
if (access(fn, F_OK ))		// mode=F_OK=0, where non-zero return value means file doesn't exist AT ALL
	{						// mode could be either or both (R_OK|W_OK) for "User has Read / Write access"
	Xecho("Creating database %s\r\n",fn);
	if (!dbist(fn) || (db=dbopen2(fn))==NULLHDL) goto err;
	memset(&hdr,0,sizeof(hdr));
	hdr.ver=3;
	hdr.om_rhdl=btrist(db,DT_ULONG,sizeof(int32_t));
	dbsetanchor(db,recadd(db,&hdr,sizeof(hdr)));
	dbsafeclose(db);
	}
db_open(fn);
return;
err:
m_finish("Error creating %s",fn);
}

OMDB1::~OMDB1()
{
dbsafeclose(db);
}
void OMDB1::put(OM1_KEY *k)
{
RHDL rh;
if (bkysrch(om_btr,BK_EQ,&rh,&k->imno)) m_finish("rec already exists!");
rh=recadd(db,k,sizeof(OM1_KEY));
bkyadd(om_btr,rh,&k->imno);
}

bool OMDB1::upd(OM1_KEY *k)
{
RHDL rh;
if (bkysrch(om_btr,BK_EQ,&rh,&k->imno))
    {
    recupd(db,rh,k,sizeof(OM1_KEY));
    return(true);
    }
return(false);
}

bool OMDB1::get_om1(int32_t imno, OM1_KEY *k)
{
RHDL rh;
if (!bkysrch(om_btr,BK_EQ,&rh,&imno)) return(NO);
if (recget(db,rh,k,sizeof(OM1_KEY))!=sizeof(OM1_KEY) || k->imno!=imno) throw(88);
return(YES);
}

bool OMDB1::get_ge(OM1_KEY *om1)
{
RHDL rh;
int32_t imno=om1->imno;
if (!bkysrch(om_btr,BK_GE,&rh,&imno)) return(NO);
if (recget(db,rh,om1,sizeof(OM1_KEY))!=sizeof(OM1_KEY) || om1->imno!=imno) throw(88);
return(YES);
}

bool OMDB1::scan_all(OM1_KEY *k, bool *again)
{
RHDL rh;
if (!*again) {*again=true; k->imno=0;}
if (!bkysrch(om_btr,BK_GT,&rh,&k->imno)) return(NO);
if (recget(db,rh,k,sizeof(OM1_KEY))!=sizeof(OM1_KEY)) throw(88);
return(true);
}

bool OMDB1::del(int32_t imno)
{
RHDL rh;
OM1_KEY om1;
om1.imno=imno;
if (!bkysrch(om_btr,BK_EQ,&rh,&om1.imno)) {Sjhlog("Can't delete imno:%d from %s",imno,filename()); return(NO);}
if (zrecget(db,rh,&om1,sizeof(OM1_KEY))!=sizeof(OM1_KEY)) throw(88);
recdel(db,om1.mytitle); // does nothing if no custom title (i.e. mytitle rhdl is NULL)
recdel(db,om1.notes);
recdel(db,rh);
bkydel(om_btr,&rh,&om1.imno);
Sjhlog("Deleted imno:%d from %s",imno,filename());
return(YES);
}

int OMDB1::get_rating(int32_t imno)
{
OM1_KEY om1;
if (get_om1(imno,&om1)) return(om1.rating);
return(0);
}

void OMDB1::put_rating(int32_t imno, int rating, OM1_KEY *omzk)  // omz==NULL if non-steve rating change within q3
{
OM1_KEY omk;
memset(&omk,0,sizeof(OM1_KEY));
RHDL rh=0;    // have to search AGAIN to get the RH value TODO - add option for Get_om1() to do that
if (get_om1(omk.imno=imno,&omk))
    bkysrch(om_btr,BK_EQ,&rh,&imno);    // CAN'T fail, 'cos it just worked on precediing line!
if (omzk==NULLPTR) omk.seen=calnow();   // if just adding/amending record for user_not_steve database 
else
    {
    if (rh==0) m_finish("put_rating get_om1 FAILED for %d",imno);
    if (omzk->sz>omk.sz) omk.sz=omzk->sz;     // This is a more recent (larger) copy of the movie
    int32_t now=calnow();
    if (short_bd(omk.seen)<short_bd(now)-30) omk.seen=now;        // not "re-watched" unless more than a month ago
    }
omk.rating=rating;
if (rh!=0) recupd(db,rh,&omk,sizeof(OM1_KEY));
else  bkyadd(om_btr,recadd(db,&omk,sizeof(OM1_KEY)),&omk);
}

std::string OMDB1::get_notes(int32_t imno)
{
OM1_KEY om1;
if (!get_om1(imno,&om1) || om1.notes==NULLRHDL) return((char*)memgive(1));
int sz;
char *txt=(char*)zrecmem(db,om1.notes,&sz);
if (sz==0 || txt[sz-1]!=0) throw(81);   // Notes record must be non-zero length, terminated by nullbyte
std::string sstr(txt);
memtake(txt);
return(sstr);
}

void OMDB1::put_notes(int32_t imno, const char *txt)
{
OM1_KEY om1;
int sz = strlen(txt);
if (!get_om1(imno,&om1)) // There's not YET a user record for this inum
    {
    if (sz)      // so only add a record if notes exist
        {
        memset(&om1,0,sizeof(OM1_KEY));
        om1.imno=imno;
        om1.notes=zrecadd(db,txt,sz+1);
        bkyadd(om_btr,recadd(db,&om1,sizeof(OM1_KEY)),&om1.imno);
        }
    return;
    }
RHDL rh;
bkysrch(om_btr,BK_EQ,&rh,&om1.imno);    // CAN'T fail, 'cos it just worked on precediing call!
if (om1.notes==NULLRHDL)
    {
    if (sz==0) return;
    om1.notes=zrecadd(db,txt,sz+1);
    }
else
    {
    if (sz==0) {recdel(db,om1.notes); om1.notes=NULLRHDL;}
    else om1.notes=zrecupd(db,om1.notes,txt,sz+1);
    }
recupd(db,rh,&om1,sizeof(OM1_KEY));
}

#include "imdbf.h"

void OMDB1::list_missing(void)  // List all movies 
{
Xecho("List missing movies...\r\n");
RHDL rh;
int ct=0, again=0;
OM1_KEY om1;
SMDB s;
//IMDB_FLD im;                // Only used to get titles to show alongside missing imdbID numbers
int rct=btrnkeys(om_btr);
short today=short_bd(calnow());
while (bkyscn_all(om_btr,&rh,&om1,&again))
    {
    if (recget(db,rh,&om1,sizeof(OM1_KEY))!=sizeof(OM1_KEY)) throw(88);
    DYNTBL *dt=s.get(om1.imno);
    if (dt->ct==0 && om1.added>(today-300))
        {
        char buf[128];
strcpy(buf,"(didn't look up moviename)");
//im.get(om1.imno,FID_TITLE,buf);
        Xecho("%d  %s\r\n",om1.imno,buf);
        ct++;
        }
    delete dt;
    }
Xecho("%d missing movies listed\r\n",ct);
}

DYNAG* USRTXT::extract(const char *subrec_name)
{
std::string sstr=get();
char s[2048], *p;
strcpy(p=s,sstr.c_str());
int i,j, namelen=strlen(subrec_name);
DYNAG *d=new DYNAG(0); d->cargo(subrec_name, namelen+1);
while ((i=stridxc('{',p))!=NOTFND)
    {
    p+=(i+1);       // Point to first character AFTER '{'
    if (memcmp(p,subrec_name,namelen) || p[namelen]!=EQUALS) continue;    // Not a wanted subrec? Keep looking
    p+=namelen;
    if ((j=stridxc('}',p))==NOTFND) {sjhlog("unmatched { in %s notes",subrec_name); break;}
    p[j]=0;         // change '}' to EOS /0 in temporary copy 's'
    d->put(p+1);    // store the text between '{subrec_name=' and '}'
    p+=(j+1);       // move pointer to first char AFTER the '}' that ends THIS subrec 
    }
return(d);
}

// replace ANY&ALL existing subrecs for this name (in cargo) by anything in (maybe EMPTY) passed table 
void USRTXT::insert(DYNAG *subrec_tbl)
{
char *subrec_name=(char*)subrec_tbl->cargo(0);
std::string sstr=get();
char s[2048], *p;
strcpy(p=s,sstr.c_str());
int i,j, namelen=strlen(subrec_name);
while ((i=stridxc('{',p))!=NOTFND)
    {
    p+=(i+1);       // Point to first character AFTER '{'
    if (memcmp(p,subrec_name,namelen)) continue;    // Not a wanted subrec? Keep looking
    if ((j=stridxc('}',p))==NOTFND) {sjhlog("UNmatched { in %s notes",subrec_name); break;}
    strdel(p-1,j+2);
    }
for (i=0;i<subrec_tbl->ct;i++)
    strendfmt(s,"{%s=%s}",subrec_name,(char*)subrec_tbl->get(i));
put(s);
}