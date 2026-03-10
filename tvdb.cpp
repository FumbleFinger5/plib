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
#include "omdb1.h"
#include "flopen.h"
#include "drinfo.h"
#include "dirscan.h"
#include "log.h"
#include "imdb.h"
#include "exec.h"
#include "qblob.h"
#include "tvdb.h"

int cp_series(TVD *a, TVD *b)
{
int cmp=cp_long_v(a->series_imno,b->series_imno);					// Major key = imdb_no
if (!cmp) cmp=a->season - b->season;
if (!cmp) cmp=a->episode - b->episode;
return(cmp);
}

void TVDB::possible_anchor_update(RHDL new_locn_rh)
{
if (new_locn_rh==hdr.locn_rhdl) return;
hdr.locn_rhdl=new_locn_rh;
recupd(db,dbgetanchor(db),&hdr,sizeof(hdr));
dbckpt(db);
}

void TVDB::db_open(char *fn)
{
if ((db=dbopen2(fn))==NULLHDL) m_finish("Error opening %s",fn);
recget(db,dbgetanchor(db),&hdr,sizeof(hdr));						// Read Anchor record
if (hdr.ver!=3 || (ser_btr=btropen(db,hdr.ser_rhdl))==NULLHDL) m_finish("Error reading %s",fn);
btr_set_cmppart(ser_btr, (PFI_v_v)cp_series);					// (db internal)
tpth=new DYNAG(db, hdr.locn_rhdl);           // initialise table of paths from the hdr record
}

TVDB::TVDB()
{
char fn[256];
get_conf_path(fn,"series.dbf");   // TODO - filenames!
if (access(fn, F_OK ))		// mode=F_OK=0, where non-zero return value means file doesn't exist AT ALL
	{						// mode could be either or both (R_OK|W_OK) for "User has Read / Write access"
	Xecho("Creating database %s\r\n",fn);
	if (!dbist(fn) || (db=dbopen2(fn))==NULLHDL) goto err;
	memset(&hdr,0,sizeof(hdr));
	hdr.ver=3;
	hdr.ser_rhdl=btrist(db,DT_USR,sizeof(TVD));
   hdr.locn_rhdl=recadd(db,dummy_path0,strlen(dummy_path0)+1);  // (element:0 never used, others are SUBPATH's)
	dbsetanchor(db,recadd(db,&hdr,sizeof(hdr)));       // TODO: convert Locn_rhdl to variable-length record
	dbsafeclose(db);
	}
db_open(fn);
return;
err:
m_finish("Error creating %s",fn);
}

TVDB::~TVDB()
{
delete tpth;
}

void TVDB::put(TVE *ser)
{
TVD chk;
memmove(&chk,&ser->tvd,sizeof(TVD));
if (bkysrch(ser_btr,BK_EQ,&ser->rh,&chk))
   {
   if (chk.rating)
      memmove(&ser->tvd,&chk,sizeof(TVD));      // ????? (grabs "rating" from dbf if previously set)
   return;                                      // ...does nothing if SERIES-level rec (sea=epi=0)
   }
if (ser->jb!=NULL)
   {
   const char *blob=(char*)ser->jb->get(NULL);
   int   sz=strlen(blob)+1;
   ser->rh=zrecadd(db,blob,sz);
   bkyadd(ser_btr,ser->rh,&chk);
   }
else
   m_finish("jb not set in put");
}

// get subscript of THIS fullpath (legs split into passed DYNAG) as present in dbf-stored list of ALL tracked fullpaths
// stored fullpaths DON'T include anything before DRIVENAME
// stored fullpaths TRUNCATED at level of "folder containing a single episode" (which isn't itself stored)
short TVDB::locn_sz_find_or_add(DYNAG *subpath)
{
int i, sz;
char pth[512];
for (*pth=i=0; i<subpath->ct; i++)
   strendfmt(pth,"%s/",(char*)subpath->get(i));
for (i=1;i<tpth->ct; i++)
   if (!strcmp((char*)tpth->get(i),pth)) return(i);
tpth->put(pth);   // If we're still here, i = tpth->ct, which will be the subscript of newly-added element
char *buf=(char*)zrecmem(db,hdr.locn_rhdl,&sz);    // read existing multi-string zrec
int new_sz=sz+strlen(pth)+1;
buf=(char*)memrealloc(buf,new_sz);                 // make room for the extra path
strcpy(&buf[sz],pth);                              // and append it to the multi-string
RHDL rh=zrecupd(db,hdr.locn_rhdl,buf,new_sz);      // Update the header record
possible_anchor_update(rh);         // if changed from rec to xrec, need to update the hdr
return(i);                          // (i is still the number of existing paths BEFORE adding this new one)
}

short TVDB::get_locn_id(const char *path)
{
int i;
for (i=1; i<tpth->ct; i++)
   {
   char *tp=(char*)tpth->get(i);
   if (!strcmp(tp,path)) break;
   }
if (i>=tpth->ct) {sjhlog("Locn:%s not found",path); return(NOTFND);}
return(i);   // return subscript of passed path in the hdr control list of all known locations
}


void TVDB::list_locn_all(void)
{
for (int i=1; i<tpth->ct; i++)
   printf("%s\n",(char*)tpth->get(i));
}

static int64_t episode_folder_size(DYNAG *legs) // (non-recursed) total all filesizes in Episode folder (stored in 'legs')
{
int64_t sz=0;
char pth[PATH_MAX];
for (int i=pth[0]=0; i<legs->ct; i++)
   strendfmt(pth,"/%s",(char*)legs->get(i));
FILEINFO fi;
DIRSCAN ds(pth);
for (sz=0; ds.next(&fi)!=NULL; sz+=fi.size) {;}
return sz;
}

//static void list_legs(const char *txt, DYNAG *legs)
//{sjhlog("Listing legs:%s",txt); for (int i=0; i<legs->ct; i++) sjhlog("%s",(char*)legs->get(i));  sjhlog("\n");}

void TVDB::put(TVE *epi, PATHDYNAG *pd)
{
DYNAG subpath(pd);   // make a copy of the full list of path elements
int del=0;
if (!strcmp((char*)subpath.get(0),"mnt")) del=1;
if (!strcmp((char*)subpath.get(0),"media")) del=2;
if (!del) m_finish("path not mnt or media");
while (del--) subpath.del(0);      // remove first 1 or 2 path legs - should leave the first leg as a DRIVE/DEVICE
subpath.del(subpath.ct-1);         // remove final element (folder containing ONE episode)
int64_t sz=episode_folder_size(pd);       // Sums ALL filesizes within the specific Episode folder we're currently processing
bool upd=false;
TVE chk;                         // If current (*epi) imdbID+sea+epi already on file, this will contain it,
memmove(&chk,epi,sizeof(TVE));   // otherwise, it's just a copy of the passed 'epi'
if (!bkysrch(ser_btr,BK_EQ,&chk.rh,&chk.tvd))
   {
	put(&chk);     // (insist that all 4 Locn_sz entries are NULL here!)    TVDB::put(TVE *tvd)
   upd=true;
   }
else      // Maybe other fields may be changed? Not sure
   if (chk.tvd.rating!=epi->tvd.rating)
      {chk.tvd.rating=epi->tvd.rating; upd=true;}
LOCN_SZ ls;
ls.locn_id=locn_sz_find_or_add(&subpath);
ls.locn_sz=condense_filesize(sz);
int i, got=NOTFND;
for (i=0; i<4 && got==NOTFND && chk.tvd.lsz[i].locn_id!=0; i++)   // look at up to 4 existing "file location" entries
   if (chk.tvd.lsz[i].locn_id==ls.locn_id) got=i;                 // 'got' = is THIS locn already stored for episode?
if (got!=NOTFND && chk.tvd.lsz[got].locn_sz!=ls.locn_sz)          // IF already stored make it hold CURRENT filesize
   {chk.tvd.lsz[got].locn_sz=ls.locn_sz; upd=true;}                        // (set upd=true if filesize has changed)
if (got==NOTFND)                                                  // If NOT already stored, add locn+sz pair to list
   {                                                                       // if already 4 locn's,
   if (i==4) memmove(chk.tvd.lsz,&chk.tvd.lsz[1],(i=3)*sizeof(LOCN_SZ));   // remove first before appending this one
   MOVE4BYTES(&chk.tvd.lsz[i],&ls);                                        // TODO log the fact of removing a path
   upd=true;
   }
if (upd) bkyupd(ser_btr,chk.rh,&chk);
}

void TVDB::del(TVE *ser)	// FIX! - this leaves any "multiple locations" records orphanned!
{
TVE chk;
memmove(&chk,ser,sizeof(TVE));
bool ok=bkydel_rec(ser_btr,&chk.tvd);
sjhlog("deleting %s seriesI:%d",ok?"":"non-existent",ser->tvd.series_imno);
if (ser->tvd.season==0 && ser->tvd.episode==0)	// then it's the master record for series, so delete all episodes as well
	{
	while (bkysrch(ser_btr,BK_GE,0,&chk.tvd) && chk.tvd.series_imno==ser->tvd.series_imno)
		{
		sjhlog("deleting Season:%d Episode:%d",chk.tvd.season,chk.tvd.episode);
		bkydel_rec(ser_btr,&chk.tvd);
		}
	}
}

// if !blob_wanted, the union will be returned containing optional rhdl and locn
// If no locns, all nulls.
// ELSE 'locn' contains FIRST (most recently updated) 4-byte code value
//       rhdl is null OR variable-length list of any locn's in addition to FIRST one
bool TVDB::get(TVE *ser, bool blob_wanted)
{
TVE tmp;
memmove(&tmp,ser,sizeof(TVE));
if (!bkysrch(ser_btr,BK_EQ,&tmp.rh,&tmp.tvd)) return(false);
memmove(ser,&tmp,sizeof(TVD));
SCRAP(ser->jb);
ser->rh=0;
if (blob_wanted)
	{
	int sz;
   char *ptr=(char*)zrecmem(db,ser->rh=tmp.rh,&sz);
	ser->jb=new JBLOB_READER(ptr);
   char *p2=(char*)ser->jb->get(NULL);
   int cmp=strcmp(ptr,p2);
	if (sz!=strlen(p2)+1)
      m_finish("bad recsz");
	}
return(true);
}

//hkmsrda7  s/b 255, not 99     (not convinced - season will never be > 99, even if Tom&Jerry epiosode is)
bool TVDB::get_next(TVE *ser, int *again)
{
if (*again==NO) memset(ser,0,sizeof(TVE));		// Start at beginning of file on first call
else ser->tvd.season=ser->tvd.episode=99;						// Subsequent calls seek lowest record for NEXT ImdbSeriesNo
*again=YES;		// (so we'll know it's not the first call next time)
if (!bkysrch(ser_btr,BK_GT,&ser->rh,&ser->tvd)) return(false);
if (ser->tvd.season!=0 || ser->tvd.episode!=0)
   m_finish("I:%d - no series-level record!",ser->tvd.series_imno);
return(get(ser,true));	// return next series-level record - with blob, for caller to make list of known SeriesNames
}

bool TVDB::list(int flag, const char *path)	// 1=PrtBLOB(all)  2=PrtLOCN  4=PrtNAME(only)
{
TVE tmp;
int ct[2]={}, ct_epi=0, sz, again=NO;
short locn_id=0;
if (path!=NULL) if ((locn_id=get_locn_id(path))==NOTFND) return(false);
while (bkyscn_all(ser_btr,&tmp.rh,&tmp.tvd,&again))
	{
   if (locn_id!=0)
      {
      bool found=false;
      for (int i=0;!found && i<4;i++) found=(tmp.tvd.lsz[i].locn_id==locn_id);
      if (!found) continue; // path passed but this episode doesn't reference it, so skip
      }
   ct[tmp.tvd.season!=0 || tmp.tvd.episode!=0]++;
	printf("\nI:%d S%d/E%d r:%d f:%d\n",
               tmp.tvd.series_imno,tmp.tvd.season,tmp.tvd.episode,tmp.tvd.rating,tmp.tvd.flag);
	if (flag&1)
		{
		char *ptr=(char*)zrecmem(db,tmp.rh,&sz);
		if (sz!=strlen(ptr)+1) m_finish("bad REcsz");
		printf("[%s]\n",ptr);
		memtake(ptr);
		}
	if ((flag&2)!=0)
      {
      for (int i=0; i<4 && tmp.tvd.lsz[i].locn_id!=0; i++)
          printf("%s %s\n", (char*)tpth->get(tmp.tvd.lsz[i].locn_id), str_size64(expand_filesize(tmp.tvd.lsz[i].locn_sz)));
      }
   if (flag&4)
		{
      char *ptr=(char*)zrecmem(db,tmp.rh,&sz);
		JBLOB_READER jb(ptr);
		printf("name=%s\n",jb.get("name"));
		memtake(ptr);
		}
	}
printf("%d Series and %d Episode recs listed\n",ct[0],ct[1]);
return(true);
}

bool TVDB::key_exists(TVD *tvd)
{
TVD chk;
memmove(&chk,tvd,sizeof(TVD));
return(bkysrch(ser_btr,BK_EQ,NULL,&chk)!=0);
}


void TVDB::update_rating(TVE *ser)
{
TVE chk;
memmove(&chk,ser,sizeof(TVE));
if (!bkysrch(ser_btr,BK_EQ,&chk.rh,&chk.tvd)) m_finish("can't read existing rating");
if (ser->tvd.rating!=0 && chk.tvd.rating!=ser->tvd.rating)
   {
   chk.tvd.rating=ser->tvd.rating;
   bkyupd(ser_btr,chk.rh,&chk.tvd);
   }
}

bool TVDB::del_path(const char *path)
{
short locn=get_locn_id(path);
if (locn==NOTFND) return(false);
TVE tmp;
int i;
int   deleted_from_episodes=0;
LOCN_SZ *lsz=tmp.tvd.lsz;  // maximum 4 location-ID's for an episode
tmp.tvd.series_imno=0;     // lower than lowest possible key value, so BK_GT always advances
while (bkysrch(ser_btr, BK_GT, &tmp.rh,&tmp.tvd))
	{
   bool upd=false;
   for (i=0;i<4;i++)
      {
      if (lsz[i].locn_id<locn) continue;
      upd=true;                  // We WILL update this episode rec with revised location list
      if (lsz[i].locn_id==locn)
         {                       // (decrement i, because 'next' locn will already be shifted leftwards)
         for (int j=i--;j<3;j++) MOVE4BYTES(&lsz[j], &lsz[j+1]);  // strip matching locn entry from list
         *(int32_t*)&lsz[3]=0;                                      // and rotate blank entry into end of list
         deleted_from_episodes++;
         }
      else lsz[i].locn_id--;  // locn must be HIGHER, so decrement this ID in the episode record
      }
   if (!upd) continue;
   if (!bkyupd(ser_btr,tmp.rh,&tmp.tvd)) m_finish("internal logic error 345");
   SJHLOG("upd tt%d sea:%d epi:%d newlocn0:%d/%d RH:%d\n",
      tmp.tvd.series_imno,tmp.tvd.season,tmp.tvd.episode,tmp.tvd.lsz[0].locn_id,tmp.tvd.lsz[0].locn_sz,tmp.rh);
   }
tpth->del(locn);
int sz;
char *buf=dynag2multistring(tpth, &sz);
RHDL rh=zrecupd(db,hdr.locn_rhdl,buf,sz);      // Update the header record
memtake(buf);
possible_anchor_update(rh);         // if changed from rec to xrec, need to update the hdr
printf("deleted path %s\nand removed %d episode links to path\n",path,deleted_from_episodes);
return(true);
}

DYNAG *g_all_legs;
// smart comparator strverscmp() correctly sorts embedded numeric strings within key text so Season10 comes AFTER Season9
int cp_epi_legs(TREE_LEG *a, TREE_LEG *b)
{
int i, cmp;
for (i=0;i<4;i++)
   {
   char *astr=(char*)g_all_legs->get(a->legs_i[i]);
   char *bstr=(char*)g_all_legs->get(b->legs_i[i]);
   if ((cmp=strverscmp(astr,bstr))!=0) break;      // Major key = 2-4 path legs starting from "Category"
   }
if (!cmp) cmp=cp_series((TVD*)a,(TVD*)b);
return(cmp);
}



static void split_legs(DYNAG *dp, const char *pth) // put separate legs (subdirs) of passed path into passed DYNAG

{
char dir[256];
int p;
while (*pth)
   {
   if ((p=stridxc(CHR_SLASH,pth))==NOTFND) m_finish("erk");
   strancpy(dir,pth,++p);
   dp->put(dir);
   pth+=p;
   }
}

const char *CATEGORIES="n/a\tCartoons\tDrama\tComedy\tDocumentary\tOther\t";

static void add_categories(DYNAG *legs)
{
char *p;
for (int n=0;(p=vb_field(CATEGORIES,n))!=NULL; n++)
   legs->put(p);
}

static void log_all_legs(DYNAG *all_legs)
{
sjhlog("All_legs.ct=%d",all_legs->ct);
for (int i=0;i<all_legs->ct;i++)
   sjhlog("%s",(char*)all_legs->get(i));
}


DYNTBL* TVDB::xport(void)
{
DYNAG *all_legs = new DYNAG(0);
g_all_legs=all_legs;       // global pointer to table enables cp_epi_legs to sort char* strings, not subscripts
TVE tmp;
tmp.tvd.series_imno=0;     // lower than lowest possible key value, so BK_GT always advances
add_categories(all_legs);
int cat_ct=all_legs->ct;   // If all_legs.in() subscript for a path component is lower than this, it's a Category
DYNTBL *epi_legs = new DYNTBL(sizeof(TREE_LEG),(PFI_v_v)cp_epi_legs);
int i,j;
while (bkysrch(ser_btr, BK_GT, &tmp.rh,&tmp.tvd))
   {
if (tmp.tvd.series_imno==1001)
j=0;
   if (tmp.tvd.season==0 && tmp.tvd.episode==0) continue;
   if (tmp.tvd.lsz[0].locn_id==0) continue;  // If no location data we can't identify Category, so ignore
   TREE_LEG tl;
   tl.series_imno=tmp.tvd.series_imno;
   tl.season=tmp.tvd.season;
   tl.episode=tmp.tvd.episode;
   for (i=0;i<4;i++) tl.legs_i[i]=tl.drives_i[i]=0;
   for (int lc=0; lc<4 && tmp.tvd.lsz[lc].locn_id!=0; lc++) // check up to 4 FullFilePaths each holding copy of episode
      {
      DYNAG dp(0);
      const char *pth=(const char*)tpth->get(tmp.tvd.lsz[lc].locn_id);
      split_legs(&dp,pth);
      tl.drives_i[lc]=all_legs->in_or_add(dp.get(0));
      int cat_i;
      for (cat_i=1; cat_i<dp.ct; cat_i++)
         if ((i=all_legs->in(dp.get(cat_i)))!=NOTFND && i<cat_ct) break;
      if (cat_i>=dp.ct) m_finish("%s - path lacks category",pth);
      for (i=cat_i; --i>=0; dp.del(0)) {;} // remove all legs from pth (incl initial 'drive') before Category
      for (i=0;i<dp.ct;i++)   // dp.ct always <= 4. Save list of up to 4 subscripts and verify it's the same
         {                    // for ALL tracked copies of this imdbNo+Season/Episode key value
         char *leg=(char*)dp.get(i);
         j=all_legs->in_or_add(leg);
         if (tl.legs_i[i]==0)
            tl.legs_i[i]=j;
         if (tl.legs_i[i]!=j)
            m_finish("Incompatible path %s (duplicate custom _tt1nnn imdbID?)",pth);
         }
      }
   if (epi_legs->in(&tl)!=NOTFND) m_finish("unexpected dup");
   epi_legs->put(&tl);
   }
epi_legs->cargo(&all_legs,sizeof(DYNAG*));
return(epi_legs);
}




TREE_BUILDER::TREE_BUILDER(TVDB *_tvdb)
{
tvdb=_tvdb;
epi_legs=tvdb->xport();
all_legs=*(DYNAG**)epi_legs->cargo(NULL);
for (int i=0;i<4;i++) prvi[i].line=prvi[i].i=0;
}

void TREE_BUILDER::listall(void)
{
char pth[256];
for (int i=0;i<epi_legs->ct;i++)
   {
   TREE_LEG *tl=(TREE_LEG*)epi_legs->get(i);
   int j, k;
   for (j=pth[0]=0; j<4 && (k=tl->legs_i[j])!=0; j++) strendfmt(pth,"%s/",(char*)all_legs->get(k));
   TVE tve;
   memset(&tve,0,sizeof(TVE));            // Have to zeroise the record or tvdb->get() erroneously frees tve.jb
   memmove(&tve.tvd,tl,6);
   if (!tvdb->get(&tve,true)) m_finish("bums");
   if (tve.tvd.season!=0) strendfmt(pth,"Season %d/",tve.tvd.season);
   const char *title = tve.jb->get("Title");
   if (title==NULL || *title==0 || stridxc(CHR_SLASH,title)!=NOTFND) m_finish("episode title error!");
   strendfmt(pth,"%d - %s/",tve.tvd.episode, title);
   delete(tve.jb);
   sjhlog("%s",pth);
   }
}

bool TREE_BUILDER::next(char *desc, int *parent_line)
{
int n;
if (tli>=epi_legs->ct) return(false);
TREE_LEG *tl=(TREE_LEG*)epi_legs->get(tli);
lct++;      // Regardless of whether following loop returns an intermediate leg or not, ONE line will be returned
for (n=0; n<=3 && tl->legs_i[n]!=0; n++)
   {
   if (tl->legs_i[n]!=prvi[n].i)
      {
      strfmt(desc,"%s",(char*)all_legs->get(tl->legs_i[n]));
      prvi[n].i=tl->legs_i[n];
      prvi[n].line=lct;
      *parent_line = n ? prvi[n-1].line : 0;
while (++n<=3) prvi[n].i=0;   // if B Series2/Season1 after A Series1/Season1, stop B from linking back to Season1 from A
      return(true);
      }
   }
*parent_line=prvi[n-1].line;     // n identifies first UNUSED tl->legs_i[n] value found in above loop
//for (int i=n;i<=3;i++) prvi[i].line=prvi[i].i=0;
tli++;
TVE tve = {tl->series_imno, tl->season, tl->episode};
tve.jb=NULL;     // Have to zeroise jb or tvdb->get() erroneously frees it
const char *ttl;
if (!tvdb->get(&tve,true) || (ttl = tve.jb->get("Title"))==NULL || *ttl==0 || stridxc(CHR_SLASH,ttl)!=NOTFND)
   m_finish("episode title error!");
strfmt(desc,"%d - %s",tve.tvd.episode, ttl);
delete(tve.jb);      // release the episode-level blob we just used to get episode-level title
return(true);
}
