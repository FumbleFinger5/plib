#define USE_QSETTINGS YES

#ifdef USE_QSETTINGS
#include <QApplication>
#include <QTime>
#include <QSettings>
#endif 

#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <utime.h>

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
#include "omdb.h"
#include "csv.h"
#include "dirscan.h"
#include "parm.h"
#define PARM_FN "/home/steve/.config/Softworks/SMDB.conf"

#define CURLE "2>/dev/null"
//#define CURLE ""

int omdb_read_only;

static void fgets_no_lf(char *buf, int maxct, FILE *f)
{
fgets(buf, maxct-1, f);
int i=strlen(buf);
buf[i-1]=0;
}

static int exec_cmd(const char *cmd, char *buf, int bufsz)
{
FILE *f = popen(cmd,"r");
fgets_no_lf(buf, bufsz, f);
int err=pclose(f);
if (err<0) throw(77);
return(err);
}

// Return an ALLOCATED null-terminated string (length 'len' + EOS byte)
// If passed 'str' was already allocated AND contains a string of at least 'len', just use it
// If 'str' is NULL or contains a SHORTER string, reallocate/resize before copying 'ptr' text
static char *copyhack(char *str, char *ptr, int len)
{
if (len<1) len=0;
if (str==NULLPTR || strlen(str)<len) str=(char*)memrealloc(str,len+1);
if (len) memmove(str,ptr,len);
str[len]=0;
return(str);
}

// Return ALLOCATED pointer to string containing the text value of 'id' field within  'buf'
// If 'buf' doesn't contain 'id', return an EMPTY string (ie. - just a null-byte, being the EOS)
static char *strquote(char *buf, const char *id)
{
static char sep[3]={34,58,34};      // ":" separates api returned id (name of fld) from "contents"
static char *a[4];
static int n=NOTFND; n=(n+1)&3;     // cycles 0,1,2,3,0,1,2,3,0,...
if (buf==NULLPTR) {for (n=3;n>=0;n--) Scrap(a[n]); return(0);}  // "cleanup" call with nullptr
int p=0, q, idlen=strlen(id), buflen=strlen(buf), vallen=NOTFND;
while ((q=stridxc(CHR_QTDOUBLE,&buf[p]))!=NOTFND)
    {
    if (p+q+idlen+5 > buflen) break; // insufficient remaining text to contain sought key+value pair
    if (memcmp(&buf[p+=(q+1)],id,idlen) || !SAME3BYTES(&buf[p+idlen],sep)) continue; // keep looking
    vallen=stridxc(CHR_QTDOUBLE,&buf[p+=(idlen+3)]); // (shouldn't be NOTFND, but ret="" s/b okay)
    break;
    }
return(a[n]=copyhack(a[n],&buf[p],vallen));
}

// Call API with ImdbNo. If TRUE, 'buf' contains the values returned by api
static bool call_api_with_number(int32_t imdb_num, char *buf, int mxsz)
{
char cmd[256];
strfmt(cmd,"%s%s&i=tt%07d' %s", "curl 'http://www.omdbapi.com/?apikey=", APIKEY, imdb_num, CURLE);
int err=exec_cmd(cmd, buf, mxsz);
//sjhLog("EXEC: %s",cmd);
return(err==0);
}

void OMDB::db_open(char *fn)
{
if ((db=dbopen(fn))==NULLHDL) m_finish("Error opening %s",fn);
recget(db,dbgetanchor(db),&hdr,sizeof(hdr));						// Read anchor record
if ( !hdr.ver || (om_btr=btropen(db,hdr.om_rhdl))==NULLHDL)
	m_finish("Error reading %s",fn);
}

// HALF-HEARTED attempt to populate passed EM if found!
bool OMDB::get(EM_KEY1 *e)	// If e->imdb_num key exists, populate 'e' and return TRUE, else FALSE
{
RHDL rh;
OM_KEY om;
if (!bkysrch(om_btr,BK_EQ,&rh,&e->e.imdb_num)) return(NO);
if (zrecget(db,rh,&om,sizeof(OM_KEY))!=sizeof(OM_KEY)) throw(88);
char w[61];
if (recget(db,om.nam,w,61)>60) throw(86);
strcpy(e->e.nam,w);
e->e.year=om.year;
e->e.rating=om.rating;
return(YES);
}

/*static void write_all_text(char *fn, char *txt)
{
HDL f=flopen(fn,"w");
flput(f,strlen(txt),f);
flclose(f);
sjhLog("Wrote all api text to %s",fn);
}*/

bool OMDB::put(EM_KEY1 *e)	// Add this movie, taking nam+rating  from passed key
{                           // get other fields from imdb API
char buf[4096], w[2048];    // Allow PLENTY of space for the ENTIRE ibmdb API call
if (get((EM_KEY1*)memmove(&buf,e,sizeof(EM_KEY1)))) return(NO); // key already exists - can't add
OM_KEY om;
memset(&om,0,sizeof(OM_KEY));
bool ok=call_api_with_number(om.imdb_num=e->e.imdb_num, buf, sizeof(buf));
if (!ok) throw(83);
om.nam=recadd(db,e->e.nam,strlen(e->e.nam)+1);
om.rating=e->e.rating;
om.year=a2i(strquote(buf,"Year"),4);
om.emdb_num=btrnkeys(om_btr)+1;
strancpy(w,strquote(buf,"Director"),sizeof(EM_KEY1::director));
om.director=people2rhdl(w);
strancpy(w,strquote(buf,"Actors"),sizeof(EM_KEY1::cast));
om.cast=people2rhdl(w);
om.runtime=a2i(strquote(buf,"Runtime"),0);  // Should maybe check format really is "nnn min"
om.added=short_bd(calnow());
om.seen=e->seen;
om.filesz=e->filesz;
bkyadd(om_btr,recadd(db,&om,sizeof(OM_KEY)),&om.imdb_num);
strquote(0,0);              // release allocated strings used to fish data out of returned API buffer
return(YES);
}

bool OMDB::upd(EM_KEY1 *e)	// Only update moviename + rating + seen + filesz
{
RHDL rh;
OM_KEY om;
int upd=NO;
if (!bkysrch(om_btr,BK_EQ,&rh,&e->e.imdb_num)) throw(82);
if (zrecget(db,rh,&om,sizeof(OM_KEY))!=sizeof(OM_KEY)) throw(88);
char w[61];
if (recget(db,om.nam,w,61)>60) throw(86);
if (strcmp(e->e.nam,w)) recupd(db,om.nam,e->e.nam,strlen(e->e.nam)+1);
if (e->e.rating==0 && om.rating!=0) e->e.rating=om.rating;
if (e->seen==0 && om.seen!=0) e->seen=om.seen;
if (om.rating!=e->e.rating)
    {
    if (om.rating!=0 && e->seen-om.seen>31) // Was movie already watched & rated more than a month ago?
        {
        const char *prv;
        int sz;
        if (om.notes==NULLRHDL) prv=(const char*)memgive(sz=1);     // points to an empty string
        else prv=(const char*)zrecmem(db,om.notes,&sz);
        if (sz==0 || prv[sz-1]!=0) throw(81);   // Notes record must be non-zero length, terminated by nullbyte
        char *add=(char*)memgive(32);
        calfmt(add,"%3.3M %4C",long_date(om.seen));
        strendfmt(add,"  %1.1f",0.1*om.rating);
        if (stridxs(add,prv)==NOTFND)       // ONLY add 'rating history' line if not already present
            {
            strcat(add,"\n");
            if (*prv)
                {
                strins(add,"\n");
                add=(char*)memrealloc(add,strlen(add)+sz);  // (sz = prv notes length + 1 for EOS)
                strins(add,prv);
                }
            sz=strlen(add)+1;
            if (om.notes==NULLRHDL) om.notes=zrecadd(db,add,sz);
            else om.notes=zrecupd(db,om.notes,add,sz);
            }
        memtake(add); memtake(prv);
        }
    upd=YES; om.rating=e->e.rating;
    }
if (om.seen!=e->seen) {upd=YES; om.seen=e->seen;}
if (om.filesz!=e->filesz) {upd=YES; om.filesz=e->filesz;}
if (upd) zrecupd(db,rh,&om,sizeof(OM_KEY));
return(YES);
}

void OMDB::fix(int32_t ino, short eno)  // only run ONCE (fix inital eno=9999 values)
{
RHDL rh;
OM_KEY om;
om.imdb_num=ino;
if (!bkysrch(om_btr,BK_EQ,&rh,&om.imdb_num)) throw(82);
if (zrecget(db,rh,&om,sizeof(OM_KEY))!=sizeof(OM_KEY)) throw(88);
om.emdb_num=eno;
zrecupd(db,rh,&om,sizeof(OM_KEY));
}

struct FIX2 {short emdb_num; int32_t imdb_num;};
static int cp_fix2(const FIX2 *a, const FIX2 *b)
{
int cmp=cp_short(&a->emdb_num,&b->emdb_num);
if (!cmp) cmp=cp_int32_t(&a->imdb_num,&b->imdb_num);
return(cmp);
}


void OMDB::fix2(void)
{
int nk=btrnkeys(om_btr);
DYNTBL t(sizeof(FIX2),(PFI_v_v)cp_fix2);
FIX2 fx2, *ff;
RHDL rh;
OM_KEY om;
int again=NO, imdb_num;
while (bkyscn_all(om_btr,&rh,&imdb_num,&again))
    {       // change director, cast contents to "No. of subscripts into RHNM (after OM structure)"
    if (zrecget(db,rh,&om,sizeof(OM_KEY))!=sizeof(OM_KEY)) throw(88);
    if (om.imdb_num!=imdb_num) throw(86);
    fx2.emdb_num=om.emdb_num;
    fx2.imdb_num=imdb_num;
    t.put(&fx2);
    }
short prv=0;
for (int i=0;i<t.ct;i++)
    {
    ff=(FIX2*)t.get(i);
#ifdef FIXING
om.imdb_num=ff->imdb_num;
if (!bkysrch(om_btr,BK_EQ,&rh,&om.imdb_num)) {Sjhlog("Can't delete imdb_num:%d",om.imdb_num); return;}
if (zrecget(db,rh,&om,sizeof(OM_KEY))!=sizeof(OM_KEY)) throw(88);
om.emdb_num=i+1;
zrecupd(db,rh,&om,sizeof(OM_KEY));
#endif
    prv=ff->emdb_num;
    }
}

bool OMDB::del(int32_t imdb_num)
{
RHDL rh;
OM_KEY om;
int32_t num;
num = om.imdb_num=imdb_num;
if (!bkysrch(om_btr,BK_EQ,&rh,&om.imdb_num)) {Sjhlog("Can't delete imdb_num:%d",imdb_num); return(NO);}
if (zrecget(db,rh,&om,sizeof(OM_KEY))!=sizeof(OM_KEY)) throw(88);
recdel(db,om.nam);
recdel(db,om.director);
recdel(db,om.cast);
if (om.notes!=NULLRHDL) recdel(db,om.notes);
recdel(db,rh);
bkydel(om_btr,&rh,&om.imdb_num);

int again=NO;
short emx=om.emdb_num;
while (bkyscn_all(om_btr,&rh,&imdb_num,&again))
    {       // change director, cast contents to "No. of subscripts into RHNM (after OM structure)"
    if (zrecget(db,rh,&om,sizeof(OM_KEY))!=sizeof(OM_KEY)) throw(88);
    if (om.imdb_num!=imdb_num) throw(86);
    if (om.emdb_num>emx)
        {om.emdb_num--; zrecupd(db,rh,&om,sizeof(OM_KEY));}
    }
Sjhlog("Deleted imdb_num:%d from OMDB.dbf AND RENUMBERED ALL EmdbNum GT:%d",num, emx);
return(YES);
}

const char* OMDB::get_notes(int32_t imdb_num)
{
RHDL rh;
OM_KEY om;
om.imdb_num=imdb_num;
if (!bkysrch(om_btr,BK_EQ,&rh,&om.imdb_num)) throw(87);
if (recget(db,rh,&om,sizeof(OM_KEY))!=sizeof(OM_KEY)) throw(88);
if (om.notes==NULLRHDL) return((const char*)memgive(1));
int sz;
const char *txt=(const char*)zrecmem(db,om.notes,&sz);
if (sz==0 || txt[sz-1]!=0) throw(81);   // Notes record must be non-zero length, terminated by nullbyte
return(txt);
}

void OMDB::put_notes(int32_t imdb_num, const char *txt)
{
RHDL rh;
OM_KEY om;
om.imdb_num=imdb_num;
int sz = strlen(txt);
if (!bkysrch(om_btr,BK_EQ,&rh,&om.imdb_num)) throw(87);
if (recget(db,rh,&om,sizeof(OM_KEY))!=sizeof(OM_KEY)) throw(88);
if (om.notes==NULLRHDL)
    {
    if (sz==0) return;
    om.notes=zrecadd(db,txt,sz+1);
    }
else
    {
    if (sz==0) {recdel(db,om.notes); om.notes=NULLRHDL;}
    else om.notes=zrecupd(db,om.notes,txt,sz+1);
    }
recupd(db,rh,&om,sizeof(OM_KEY));
}

int	OMDB::namsub(RHDL rh_list, DYNAG *d)	// return number of namesubscripts added to passed tbl (of shorts)
{
int i, ct, added=0;
RHDL rh[17];    // 16 = maximum FEASIBLE consecutive rhdl's (of directors / cast members in single movie)
if ((i=recget(db,rh_list,rh,65))>64) throw(83);
if (i&3) throw(86);
ct=i/4;
for (i=0;i<ct;i++)
    {
    char w[61];
    if (recget(db,rh[i],w,61)>60) throw(84);         // Read this actual string name into 'w'
    RHNM rn={0,w};
    int j=rhnm->in(&rn);    // Get the SUBSCRIPT of this name (which MUST exist) in temp table 'rhnm'
    if (j==NOTFND) throw(80);
    ushort uj=(ushort)j;
    d->put(&uj);
    added++;
    }
return(added);
}

static void flset_dttm(const char *dst, const char *src) // set dttm of 'dst' filename to match that of 'src'
{
FILEINFO fi;
if (!drinfo(src, &fi)) throw(99);
utimbuf ut = {0, fi.dttm};              
utime(dst,&ut);
}

HDL OMDB::bck_open(const char *mode)
{
HDL f=flopen(bk_fn,mode);
if (f==NULL) {Sjhlog("Can't open %s for '%s' access",bk_fn,mode); throw(101);}
if (*mode=='r') sjhlog("Restoring database from %s",bk_fn);
return(f);
}

void OMDB::backup(void)
{
HDL f=bck_open("w");
if (rhnm==NULL) create_rhnm();
int i, again=0;
ushort ct=rhnm->ct;
flput(&ct,sizeof(ushort),f);
for (i=0;i<ct;i++)
    {char *n=((RHNM*)rhnm->get(i))->nm; flput(n,strlen(n)+1,f);}
RHDL omrh, notes_rh;
ct=btrnkeys(om_btr);
flput(&ct,sizeof(ushort),f);
int32_t imdb_num;
while (bkyscn_all(om_btr,&omrh,&imdb_num,&again))
    {       // change director, cast contents to "No. of subscripts into RHNM (after OM structure)"
    OM_KEY om;
    if (zrecget(db,omrh,&om,sizeof(OM_KEY))!=sizeof(OM_KEY)) throw(88);
    if (om.imdb_num!=imdb_num) throw(86);
    DYNAG t(sizeof(short));     // table from which to actually WRITE director,cast subscripts
    om.director=namsub(om.director, &t);
    om.cast=namsub(om.cast, &t);
    if (om.director+om.cast!=t.ct) {Sjhlog("Impossible!"); throw(80);}
    char w[61];
    int sz=recget(db,om.nam,w,61);  // (sz includes eos nullbyte of MovieName)
    if (sz>60) throw(86);
    om.nam=sz;
    if ((notes_rh=om.notes)!=NULLRHDL) om.notes=zrecsizof(db,notes_rh);
    flput(&om,sizeof(OM_KEY),f);
    flput(w,sz,f);  // Write MovieName (incl eos byte)
    flput(t.get(0),sizeof(short)*t.ct,f);   // Write ALL namsubs (director PLUS cast) as one block
    if ((sz=om.notes)!=0)
        {
        char ww[4096];
        if (zrecget(db,notes_rh,ww,sz)!=sz) throw(80);
        flput(ww,sz,f);
        }
    }
flclose(f);
flset_dttm(bk_fn,dbfnam(db));
}

RHDL OMDB::namadd(int ct, HDL f)		// Construct, write, & return "rh_list" rhdl (director or cast)
{
ushort u;
RHDL rh[17];       // max feasible elements in "rh_list" of multiple names
for (int i=0;i<ct;i++)
    {
    flget(&u,sizeof(ushort),f);
    RHNM *rn=(RHNM*)rhnm->get(u);
    rh[i]=rn->rh;
    }
// TODO  -  HERE is where the list of up to 5 rhdl's (to actors) in this movie
//          could be SORTED into "most -> least" common sequence (see Dorothy McGuire / Robert Mitchum)
return(recadd(db,rh,ct*sizeof(RHDL)));
}

int _cdecl cp_rhnm(RHNM *a, RHNM *b) {return(strcmp(a->nm,b->nm));}

void OMDB::restore(void)
{
scrap_rhnm();
HDL f=bck_open("r");
int i, j, ctx;
ushort ct;
flget(&ct,sizeof(ushort),f);  // number of "name" (actor or director) strings coming next in backup file
char w[61];
rhnm=new DYNTBL(sizeof(RHNM),(PFI_v_v)cp_rhnm, ct);
for (i=0;i<ct;i++)      // read the name strings from *.bck, write to *.dbf, and add to temp table rhnm
    {
    if (flgetstr(w,61,f)>60) throw(82);  // flgetstr() return value DOES include eos byte
    RHNM rn;
    rn.nm=stradup(w);
    rn.rh=recadd(db, rn.nm,strlen(w)+1);
    rhnm->put(&rn); // (should double-check that all entries are being added in ASCENDING sequence)
    }
RHDL omrh;
flget(&ct,sizeof(ushort),f);  // number of MOVIE records coming next in backup file
for (i=0;i<ct;i++)
    {       // change director, cast contents to "No. of subscripts into RHNM (after OM structure)"
    OM_KEY om;
    flget(&om,sizeof(OM_KEY),f);
    int sz=(int)om.nam;
    if (sz < 1 || sz > 60) throw(79);
    flget(w,sz,f);
    if (strlen(w)!=sz-1) throw(82);
    om.nam=recadd(db,w,sz);
    om.director=namadd((int)om.director,f);
    om.cast=namadd((int)om.cast,f);
    if ((sz=(int)om.notes)!=NULLRHDL)
        {
        char *ww=(char*)memgive(sz);
        flget(ww,sz,f);
        om.notes=zrecadd(db,ww,sz);
        memtake(ww);
        }
    bkyadd(om_btr,recadd(db,&om,sizeof(OM_KEY)),&om.imdb_num);
    }
flclose(f);
}

#ifdef USE_QSETTINGS
static void get_conf_path(QSettings *qs, char *fn, const char *ext)
{
char xx[16];
strfmt(xx,"%s_path",ext);
strcpy(fn, (const char*)qs->value(xx).toString().toStdString().c_str());
#else
static void get_conf_path(PARM *pm, char *fn, const char *ext)
{
char xx[16];
strfmt(xx,"%s_path",ext);
strcpy(fn,pm->get(xx));
#endif
if (!fn[0]) {sjhlog("No configured [%s] for OMDB.%s",xx,ext); throw(102);}
if (fn[strlen(fn)-1]!='/') strcat(fn,"/");
strendfmt(fn,"OMDB.%s",ext);
}



static void grab(void) {;}

OMDB::OMDB(void)
{
char fn[256];
//grab();
#ifdef USE_QSETTINGS
    {
    QSettings qs;
    get_conf_path(&qs, fn, "dbf");
    get_conf_path(&qs, bk_fn, "bck");
    }
#else
    {
    PARM parm(PARM_FN);
    get_conf_path(&parm, fn, "dbf");
    get_conf_path(&parm, bk_fn, "bck");
    }
#endif
dbactivated=dbstart(32);
if (access(fn, F_OK ))		// mode=F_OK=0, where non-zero return value means file doesn't exist AT ALL
	{						// mode could be either or both (R_OK|W_OK) for "User has Read / Write access"
	Xecho("Creating database %s\r\n",fn);
	if (!dbist(fn) || (db=dbopen(fn))==NULLHDL) goto err;
	memset(&hdr,0,sizeof(hdr));
	hdr.ver=1;
	hdr.om_rhdl=btrist(db,DT_ULONG,sizeof(int32_t));
	dbsetanchor(db,recadd(db,&hdr,sizeof(hdr)));
	dbsafeclose(db);
	}
db_open(fn);
if (btrnkeys(om_btr)==0)
    {
    restore();
    scrap_rhnm();                           // After 'restore' - Close *.dbf, adjust dttm, and re-open
    dbsafeclose(db);                        // Mirrors 'backup' which sets *.bck dttm to match *.dbf
    flset_dttm(fn,bk_fn);
    db_open(fn);
    }
return;
err:
m_finish("Error creating %s",fn);
}

void OMDB::scrap_rhnm()
{
if (rhnm!=NULL)
    {
    for (int i=0;i<rhnm->ct;i++) memtake(((RHNM*)rhnm->get(i))->nm);    // release all alloc'd names
    SCRAP(rhnm);
    }
}

OMDB::~OMDB()
{
scrap_rhnm();
dbsafeclose(db);
if (dbactivated) dbstop();
}

static char *comma_or_eos(char *a)
{
while (*a) if (*a==COMMA) break; else a++;
return(a);
}

// Populate temporary internal table of every name (actors & directors) present in any movie(s)
void OMDB::create_rhnm(void)
{
rhnm=new DYNTBL(sizeof(RHNM),(PFI_v_v)cp_rhnm);
DYNTBL rhall(sizeof(int32_t),(PFI_v_v)cp_ulong);
int32_t imdb_num;
int again=0;
RHDL omrh;
while (bkyscn_all(om_btr,&omrh,&imdb_num,&again))
    {
    OM_KEY om;
    if (zrecget(db,omrh,&om,sizeof(OM_KEY))!=sizeof(OM_KEY)) throw(88);
    if (om.imdb_num!=imdb_num) throw(86);
    add_names_to_rhnm(om.director, &rhall);
    add_names_to_rhnm(om.cast, &rhall);
    }
}

void OMDB::add_names_to_rhnm(RHDL rh_list, DYNTBL *rhall)
{
int i, ct;
RHDL rh[17];        // maximum FEASIBLE consecutive rhdl's (of directors / cast members in single movie)
if ((i=recget(db,rh_list,rh,65))>64) throw(83);
if (i&3) throw(86);
ct=i/4;
for (i=0;i<ct;i++)
    {
    if (rhall->in(&rh[i])!=NOTFND) continue;    // We've already loaded this rhdl + name into temporary internal tables
    char w[61];
    RHNM rn1, *rn;
    RHDL rhdl;
    if (recget(db,rn1.rh=rh[i],w,61)>60) throw(84);         // Read this actual string name into 'w'
    rn1.nm=w;
    if ((rn=(RHNM*)rhnm->find(&rn1))!=NULL) {Sjhlog("This shouldn't happen!"); throw(83);}
    // it's a new name - add rhdl+name to temporary internal tables
    rhall->put(&rh[i]);
    rn1.nm=stradup(w);
    rhnm->put(&rn1);
    }
}

// Return (new or existing) rhdl for an (actor or director) name record
RHDL OMDB::find_or_add_person(char *str)
{
if (rhnm==NULL) create_rhnm();
RHNM rn1, *rn;
rn1.nm=str;
if ((rn=(RHNM*)rhnm->find(&rn1))!=NULL) return(rn->rh);    // This name already recorded
// ELSE it's a new name - write a record and add rhdl+name to temporary internal table
rn1.rh=recadd(db,rn1.nm=stradup(str),strlen(str)+1);
rhnm->put(&rn1);
return(rn1.rh);
}

RHDL OMDB::people2rhdl(char *str)	// RHDL -> LIST of rhdl's (of each comma-separated name in 'str)
{
char *s, *e, w[60];
DYNAG t(sizeof(RHDL));  // The (indirect) LIST of rhdl's created/found for the (1 or more) names in passed string
for (s=str; *s; s=e)    // This function creates that LIST, then writes it and returns the rhdl of the 'list' record
    {
    e=comma_or_eos(s);
    int len=e-s;
    memmove(w,s,len);
    w[len]=0;
//  Now either find the rhdl of existing 'name' record for this 'person', or add a new record
    RHDL rhdl = find_or_add_person(w);
    t.put(&rhdl);   // add rhdl of this name (existing OR newly=added) to the list of rhdl's for item
    while (*e==COMMA || *e==SPACE) e++; // step forward to next name, or end-of-string
    }
return(recadd(db,t.get(0),t.ct*sizeof(RHDL)));  // rhdl of the list itself (of 'name' rhdl's)
}

// Passed rh_list gets a record containing a LIST of up to 5 rhdl's of actual actor records
char* OMDB::rhdl2people(char *str, RHDL rh_list, int mxsz)
{
int i, ct;
RHDL rh[17];        // maximum FEASIBLE consecutive rhdl's (of cast members in single movie)
if ((i=recget(db,rh_list,rh,65))>64) throw(83);
if (i&3) throw(86);
ct=i/4;
for (*str=i=0;i<ct;i++)
    {
    char w[61];
    if (recget(db,rh[i],w,61)>60) throw(84);
    if (strlen(str)+strlen(w)+3 > mxsz) break;  // WOULD FAIL with, say, 29-char director name
    if (*str) strcat(str,", ");
    strcat(str,w);
    }
return(str);
}

void OMDB::om2em(EM_KEY1 *em, OM_KEY *om)
{
memset(em,0,sizeof(EM_KEY1));
em->e.year=om->year;
em->e.emdb_num=om->emdb_num;
em->e.imdb_num=om->imdb_num;
em->added=om->added;
em->seen=om->seen;
em->filesz=om->filesz;
em->runtime=om->runtime;
em->e.rating=om->rating;
char w[61];
if (recget(db,om->nam,w,61)>60) throw(86);
strcpy(em->e.nam,w);
strcpy(em->director, rhdl2people(w,om->director,sizeof(em->director)));
strcpy(em->cast, rhdl2people(w,om->cast,sizeof(em->cast)));
}

// Return full movie list (DYNTBL emk) from omdb.dbf (previously created from emdb.csv)
DYNTBL *OMDB::dyntbl_out(void)
{
int again=0;
DYNTBL *eee = new DYNTBL(sizeof(EM_KEY1), (PFI_v_v)cp_em_key);
EM_KEY1 e;
RHDL rh;
while (bkyscn_all(om_btr,&rh,&e.e.imdb_num,&again))
    {
    OM_KEY om;
    if (zrecget(db,rh,&om,sizeof(OM_KEY))!=sizeof(OM_KEY)) throw(88);
    om2em(&e,&om);
    eee->put(&e);
    }
return(eee);
}

// Moved all this functionality into OMDB.CPP so it can be replaced
// by code to read OMDB.DBF rather than *.csv without "q3" knowing or caring (if 'action'==2)
DYNTBL *make_emk(void)
{
OMDB om;
om.backup_if_needed();
return(om.dyntbl_out());
}

struct XDT {long dttm; char ext;};
// Create a temporary (sorted) DYNTBL of older backup files and copy to passed DYNAG in reverse sequence
// So the actual RETURNED table has MOST RECENT ("highest" dated) files FIRST
static void make_list_of_older_backups(const char *pth, DYNAG *tt)
{
DYNTBL t(sizeof(XDT),(PFI_v_v)cp_ulong);    // OLDEST bckN comes FIRST here
DIRSCAN ds(pth);
FILEINFO fi;
struct dirent *entry;
while ((entry=ds.next(&fi))!=NULLPTR)
	{
	if ((entry->d_type&DT_DIR)!=0 || memcmp(entry->d_name,"OMDB.bck",8)) continue;
    char e=entry->d_name[8];
    if (entry->d_name[9]!=0 || !ISDIGIT(e)) continue;    // Ignore (e=0=eos) CURRENT backup file (only want *.bckN)
    XDT xd = {fi.dttm, entry->d_name[8]};
    t.put(&xd);
	}
for (int i=t.ct;i--;) tt->put(t.get(i));
}

// Delete *.bckN - both the file itself, AND that element in the passed table
static void del_bckN(const char *pth, DYNAG *t, int N, char *Nused)
{
XDT *x=(XDT*)t->get(N);
char p[256];
int ok=drunlink(strfmt(p,"%s/OMDB.bck%c", pth, x->ext));
if (!ok) throw(81);
strdel(strchr(Nused,x->ext),1);     // Remove this digit from 'used' list (so it might get RE-USED)
t->del(N);
}

static void list(DYNAG *t, int rng)
{
XDT *x;
int i;
char s[100];
sjhlog("Rng:%d got %d elements...",rng,t->ct);
for (i=0;(x=(XDT*)t->get(i))!=NULL;i++)
    sjhlog("%d %s %c", i, calfmt(s,"%C-%02O-%02D_%2T-%2I.bck",x->dttm), x->ext);
}

static char *strdt(int32_t dttm)
{static char s[24]; return(calfmt(s,"%3.3M %02D %2T:%2I",dttm));}


// We're about to create a new *.bck for dttm as passed (current dttm of *.dbf, preserved in *.bck)
// Delete any superfluous *.bckN files, rename existing *.bck (which MUST exist) to lowest unused digit
// Allow at most ONE *.bckN within 24 hrs of current (so there will be at most TWO after the rename)
// At most TWO *.bckN > 24hrs && < 1 week, and at most TWO > 1 week && < 1 month
// If there are TWO of either category above, they're the most recent AND the oldest
static char tidy_up_older_backups(DYNAG *t, const char *pth, int32_t dttm)
{
XDT *x;
int i, rng;   
int32_t limit[4]={BIGLONG, dttm-ONE_DAY, dttm-(ONE_DAY*7), 0};
char Nused[11];
for (Nused[0]=i=0; (x=(XDT*)t->get(i))!=NULL; i++) strinsc(Nused, x->ext);
for (rng=0; rng<3; rng++)   // 0=Day, 1=Week, 2=Month
    {
    int32_t hi=limit[rng], lo=limit[rng+1];
    DYNAG t1(sizeof(XDT));
    for (i=0;(x=(XDT*)t->get(i))!=NULL;i++)
        if (x->dttm < hi && x->dttm >= lo)
            t1.put(x);
//list(&t1,rng);
    if (rng==2) // Unilaterally delete any (N) elements / files more than a month old
        while (t1.ct>0 && (x=(XDT*)t->get(t1.ct-1))->dttm < dttm-(ONE_DAY*30))
            {
            sjhlog("Delete backup *.bck%c %s more than 1 month old",x->ext,strdt(x->dttm));
            del_bckN(pth,&t1,t1.ct-1, Nused);
            }
    while (t1.ct > (rng?2:1)) // mx initially 1 for 'day' range 'cos it'll soon include renamed *.bck
        del_bckN(pth,&t1, (rng?1:0), Nused);
    }
for (i=0; i<10; i++) if (strchr(Nused, '0'+i) == NULL) break;   // Find the lowest N no longer in use
return('0'+i);                                    // this is what caller will rename exsting OMDB.bck to
}

// As well as OMDB.bck, we retain up to 6 older backup files (*.bckN, where N is any digit)
// At most two up to 24hrs older than current (most recent), two 1 week older, and two 4 weeks older
void OMDB::rename_backups(int32_t dttm) // Passed param is current *.dbf dttm about to be backed up
{                                       // This fn is only called if there IS an existing *.bck file
int i, j;                               // - which gets renamed as indicated by tidy_up_older_backups()
char pth[256], cmd[512], *ext;
*(ext=strrchr(strcpy(pth,bk_fn),'/'))=0;    // Remove final element (actual filename OMDB.bck)
DYNAG t(sizeof(XDT));                       // MOST RECENT bckN comes FIRST here
make_list_of_older_backups(pth, &t);
strfmt(ext,"/OMDB.bck%c",tidy_up_older_backups(&t, pth, dttm)); // get *.bckN that *.bck is renamed to
strfmt(cmd,"mv \"%s\" \"%s\"",bk_fn,pth);  // Actually do rename of *.bck to *.bckN (after choosing N)
#ifdef USE_QSETTINGS
bool ok = (system(cmd)==0); // ret = zero = no error
#else
int statusflag, id = fork();
if (id == 0)  // then it's the child process
    execlp("/bin/sh","/bin/sh", "-c", cmd, (char *)NULL);
wait(&statusflag);  // wait(&status) until child returns, else it might not be ready to use results
bool ok=(statusflag==0);
if (!ok) {printf("Error moving %s to %s",bk_fn,pth); throw(88);}
#endif
}

void OMDB::backup_if_needed(void)
{
if (dbreadonly(db)) return;
int no_bck;
#ifdef USE_QSETTINGS
    {
    QSettings qs;
    no_bck=*qs.value("autobackup").toString().toStdString().c_str();
    }
#else
    {
    PARM parm(PARM_FN);
    no_bck=*parm.get("autobackup");
    }
#endif
if (TOLOWER(no_bck)=='n') {sjhlog("No AutoBCK"); return;}
int32_t db_dttm, bk_dttm=0;
FILEINFO fi;
if (!drinfo(dbfnam(db), &fi)) throw(99);
db_dttm=fi.dttm;
if (drinfo(bk_fn, &fi)) bk_dttm=fi.dttm;
if (bk_dttm==db_dttm) return;   // Nothing to do if current backup already contains latest data
if (bk_dttm!=0) rename_backups(db_dttm);
backup();
}



void OMDB1::db_open(char *fn)
{
if ((db=dbopen(fn))==NULLHDL) m_finish("Error opening %s",fn);
recget(db,dbgetanchor(db),&hdr,sizeof(hdr));						// Read anchor record
if ( !hdr.ver || (om_btr=btropen(db,hdr.om_rhdl))==NULLHDL)
	m_finish("Error reading %s",fn);
}

OMDB1::OMDB1(void)
{
char fn[256];
#ifdef USE_QSETTINGS
    {
    QSettings qs;
    get_conf_path(&qs, fn, "db1");
    }
#else
    {
    PARM parm(PARM_FN);
    get_conf_path(&parm, fn, "db1");
    }
#endif
dbactivated=dbstart(32);
int prv=dbsetlock(NO);
if (access(fn, F_OK ))		// mode=F_OK=0, where non-zero return value means file doesn't exist AT ALL
	{						// mode could be either or both (R_OK|W_OK) for "User has Read / Write access"
	Xecho("Creating database %s\r\n",fn);
	if (!dbist(fn) || (db=dbopen(fn))==NULLHDL) goto err;
	memset(&hdr,0,sizeof(hdr));
	hdr.ver=1;
	hdr.om_rhdl=btrist(db,DT_ULONG,sizeof(int32_t));
	dbsetanchor(db,recadd(db,&hdr,sizeof(hdr)));
	dbsafeclose(db);
	}
db_open(fn);
dbsetlock(prv);
//sjhlog("omdb1 nkeys=%d",btrnkeys(om_btr));
return;
err:
m_finish("Error creating %s",fn);
}

OMDB1::~OMDB1()
{
dbsafeclose(db);
if (dbactivated) dbstop();
}

bool OMDB1::get_om1(int32_t imdb_num, OM1_KEY *om1)
{
RHDL rh;
if (!bkysrch(om_btr,BK_EQ,&rh,&imdb_num)) return(NO);
if (recget(db,rh,om1,sizeof(OM1_KEY))!=sizeof(OM1_KEY) || om1->imdb_num!=imdb_num) throw(88);
return(YES);
}

int OMDB1::get_rating(int32_t imdb_num)
{
OM1_KEY om1;
if (get_om1(imdb_num,&om1)) return(om1.rating);
return(0);
}

void OMDB1::put_rating(int32_t imdb_num, int rating)
{
OM1_KEY om1;
if (get_om1(imdb_num,&om1))
    {
    RHDL rh;    // have to search AGAIN to get the RH value TODO - add option for get_om1() to do that
    bkysrch(om_btr,BK_EQ,&rh,&imdb_num);    // CAN'T fail, 'cos it just worked on precediing line!
    om1.rating=rating;
    recupd(db,rh,&om1,sizeof(OM1_KEY));
    }
else
    {
    memset(&om1,0,sizeof(OM1_KEY));
    om1.imdb_num=imdb_num;
    om1.rating=rating;
    bkyadd(om_btr,recadd(db,&om1,sizeof(OM1_KEY)),&om1.imdb_num);
    }
}

const char* OMDB1::get_notes(int32_t imdb_num)
{
OM1_KEY om1;
if (!get_om1(imdb_num,&om1) || om1.notes==NULLRHDL) return((const char*)memgive(1));
int sz;
const char *txt=(const char*)zrecmem(db,om1.notes,&sz);
if (sz==0 || txt[sz-1]!=0) throw(81);   // Notes record must be non-zero length, terminated by nullbyte
return(txt);
}

void OMDB1::put_notes(int32_t imdb_num, const char *txt)
{
OM1_KEY om1;
int sz = strlen(txt);
if (!get_om1(imdb_num,&om1)) // There's not YET a user record for this inum
    {
    if (sz)      // so only add a record if notes exist
        {
        memset(&om1,0,sizeof(OM1_KEY));
        om1.imdb_num=imdb_num;
        om1.notes=zrecadd(db,txt,sz+1);
        bkyadd(om_btr,recadd(db,&om1,sizeof(OM1_KEY)),&om1.imdb_num);
        }
    return;
    }
RHDL rh;
bkysrch(om_btr,BK_EQ,&rh,&om1.imdb_num);    // CAN'T fail, 'cos it just worked on precediing call!
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



USRTXT::USRTXT(int32_t num)
{
imdb_num=num;
}

const char* USRTXT::get(void)    // TODO populate rest of 'e' (name,year) for caller to use in window title
{
if (omdb_read_only)
    {
    OMDB1 omdb1;
    return(txt=omdb1.get_notes(imdb_num));
    }
OMDB omdb;
return(txt=omdb.get_notes(imdb_num));
}

void USRTXT::put(const char *t)
{
if (!strcmp(t,txt)) return;                  // ONLY update if txt has changed!
if (omdb_read_only)
    {
    OMDB1 omdb1;
    omdb1.put_notes(imdb_num,t);
    }
else
    {
    OMDB omdb;
    omdb.put_notes(imdb_num,t);
    }
}
