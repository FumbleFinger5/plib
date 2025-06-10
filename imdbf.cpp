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
#include "imdbf.h"
#include <json-c/json.h>
#include "qblob.h"


const char *gEnre[26] = {"Action","Adult","Adventure","Animation","Biography","Comedy","Crime","Drama",
"Documentary","Family","Fantasy","Film-Noir","History","Horror","Mystery","Musical","Music","Mystery",
"Romance","Sci-Fi","Short","Sport","Thriller","TV Movie","War","Western"};

FLDX fld[NUMCOLS] =     // definitive field list in q3 "base" column number sequence
    {
    {"recent","",'C',0},
    {"title","Title",'L',FID_TITLE},
    {"year","Year",'L',FID_YEAR},
    {"rating","Rating",'C',0},
    {"emdb","Emdb",'L',0},
    {"imdb","IMDb",'L',FID_IMDB_NUM},
    {"director","Director",'L',FID_DIRECTOR},
    {"added","Added",'R',0},
    {"seen","Seen",'R',0},
    {"gb","Gb",'C',0},
    {"cast","Actors",'L',FID_CAST},
    {"runtime","Runtime",'C',FID_RUNTIME},
    {"notes","Notes",'L',0},
    {"notes1","MyNotes",'L',0},
    {"rating1","MyRating",'C',0},
    {"genre","Genre",'C',FID_GENRE},
    {"seen1","MySeen",'C',0}
    };

char idx_fid[]={FID_IMDB_NUM, FID_GENRE, FID_CAST, FID_DIRECTOR, FID_TITLE, FID_YEAR, FID_RUNTIME, 0};


const char *get_fld_name(char fld_id)
{
int i=0;
while (fld[i].fid!=fld_id) if (++i==NUMCOLS) m_finish("unknown field!");
return(fld[i].name);
}

// ip is a list of comma-separated genres for the current movie
// convert each 'known' genre in gEnre[] to the corresponding SINGLE CHAR, and warn if any are unknown
static char *genre_compress(const char *ip, char *op)
{
int i, p;
char s[128];
strcpy(s,ip);
if ((p=stridxs("Science Fiction",s))!=NOTFND)   // Change tmdb  "Science Fiction"  to  "Sci-Fi" 
    memmove(strdel(&s[p],9),"Sci-Fi",6);
while ((p=stridxs(" & ",s))!=NOTFND)   // Change (very rare) tmdb  " & "  between 2 genres to a comma
    *strdel(&s[p],2)=COMMA;
for (i=*op=0;i<26;i++)
	if ((p=stridxs(gEnre[i],s))!=NOTFND) {strdel(&s[p],strlen(gEnre[i])); strendfmt(op,"%c",'A'+i);}
strip(s,COMMA);
strtrim(s);
if (*s)
    sjhlog("Unexpected Genre:[%s] leftover from %s",s,ip);
return(op);
}

// Reverse of Genre_compress() converts string of single-char Genres to (comma-separated) full text format
char *genre_uncompress(const char *ip, char *op)    // used by SCAN_ALL as well as IMDB_FLD
{
for (int i=*op=0;ip[i];i++)
	strendfmt(op,"%s%s",*op?",":"", gEnre[ip[i]-'A']);
return(op);
}


void IMDB_FLD::getkey(char fld_id)  // activate this field-id (sets/reads kY and rH)
{
*((int64_t*)&kY) = fld_id;
if (!bkysrch(fld_btr,BK_EQ,&rH,&kY)) m_finish("fld_id:%c (%d) - no record",fld_id,fld_id);
}

void* IMDB_FLD::load_rH(int elem_sz, int adj)
{
int bytes;
void *v=zrecmem(db,rH,&bytes);
if ((bytes/elem_sz)!=(imx->ct + adj)) m_finish("Invalid record size");
return(v);
}

void IMDB_FLD::load_imx(void)  // MovieNumber Master list (IMdbNo's) stored in the order first indexed
{
getkey(FID_IMDB_NUM);
int i, ct, sz;
int32_t *ii=(int32_t*)zrecmem(db,rH,&sz);
ct=sz/sizeof(int32_t);
imx=new DYNTBL(sizeof(IMN2XLT),cp_long,ct);
for (i=0;i<ct;i++)
    {
    IMN2XLT ix={ii[i],(short)i};
    imx->put(&ix);
    }
memtake(ii);
}

void IMDB_FLD::load_fctl(FCTL *f)
{
getkey(f->id);
if (f->id==FID_IMDB_NUM) f->ph=memadup(imx->get(0),imx->total_size());  // The master ImdbNo table

if (f->id==FID_GENRE || f->id==FID_TITLE || f->id==FID_YEAR || f->id==FID_RUNTIME)
    f->ph = zrecmem(db,rH,NULL);    // each entry = fixed-len (actual values, or ptrs -> allv 'pa')
if (f->id==FID_CAST || f->id==FID_DIRECTOR)
    f->ph = new DYNAG(db,rH);       // each entry = variable-length set of 2-byte Z2i ptrs into allv

if (f->id==FID_GENRE || f->id==FID_TITLE || f->id==FID_CAST || f->id==FID_DIRECTOR)
    f->pa = new DYNAG(db,kY.rhav);
if (f->ph==NULL && f->pa==NULL) m_finish("Unknown Fld-ID:%c (Ascii:%d)",f->id,f->id);
}

IMDB_FLD::IMDB_FLD(void)    // imdb.dbf is (rebuildable) optimised access for q3
{
char fn[256];
int init=NO;
if (access(get_conf_path(fn, "imdb.fld"), F_OK ))		// mode=F_OK=0, where non-zero return value means file doesn't exist AT ALL
	{						// mode could be either or both (R_OK|W_OK) for "User has Read / Write access"
	Xecho("Creating database %s\r\n",fn);
	if (!dbist(fn) || (db=dbopen2(fn))==NULLHDL) m_finish("Error creating %s",fn);
	memset(&hdr,0,sizeof(hdr));
	hdr.ver=1;
	hdr.fld_rhdl=btrist(db,DT_USR,sizeof(KYX));  // 1-char key = FLD_ID + 3 'spare' + RHDL2 (to 'allv')
	dbsetanchor(db,recadd(db,&hdr,sizeof(hdr)));
	dbsafeclose(db);
    init=YES;
	}
db_open(fn);
if (init)
    for (int i=0; idx_fid[i]; i++)
        {
        *((int64_t*)&kY) = idx_fid[i];
        bkyadd(fld_btr, 0, &kY);
        }
load_imx();
}

void IMDB_FLD::db_open(char *fn)
{
db=dbopen2(fn);
recget(db,dbgetanchor(db),&hdr,sizeof(hdr));						// Read anchor record
if ( !hdr.ver || (fld_btr=btropen(db,hdr.fld_rhdl))==NULLHDL)
	m_finish("Error reading %s",fn);
btr_set_cmppart(fld_btr, cp_mem1);					// (db internal)
}


void IMDB_FLD::put_imno(int32_t imno)   // Update the master table of movie numbers
{
IMN2XLT ix={imno,(short)imx->ct};   // xlt of the elem about to be added = existing count before incr
if (imx->in(&ix)!=NOTFND)
    {
    sjhlog("Unexpected movie tt%d already in %s",imno,filename());
    m_finish("logic bomb!");
    }
imx->put(&ix);  // add the new movie to imx table in memory (then update actual file below)
getkey(FID_IMDB_NUM);
int32_t *im=(int32_t*)load_rH(sizeof(int32_t), -1); // -1 = already added to memtable, not yet rH
im=(int32_t*)memrealloc(im,imx->total_size());
im[imx->ct-1]=imno;
bool kyupd=false;
rH=zrecadd_or_upd(db, rH, im, imx->ct*sizeof(int32_t), &kyupd);
if (kyupd) bkyupd(fld_btr, rH, &kY);
memtake(im);
}

bool IMDB_FLD::put2a(const char *str)  // Add GENRE value for new movie
{
bool kyupd=false;
DYNAG t(db,kY.rhav);      // NewConstructor   -   Find or add THIS fldvalue (in 'str') to allv
int prv_t_sz=t.total_size();
short *ii=(short*)load_rH(sizeof(short), -1);     // -1 cos already added to imx
ii=(short*)memrealloc(ii,imx->ct*sizeof(short));    // make it bigger to contain one more short

ii[imx->ct -1] = t.in_or_add(str);             // append this new imno's xlt value to ii
rH=zrecadd_or_upd(db, rH, ii, imx->ct*sizeof(short), &kyupd);
memtake(ii);
if (t.total_size()>prv_t_sz)
    kY.rhav=zrecadd_or_upd(db, kY.rhav, t.get(0), t.total_size(),&kyupd);
return(kyupd);
}

DYNAG *make_cast_table(const char *str)
{
DYNAG *t=new DYNAG(0);
while (*str)
    {
//if (t->ct==3) break;
    char wrk[48];
    int i, ln=stridxc(COMMA,str);
    if (ln==NOTFND) ln=strlen(str);
    if (ln>=sizeof(wrk)) m_finish("name [%s] too long",str);
    memmove(wrk,str,ln);
    wrk[ln]=0;
    if ((i=stridxs("(voice)",wrk))!=NOTFND) {sjhlog("%s",wrk); strdel(&wrk[i],7);}
    t->put(strtrim(wrk));
    str+=ln;
    while (*str==COMMA || *str==SPACE) str++;
    }
return(t);
}


bool IMDB_FLD::put2b(const char *str)  // Add CAST value for new movie
{
bool    kyupd=false;
int     i, tc_ct;
char    subs[32];
DYNAG tc(db,kY.rhav);       // NewConstructor - Table of ALL actors, recsiz Feb 2024 Cast:168947, Directors:38690
//sjhlog("Fld:%c Sz:%d",kY.fid,zrecsizof(db,kY.rhav));
tc_ct=tc.ct;
DYNAG tx(db,rH);     // NewConstructor - table of i2Z subscripts into tc (normally 3 i2Z elements per movie).
DYNAG *tt=make_cast_table(str);     // table of (usually 3) actors in THIS movie
for (i=subs[0]=0; i<tt->ct; i++)
    {
    int n=tc.in_or_add(tt->get(i));
    strcat(subs,(const char*)i2Z(n));
    }
delete tt;
tx.put(subs);
rH=zrecadd_or_upd(db,rH, tx.get(0), tx.total_size(), &kyupd); // rewrite xlt values with THIS movie extra one at end

if (tc_ct!=tc.ct)   // We must have added 1 or more actors to the master table
    kY.rhav=zrecadd_or_upd(db,kY.rhav,tc.get(0),tc.total_size(), &kyupd);
return(kyupd);
}

bool IMDB_FLD::put2c(const char *str)  // Add 1-byte rh value
{
bool kyupd=false;
if (kY.fid==FID_RUNTIME)
    {
    short *ii=rH?(short*)load_rH(sizeof(short), -1) : NULL;
    ii=(short*)memrealloc(ii,imx->ct*sizeof(short));    // make it bigger to contain one more short
    int mins=a2i(str,0);
    ii[imx->ct -1] = mins;
    rH=zrecadd_or_upd(db, rH, ii, imx->ct*sizeof(short), &kyupd);
    memtake(ii);
    }
else
    {
    uchar *cc=rH?(uchar*)load_rH(sizeof(uchar), -1) : NULL;   // uchar not short, 'cos < 256 values
    cc=(uchar*)memrealloc(cc,imx->ct*sizeof(uchar));    // make it bigger to contain one more uchar
    int year=a2i(str,0);
    if (year>1920 && year<2120) year-=1900; else year=0;
    cc[imx->ct -1] = year;
    rH=zrecadd_or_upd(db, rH, cc, imx->ct, &kyupd);
    memtake(cc);
    }
return(kyupd);
}

// Passed buffer here is as returned by the api call (all records in imdb.dbf should also be in imdb.Api)
// First add imno to the master table, then add each indexed field
void IMDB_FLD::put(int32_t imno, const char *buf)   // Add a new movie
{
char fid;
bool kyupd;
char str[128];
put_imno(imno);     // Appends THIS movie to end of master list (and adds to memory-based table 'imx')
JSS4 jss4;
json_object *parser = json_tokener_parse(buf);
for (int i=1; (fid=idx_fid[i])!=0; i++)       // start at 1, not 0 (imno already done)
    {
    getkey(fid);    // ensures kY matches THIS fid, so each different put??() call know what it's doing
    const char *fld_name=get_fld_name(idx_fid[i]);
    const char *ptr=jss4.get(parser, fld_name);   // return ptr-> (alloc'd) copy of named metric string
    switch (fid)
        {
        case FID_RUNTIME:
        case FID_YEAR:
            kyupd=put2c(ptr);
            break;
        case FID_GENRE:
        case FID_TITLE:
            if (fid==FID_GENRE) ptr=genre_compress(ptr,str);
            kyupd=put2a(ptr);
            break;
        case FID_CAST:
        case FID_DIRECTOR:
            kyupd=put2b(ptr);
            break;
        default:
            m_finish("wqtredyt");
            break;
        }
    if (kyupd && !bkyupd(fld_btr,rH,&kY)) m_finish("bkyupd failed!");
    }
json_object_put(parser); // Clean up
}

bool IMDB_FLD::del2a(char fld_id, int xlt)
{
bool kyupd=false;
int i, j;
short *ii=(short*)load_rH(sizeof(short), +1);    // These could be uchar not short if < 256 values
short iix=ii[xlt];  // ptr into allv of THIS movie being deleted (maybe the ONLY movie for that 'allv' genre combo)
DYNAG tii(sizeof(short),imx->ct+1);
for (i=0;i<=imx->ct;i++) tii.put(&ii[i]);
memtake(ii);
tii.del(xlt);
if (tii.in(&iix)==NOTFND)    // IF no other movies reference this combination of genres, delete the allv entry
    {
    DYNAG told(db,kY.rhav);
    DYNAG tnew(0);                     // table of all genre combinations, (one) of which is now redundasnt
    ii = (short*)tii.get(0);    // use this table to directly overwrite tii ptrs in situ ready for file update
    for (i=0; i<tii.ct; ii[i++]=j)
        j=tnew.in_or_add(told.get(ii[i]));
    kY.rhav=zrecadd_or_upd(db,kY.rhav,tnew.get(0),tnew.total_size(),&kyupd);
    }
rH=zrecadd_or_upd(db,rH,tii.get(0),tii.ct*sizeof(short), &kyupd);
return(kyupd);
}

static bool people_now_unused(DYNAG *txx, DYNAG *tx)    // Checking Actors OR Directors
{
int i,j;
for (i=0;txx->ct>0 && i<tx->ct;i++)
    for (uchar *p=(uchar*)tx->get(i); txx->ct>0 && *p; p+=2)
        if ((j=txx->in(p))!=NOTFND) txx->del(j);
return(txx->ct>0);
}

static void rebuild_people(DYNAG *tx, DYNAG *told, DYNAG *tnew)
{
int i,j;
for (i=0;i<tx->ct;i++)
    for (uchar *p=(uchar*)tx->get(i); *p; p+=2)
        {
        char *a=(char*)told->get(Z2i(p));
        j=tnew->in_or_add(a);
        MOVE2BYTES(p,i2Z(j));
        }
}

bool IMDB_FLD::del2b(char fld_id, int xlt)  // remove CAST value for now-deleted movie at imx[xlt]
{
bool kyupd=false;
int i;
DYNAG tx(db,rH);

DYNAG txx(2);   // table (xlt's into allv) of (usually just 3 actors / 1 director) referenced by now-deleted movie
for (uchar *p=(uchar*)tx.get(xlt); *p; p+=2) txx.put(p);

tx.del(xlt);
rH=zrecadd_or_upd(db,rH,tx.get(0),tx.total_size(), &kyupd);
if (people_now_unused(&txx,&tx))
    {
    DYNAG told(db,kY.rhav);
    DYNAG tnew(0);                     // table of all actors, some of which may now be redundasnt
    rebuild_people(&tx,&told,&tnew);
    kY.rhav=zrecadd_or_upd(db,kY.rhav,tnew.get(0),tnew.total_size(), &kyupd);
    zrecupd(db,rH,tx.get(0),tx.total_size());   // recsiz won't change (i2Z 2-bytes sequences overwritten in situ)  
    }
return(kyupd);
}

bool IMDB_FLD::del2c(char fld_id, int xlt)
{
bool kyupd=false;
if (kY.fid==FID_YEAR)
    {
    char *aa=(char*)load_rH(sizeof(char), +1);                // These could be uchar not short if < 256 values
    memmove(&aa[xlt],&aa[xlt+1],(imx->ct-xlt)*sizeof(char));
    rH=zrecadd_or_upd(db,rH,aa,imx->ct*sizeof(char), &kyupd);
    memtake(aa);
    }
else    // FID_RUNTIME
    {
    short *ii=(short*)load_rH(sizeof(short), +1);                // These could be uchar not short if < 256 values
    memmove(&ii[xlt],&ii[xlt+1],(imx->ct-xlt)*sizeof(short));
    rH=zrecadd_or_upd(db,rH,ii,imx->ct*sizeof(short), &kyupd);
    memtake(ii);
    }
return(kyupd);
}

bool IMDB_FLD::del(int32_t imno)
{
IMN2XLT ix={imno,0};
int i, n=imx->in(&ix);
if (n==NOTFND) {sjhlog("Error deleting tt%d from %s",imno,filename()); return(false);}
int xlt=((IMN2XLT*)imx->get(n))->xlt;   // remember position in list of known imno's before deletion
imx->del(n);
getkey(FID_IMDB_NUM);
int32_t *im=(int32_t*)load_rH(sizeof(int32_t), +1);   // list of movie numbers in the sequence first encountered
memmove(&im[xlt],&im[xlt+1],(imx->ct-xlt)*sizeof(int32_t));
bool kyupd=false;
rH=zrecadd_or_upd(db, rH, im, sizeof(int32_t) * imx->ct, &kyupd);
if (kyupd) bkyupd(fld_btr, rH, &kY);       // Update 'ky' for the MASTER table of movie numbers
memtake(im);
char fid;
for (i=1; (fid=idx_fid[i])!=0; i++)     // Start from 1 (not 0) 'cos we already updated the master table above 
    {
    getkey(fid=idx_fid[i]); // ?????????????????????? already assigned in loop above!
    switch (fid)
        {
        case FID_YEAR:
        case FID_RUNTIME:
            kyupd=del2c(fid, xlt);
            break;
        case FID_GENRE:
        case FID_TITLE:
            kyupd=del2a(fid, xlt);
            break;
        case FID_CAST:
        case FID_DIRECTOR:
            kyupd=del2b(fid, xlt);
            break;
        default:
            m_finish("uyfuijyjh");
            break;
        }
    if (kyupd)
        bkyupd(fld_btr, rH, &kY);
    }
return(true);
}

IMDB_FLD::~IMDB_FLD()
{
delete imx; dbsafeclose(db);
}

bool IMDB_FLD::upd(DYNAG *avd)
{
bool kyupd=false;
getkey(FID_TITLE);
kY.rhav=zrecadd_or_upd(db,kY.rhav,avd->get(0),avd->total_size(),&kyupd);
if (kyupd)
   bkyupd(fld_btr, rH, &kY);
return(kyupd);
}

bool IMDB_FLD::upd_title(int32_t imno, const char *title)
{
IMN2XLT ix={imno,0};
int n=imx->in(&ix);                             // n=position in "all known imnos" primary list
if (n==NOTFND) m_finish("Error amending tt%d",imno);
int xlt=((IMN2XLT*)imx->get(n))->xlt;           // xlt=position in "all values" indirection list by field-id
getkey(FID_TITLE);                              // set kY and rH
DYNAG *avd = new DYNAG(db,kY.rhav);
short *im=(short*)load_rH(sizeof(short), 0);    // allocates "indirection array", which must be released
int sub=im[xlt];
memtake(im);
avd->del(sub);
avd->put(title,sub);
bool kyupd=false;
kY.rhav=zrecadd_or_upd(db,kY.rhav,avd->get(0),avd->total_size(),&kyupd);
if (kyupd)
   bkyupd(fld_btr, rH, &kY);
return(true);
}
