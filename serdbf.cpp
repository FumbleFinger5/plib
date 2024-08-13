#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <utime.h>

#include "pdef.h"
#include "cal.h"
#include "str.h"
#include "memgive.h"
#include "parm.h"
#include "smdb.h"
#include "omdb1.h"
#include "flopen.h"
#include "drinfo.h"
#include "dirscan.h"
#include "log.h"
#include "imdb.h"
#include "exec.h"

#include "serdbf.h"
#include "qblob.h"

//#include <libgen.h>

static int64_t biggest_filesize(const char *pth)
{
DIRSCAN ds(pth);
FILEINFO fi;
struct dirent *entry;
int64_t biggest=0;
while ((entry=ds.next(&fi))!=NULLPTR)	// fi.name=FULLPATHname, entry.name is JUST BASEFILENAME
	if ((entry->d_type&DT_DIR)==0 && fi.size>biggest) biggest=fi.size;
return(biggest);
}

static uchar sz2u(int64_t sz)	// encode ANY filesize into a single byte
{
int e=0;
sz>>=16;				// start by reducing 'sz' to Kb
if (!sz) sz=1;
for (e=0; sz>15; e++) sz>>=2;
return((e<<4) | sz);
}

static int64_t u2sz(uchar u)	// get back to approximately the original full filesize from encoded byte
{
int n=u&15, e=(u>>4);
int64_t sz=65535;
while (e--) sz<<=2;
return(sz*n);
}

static int get_lox_id(const char *p)
{
int lid=a2i(p,2);
if (a2err==0 && lid>=1 && lid<=99) return(lid);
sjhlog("bad lox [%s]",p);
return(NOTFND);
}

LOCN_INST::LOCN_INST(const char *lox)	// Constructor take list of all current locations in one tab-separated string
{													// first 2 chars in each elememt is 2-digit code (followed by text "folder")
char *p;
tbl=new DYNAG(0);
for (int i=0; (p=vb_field(lox,i))!=NULL; i++)
	{
	if (get_lox_id(p)==NOTFND) m_finish("bad lox [%s] in %s",p,lox);
	tbl->put(p);
	}
upd=false;
}   // tab-separated multistring of locations

LOCN_INST::~LOCN_INST() {delete tbl;}   // tab-separated multistring of locations

char* LOCN_INST::get(uchar n)
{
for (int i=0;i<tbl->ct;i++)
	{
	char *p=(char*)tbl->get(i);
	int lid=get_lox_id(p);
	if (lid==n) return(&p[2]);
	}
m_finish("no lox %d",n);
return(0);
}

uchar LOCN_INST::get(char *s)		// return ID code 1-99 of this 'pathpart' element (add to tbl if n/f)
{
int i, lid;
char *p;
BitMap bm(99);				// values 1-99 - we don't use 0 (so we map keys as 'n-1', not 'n')
for (i=0;i<tbl->ct;i++)
	{
	p=(char*)tbl->get(i);
	lid=get_lox_id(p);
	bm.set(lid-1);
	if (!strcmp(s,&p[2])) return(lid);		// return 01-99 value (first 2 bytes before actual pathpart)
	}
lid=bm.first(NO)+1;	// find lowest currently-unused number. Add 1 'cos we want 1-99, not 0-98
strfmt(p=(char*)memgive(strlen(s)+3),"%02d%s",lid,s);			// +1 for eos, +2 more for 'key' prefix
tbl->put(p);
memtake(p);
upd=true;
return(lid);
}

char* LOCN_INST::lox(void)			// Returns ALLOCATED string! Caller responsible for releasing the memory
{
int i,totlen;
char *str, *p;
for (i=totlen=0;i<tbl->ct;i++)
	totlen+=strlen((char*)tbl->get(i));
str=(char*)memgive(totlen + tbl->ct + 1);		// 1 more for a TAB after each item, + 1 for EOS
for (i=0;i<tbl->ct;i++)
	{
	p=(char*)tbl->get(i);
	strendfmt(str,"%s%c",p,TAB);
	}
return(str);
}


int _cdecl cp_series(SERIES *a, SERIES *b)
{
int cmp=cp_long_v(a->series_imno,b->series_imno);					// Major key = imdb_no
if (!cmp) cmp=a->season - b->season;
if (!cmp) cmp=a->episode - b->episode;
return(cmp);
}

void SERDBF::db_open(char *fn)
{
if ((db=dbopen2(fn))==NULLHDL) m_finish("Error opening %s",fn);
recget(db,dbgetanchor(db),&hdr,sizeof(hdr));						// Read anchor record
if (!hdr.ver || (ser_btr=btropen(db,hdr.ser_rhdl))==NULLHDL) m_finish("Error reading %s",fn);
btr_set_cmppart(ser_btr, (PFI_v_v)cp_series);					// (db internal)
}

SERDBF::SERDBF()
{
char fn[256];
get_conf_path(fn,"series.dbf");   // TODO - filenames!
if (access(fn, F_OK ))		// mode=F_OK=0, where non-zero return value means file doesn't exist AT ALL
	{						// mode could be either or both (R_OK|W_OK) for "User has Read / Write access"
	Xecho("Creating database %s\r\n",fn);
	if (!dbist(fn) || (db=dbopen2(fn))==NULLHDL) goto err;
	memset(&hdr,0,sizeof(hdr));
	hdr.ver=1;
	hdr.ser_rhdl=btrist(db,DT_USR,sizeof(SERIES));
	dbsetanchor(db,recadd(db,&hdr,sizeof(hdr)));
	dbsafeclose(db);
	}
db_open(fn);
return;
err:
m_finish("Error creating %s",fn);
}

SERDBF::~SERDBF()
{
dbsafeclose(db);
}

void SERDBF::put(SERIES *ser)
{
RHDL rh;
SERIES chk;
memmove(&chk,ser,sizeof(SERIES));
//if (chk.season>0 || chk.episode>0) m_finish("bums");
if (bkysrch(ser_btr,BK_EQ,&rh,&chk))
    {
    rh=zrecupd(db,rh, ser->uni.blob,strlen(ser->uni.blob)+1);
    bkyupd(ser_btr,rh,ser);		// upd just in case rh value changed (rec / xrec, because of size change)
	 return;
    }
chk.uni.blob=0;		// zeroise rhdl to 'dup locns' AND 'locn' the single one
rh=zrecadd(db,ser->uni.blob,strlen(ser->uni.blob)+1);
bkyadd(ser_btr,rh,&chk);
}

static char *dbrh2str(HDL db, RHDL rh)
{
if (rh==0) return(char*)memgive(1);
return((char*)zrecmem(db,rh,NULL));
}

static void add_if_different(DYNAG *lx4, uchar *locn)
{
int i;
for (i=0; i<lx4->ct; i++)
	{
	char *p=(char*)lx4->get(i);
	if (SAME3BYTES(p,locn)) return;
	}
lx4->put(locn);
}

static bool fill_path_parts(PATHDYNAG *path_parts, LOCN_INST *li, uchar *locn)
{
int i=path_parts->is_mount();		// param 'path' was already checked to be mounted by main()
while (i--) path_parts->del(0);	// Delete "mnt" or "media/<UserName>" from path elements leaving <device> at head
for (i=0;i<2;i++) path_parts->del(path_parts->ct-1);		// Delete 2 rightmost elements (passed folder + child)
for (i=0; i < path_parts->ct; i++)
	locn[i]=li->get((char*)path_parts->get(i));	// 'li' ADDS this path element if not already known, setting upd=true
return(li->upd);
}

// pth points to "episode" level folder (contains the actual video file of one episode)
void SERDBF::put(SERIES *ser, const char *pth)
{
char *lox=dbrh2str(db,hdr.locn_rhdl);
LOCN_INST li(lox);
memtake(lox);
uchar locn[4]={0,0,0,(uchar)sz2u(biggest_filesize(pth))};
int i, sz;
PATHDYNAG path_parts(pth);
if (fill_path_parts(&path_parts,&li,locn))		// returns TRUE if any more path elements were just added to li
	{
	lox=li.lox();
	int sz=strlen(lox)+1;		// +1 for eos
	hdr.locn_rhdl=zrecadd_or_upd(db,hdr.locn_rhdl,lox,sz,0);
	recupd(db,dbgetanchor(db),&hdr,sizeof(hdr));						// update anchor record
	memtake(lox);
	}

RHDL rh;
SERIES chk;
memmove(&chk,ser,sizeof(SERIES));
if (!bkysrch(ser_btr,BK_EQ,&rh,&chk))
	put(&chk);			// here, chk.uni.blob (copied from 'ser') is a valid 8-byte pointer to json doc
else chk.rating=ser->rating;	// (because above search will have overwritten 'chk' with EXISTING rating)
//if (!bkysrch(ser_btr,BK_EQ,&rh,&chk))	// re-load the record to get the 8-byte rhdl + locn
//	m_finish("impossible!");
// Remove any existing locn element with first 3 chars matching CURRENT locn[4] as just created here
// then rewrite 'chk' record with current locn[] actually in blob.uni.locn,
// IF there are MORE locn[] values, store ALL locn's (incl CURR) in blob.uni.rhlocn, else ensure rhlocn is NULL

DYNAG lx4(4);			// each item a compressed "device + 2 path elements (maybe only 1) + sizecode" record
lx4.put(locn);			// Inititialise table of locations with CURRENT location at the front
if (*(int32_t*)chk.uni.locn!=0)		// If this rec was just added above, locn will be 4 null bytes
//int32_t qq=*(int32_t*)chk.uni.locn;
//if (qq!=0)				// If this record was just added a few lines ago, locn will be 0
	add_if_different(&lx4,chk.uni.locn);
if (chk.uni.rhlocn)
	{
	lox=(char*)memgive(sz=zrecsizof(db,chk.uni.rhlocn));
	if (sz>40 || (sz&3)!=0) m_finish("bad lox sz");				// can't possibly have > 10 copies of this episode!
	for (i=0; i<sz; i+=4) add_if_different(&lx4,(uchar*)&lox[i]);
	memtake(lox);
	}
MOVE4BYTES(chk.uni.locn,lx4.get(0));
if (lx4.ct>1)
	chk.uni.rhlocn=zrecadd_or_upd(db,chk.uni.rhlocn,lx4.get(0),lx4.ct*4, 0);
else
	{
	recdel(db,chk.uni.rhlocn);
	chk.uni.rhlocn=0;
	}
// could do final check here - have any values within key (excluding blob) actually changed
bkyupd(ser_btr,rh,&chk);
}

void SERDBF::del(SERIES *ser)	// FIX! - this leaves any "multiple locations" records orphanned!
{
//RHDL rh;
SERIES chk;
memmove(&chk,ser,sizeof(SERIES));
//if (!bkysrch(ser_btr,BK_EQ,&rh,&chk)) m_finish("rec NOT already exists!");
bool ok=bkydel_rec(ser_btr,&chk);
sjhlog("deleting %s seriesI:%d",ok?"":"non-existent",ser->series_imno);
if (ser->season==0 && ser->episode==0)	// then it's the master record for series, so delete all episodes as well
	{
	while (bkysrch(ser_btr,BK_GE,0,&chk))
		{
		sjhlog("deleting Season:%d Episode:%d",chk.season,chk.episode);
		bkydel_rec(ser_btr,&chk);
		}
	}
}

// if !blob_wanted, the union will be returned containing optional rhdl and locn
// If no locns, all nulls.
// ELSE 'locn' contains FIRST (most recently updated) 4-byte code value
//       rhdl is null OR variable-length list of any locn's in addition to FIRST one
bool SERDBF::get(SERIES *ser, bool blob_wanted)
{
SERIES tmp;
RHDL rh;
if (!bkysrch(ser_btr,BK_EQ,&rh,memmove(&tmp,ser,sizeof(SERIES)))) return(false);
memmove(ser,&tmp,sizeof(SERIES));
if (blob_wanted)
	{
	int sz;
	ser->uni.blob=(char*)zrecmem(db,rh,&sz);
	if (sz!=strlen(ser->uni.blob)+1) m_finish("bad recsz");
	}
return(true);
}

//hkmsrda7  s/b 255, not 99
bool SERDBF::get_next(SERIES *ser, int *again)
{
RHDL rh;
if (*again==NO) memset(ser,0,sizeof(SERIES));		// Start at beginning of file on first call
else ser->season=ser->episode=99;						// Subsequent calls seek lowest record for NEXT ImdbSeriesNo
*again=YES;		// (so we'll know it's not the first call next time)
if (!bkysrch(ser_btr,BK_GT,&rh,ser)) return(false);
if (ser->season!=0 || ser->episode!=0) m_finish("I:%d - no series-level record!",ser->series_imno);
return(get(ser,true));	// return next series-level record - with blob, for caller to make list of known SeriesNames
}

void SERDBF::list(int flag)	// 1=PrtBLOB(all)  2=PrtLOCN  4=PrtNAME(only)
{
SERIES tmp;
RHDL rh;
int sz, again=NO;
char *lox=dbrh2str(db,hdr.locn_rhdl);
printf("LOX:[%s]\n",lox);
LOCN_INST	lll(lox);
memtake(lox);
while (bkyscn_all(ser_btr,&rh,&tmp,&again))
	{
	printf("\nI:%d S%d/E%d r:%d\n",tmp.series_imno,tmp.season,tmp.episode,tmp.rating);
	if (flag&1)
		{
		char prv[8]; MOVE8BYTES(prv,&tmp.uni.blob);
		tmp.uni.blob=(char*)zrecmem(db,rh,&sz);
		if (sz!=strlen(tmp.uni.blob)+1) m_finish("bad REcsz");
		printf("[%s]\n",tmp.uni.blob);
		Scrap(tmp.uni.blob);
		MOVE8BYTES(&tmp.uni.blob, prv);
		}
	if ((flag&2)!=0 && *(int32_t*)tmp.uni.locn!=0)
		{
		int i, j;
		uchar kl;
		char wrk[256], *l4;
		DYNAG t(4);
		t.put(tmp.uni.locn);	// FIRST 4-byte LocationCode (if present) is stored here
		if (tmp.uni.rhlocn)	// if rh!=0, read more location codes from zrec
			{
			l4=(char*)zrecmem(db,tmp.uni.rhlocn,&sz);
			for (i=0;i<sz;i+=4) t.put(&l4[i*4]);
			memtake(l4);
			}
		printf("%d locations recorded\n",t.ct);
		for (i=0;i<t.ct;i++)
			{
			l4=(char*)t.get(i);
			for (j=*wrk=0;j<3 && (kl=l4[j])!=0; j++)
				strendfmt(wrk,"%s%s",*wrk?"/":"", lll.get(kl));	
			printf("%s  SizeCode:%d=%s\n",wrk,l4[3], str_filesize(u2sz(l4[3])));
			}
		}
	if (flag&4)
		{
		char *blob;
		JBLOB_READER jb(blob=(char*)zrecmem(db,rh,&sz));
		printf("name=%s\n",jb.get("name"));
		memtake(blob);
		}
	}
}

void SERDBF::update_rating(SERIES *ser)
{
RHDL rh;
SERIES chk;
if (!bkysrch(ser_btr,BK_EQ,&rh,memmove(&chk,ser,sizeof(SERIES)))) m_finish("can't read existing rating");
if (chk.rating==ser->rating) return;	// nothing to do
chk.rating=ser->rating;
bkyupd(ser_btr,rh,&chk);
}

