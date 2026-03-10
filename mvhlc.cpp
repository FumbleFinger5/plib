#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/wait.h>
#include "pdef.h"
#include "cal.h"
#include "flopen.h"
#include "memgive.h"
#include "str.h"
#include "drinfo.h"
#include "omdb1.h"
#include "imdb.h"
#include "imdbf.h"
#include "dirscan.h"
#include "log.h"
#include "exec.h"
#include "parm.h"
#include "qblob.h"
#include "mvhlc.h"


void MVHL::db_open(char *fn)
{
if ((db = dbopen2(fn)) == NULLHDL)
   m_finish("Error opening %s", fn);
recget(db, dbgetanchor(db), &hdr, sizeof(hdr)); // Read anchor record
if (hdr.ver!=1) m_finish("Error reading %s", fn);
}

MVHL::MVHL(bool create)
{
char fn[256];
get_conf_path(fn, "MVHL.dbf");
if (access(fn, F_OK))   // mode=F_OK=0, where non-zero return value means file doesn't exist AT ALL
   {                    // mode could be either or both (R_OK|W_OK) for "User has Read / Write access"
   if (create) SJHLOG("Creating database %s\r\n", fn);
   else m_finish("%s not found and -c(reate) flag not specified",fn);
   if (!dbist(fn) || (db = dbopen2(fn)) == NULLHDL)
      m_finish("Error creating %s", fn);
   memset(&hdr, 0, sizeof(hdr));
   hdr.ver = 1;
   dbsetanchor(db, recadd(db, &hdr, sizeof(hdr)));
   dbsafeclose(db);
   }
else if (create) m_finish("%s already exists so -c(reate) invalid",fn);
db_open(fn);
}

// get tbl of NN_FREE recs - 1 per indexed DRVNUM, each with its free_space value
DYNAG* MVHL::NNget(void)
{
DYNAG *tbl=new DYNAG(sizeof(NN_FREE));
if (hdr.nn_rh!=NULLRHDL)                  // If the SINGLE master record exists (at least 1 drv indexed)
   {                                      // ...then it's a list of NN_Key fields by drv, each including
   int sz, ct, i;                         //    the RH of its btr containing ONE IMSZ RECORD PER MOVIE
   NN_KEY* nk=(NN_KEY*)zrecmem(db,hdr.nn_rh,&sz);
   ct=sz/sizeof(NN_KEY);
   for (i=0;i<ct;i++) tbl->put(&nk[i]);      // only puts "number+freesp" into o/p table (not btr rhdl)
   memtake(nk);
   }
return(tbl);
}

// get tbl of IMSZ recs (one for each MOVIE subfolder in this NN, each including 'MovieFolderSize)
DYNAG* MVHL::get(uchar number)   // returned DYNAG (not DYNTBL) is ALWAYS in ascending imno sequence
{                                // (because it's populated by reading 'btr', which is keyed by imno)
int sz, ct, i;
RHDL rh=0;
NN_KEY* nk=(NN_KEY*)zrecmem(db,hdr.nn_rh,&sz);
ct=sz/sizeof(NN_KEY);
for (i=0;i<ct;i++) if (nk[i].nf.number==number) {rh=nk[i].rh; break;}
if (rh==0) m_finish("numberNN:%d not found",number);
memtake(nk);
//HDL btr=btropen(db,nk[i].rh);
HDL btr=btropen(db,rh);
int again=NO;
DYNAG *tbl=new DYNAG(sizeof(IMSZ));
IMSZ imsz;
while (bkyscn_all(btr,0,&imsz,&again)) tbl->put(&imsz);
return(tbl);
}

/*static void testnn(HDL db, RHDL rh)
{
int i,sz;
NN_KEY *tmp=(NN_KEY*)zrecmem(db,rh,&sz);
int ct=sz/sizeof(NN_KEY);
for (i=0;i<ct;i++)
   {
   NN_KEY *nk=(NN_KEY*)&tmp[i];
   HDL btr=btropen(db,nk->rh);
   int movie_ct=btrnkeys(btr);
   btrclose(btr);
   printf("NN:%d free:%s RH:%d CT:%d\n",nk->nf.number,str_size64(nk->nf.free_space),nk->rh, movie_ct);
   }
memtake(tmp);
}*/

static bool same_table(HDL btr, DYNAG *tbl)
{
if (btrnkeys(btr)!=tbl->ct) return(false);      // CAN'T be the same if different counts!
IMSZ imsz, *prv;
for (int i=0;i<tbl->ct;i++)
   {
   IMSZ *prv=(IMSZ*)tbl->get(i);
   IMSZ imsz={prv->imno};
   if (!bkysrch(btr,BK_EQ,0,&imsz) || imsz.sz!=prv->sz) return(false);
   }
return(true);
}

bool MVHL::NNput(uchar number, int64_t free_space, DYNAG *tbl) // tbl = IMSZ rec for each movie in folder
{
int i, sz, ct;
NN_KEY nk={number};
DYNAG NNtbl(sizeof(NN_KEY));
if (hdr.nn_rh==NULLRHDL) hdr.nn_rh=zrecadd(db,&nk,sizeof(NN_KEY));
else zrec2dynag(&NNtbl, db, hdr.nn_rh);
for (i=0;i<NNtbl.ct;i++)
   if (((NN_KEY*)NNtbl.get(i))->nf.number==nk.nf.number)
      {memmove(&nk,NNtbl.get(i),sizeof(NN_KEY)); break;}
if (i==NNtbl.ct)   // drive hasn't been indexed yet
   {
   nk.rh=btrist(db,DT_ULONG,sizeof(IMSZ));
   NNtbl.put(&nk);
   }
NN_KEY *pnk=(NN_KEY*)NNtbl.get(i);
bool upd=(pnk->nf.free_space!=free_space);
pnk->nf.free_space=free_space;
//SJHLOG("upd free_space:%s",str_size64(pnk->nf.free_space));
HDL btr=btropen(db,pnk->rh);
if (!same_table(btr,tbl))
   {
   bkydel_all(btr,YES);       // Remove all existing movie recs indexed for this FilmsNN
   for (i=0;i<tbl->ct;i++)    // Add current movies that caller just scanned into passed 'tbl' of IMSZ's
      bkyadd(btr,0,tbl->get(i));
   upd=true;
   }
btrclose(btr);
if (upd)
   {
   RHDL zrh=zrecupd(db,hdr.nn_rh,NNtbl.get(0),NNtbl.ct*sizeof(NN_KEY));    // Update hdr (of ALL FilmsNN)
   if (zrh!=hdr.nn_rh) m_finish("MVHL.dbf hdrRH unexpectedly changed!");   // should never change rec->zrec
   recupd(db, dbgetanchor(db), &hdr, sizeof(hdr));
   }
// printf("Films%02d (%s free) indexed %d movies\n",nk.nf.number, str_size64(free_space), btrnkeys(btr));
//testnn(db,hdr.nn_rh);
return(upd);
}
