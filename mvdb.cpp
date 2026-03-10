#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <utime.h>
#include <cstdarg>
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
#include "imdbf.h"
#include "exec.h"
#include "scan.h"
#include "qblob.h"
#include "my_json.h"
#include "tmdbc.h"
#include "mvdb.h"

// CAREFUL! Block compare of both key AND data might report differences because of padding bytes or uninitialized fields!
static bool bky_add_or_upd_if_changed(HDL btr, void *key, void *data, int recsz)
{
RHDL rh;
HDL db=btr_get_db(btr);
int ksz=btr_get_keysiz(btr);
void *tmp_key=memadup(key,ksz);
bool exists=bkysrch(btr,BK_EQ,&rh,tmp_key);				// If the key doesn't already exist, we MUST update the file
bool changed=true;
if (!exists)
	{
   bkyadd(btr,zrecadd(db,data,recsz),key);				// (add new key AND new data record)
	}
else
	{
	int tmp_recsz;									// If the key DOES exist, only actually update if either...
	void *tmp=zrecmem(db,rh,&tmp_recsz);	// ...non-cp part of key changed OR data (size or contents) changed
	changed=(memcmp(key,tmp_key,ksz)!=0 || tmp_recsz!=recsz || memcmp(tmp,data,recsz)!=0);
	if (changed)
	   bkyupd(btr,zrecupd(db,rh,data,recsz),key);        // (rh might switch between rec / xrec if recsz changed)
	memtake(tmp);
	}
memtake(tmp_key);
return(changed);
}


void MVDB::db_open(char *fn)
{
if ((db=dbopen2(fn))==NULLHDL) m_finish("Error opening %s",fn);
recget(db,dbgetanchor(db),&hdr,sizeof(hdr));						// Read anchor record
if (!hdr.ver || (bl_btr=btropen(db,hdr.bl_rh))==NULLHDL) m_finish("Error reading %s",fn);
btr_set_cmppart(bl_btr, (PFI_v_v)cp_mem1);						// (db internal) key=BL_CARGO, data=IMSZ table
}

MVDB::MVDB(void)
{
char fn[256];
get_conf_path(fn,"moovie.dbf");
if (access(fn, F_OK ))		// mode=F_OK=0, where non-zero return value means file doesn't exist AT ALL
	{						// mode could be either or both (R_OK|W_OK) for "User has Read / Write access"
	Xecho("Creating database %s\r\n",fn);
	if (!dbist(fn) || (db=dbopen2(fn))==NULLHDL) goto err;
	memset(&hdr,0,sizeof(hdr));
	hdr.ver=1;
	hdr.bl_rh=btrist(db,DT_USR,sizeof(BL_CARGO));
	dbsetanchor(db,recadd(db,&hdr,sizeof(hdr)));
	dbsafeclose(db);
	}
db_open(fn);
return;
err:
m_finish("Error creating %s",fn);
}

bool MVDB::get(int mode, BL_CARGO *blc, DYNTBL *tbl_imsz)   // Read cargo for an IMSZ table. Populate table if non-NULL
{
RHDL rh;
if (!bkysrch(bl_btr,mode,&rh,blc)) return(false);
if (tbl_imsz!=NULL)     // TODO - implement DYNTBL::load_from_zrec(HDL db)
   {
   int recsz;
   IMSZ *im=(IMSZ*)zrecmem(db,rh,&recsz);
   int i,ct=recsz/sizeof(IMSZ);
   for (i=0;i<ct;i++)
      tbl_imsz->put(&im[i]);
   memtake(im);
   }
return(true);
}

// TODO
// change IMSZ in blc.rh zrec to new structure IMSZRD with rating+drvnum as well as imno+size 
//  extra param DYNAG *t_imszrd(sizeof(IMSZRD))
//        append an entry for every imno where current omdb rating doesn't match 
// Need additional zrec in dbf, containing full list imno+rating+drvnum


void MVDB::get_drvdets(DYNTBL *tbl_drvdet)
{
BL_CARGO blc;
RHDL rh;
OMDB1 om(true);
for (blc.number=0; bkysrch(bl_btr,BK_GT,&rh,&blc) && blc.number<90; )
	{
	DRVDET dd;
	memset(&dd,0,sizeof(DRVDET));
	dd.number=blc.number;
	dd.drive_free_space=blc.drive_free_space;
	dd.rim[0].rating=100; dd.rim[0].imno=BIGLONG;	// Initial "extreme" values to overwrite by lowest key
	int recsz;
	IMSZ *im=(IMSZ*)zrecmem(db,rh,&recsz);
	int i,ct=recsz/sizeof(IMSZ);
	for (i=0;i<ct;i++)
		{
		OM1_KEY omk;
		if (!om.get_om1(im[i].imno,&omk)) m_finish("Imno:%d not found",im[i].imno);
		if (omk.rating<dd.rim[0].rating || (omk.rating==dd.rim[0].rating && omk.imno<dd.rim[0].imno))
			{dd.rim[0].rating=omk.rating; dd.rim[0].imno=omk.imno;}
		if (omk.rating>dd.rim[1].rating || (omk.rating==dd.rim[1].rating && omk.imno>dd.rim[1].imno))
			{dd.rim[1].rating=omk.rating; dd.rim[1].imno=omk.imno;}
		}
	tbl_drvdet->put(&dd);
	memtake(im);
	}
}

bool MVDB::upd_if_changed(BL_CARGO *blc, DYNTBL *tbl_imsz)
{
void *data=tbl_imsz->get(0);
IMSZ *ii=(IMSZ*)data;
int recsz=tbl_imsz->ct*sizeof(IMSZ);
if (!bky_add_or_upd_if_changed(bl_btr,blc,data,recsz))
   return(false);
dbckpt(db);
return(true);
}

void MVDB::del(uchar number)
{
RHDL	rh;
BL_CARGO blc;
blc.number=number;						// CAN'T be zero, 'cos we don't store _seen details in database
bool ok=bkydel(bl_btr,&rh,&blc);
if (!ok) m_finish("del error");
recdel(db,rh);
}
