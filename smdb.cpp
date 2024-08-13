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
#include "log.h"
#include "omdb1.h"
#include "smdb.h"

int _cdecl cp_emki(EMKi *a, EMKi *b)
{
int cmp=cp_long(a,b);					// Major key = imdb_no
if (!cmp) cmp=a->e.locn - b->e.locn;	// Minor key = backup disk number
return(cmp);
}

void SMDB::db_open(char *fn)
{
if ((db=dbopen2(fn))==NULLHDL) m_finish("Error opening %s",fn);
recget(db,dbgetanchor(db),&hdr,sizeof(hdr));						// Read anchor record
if ( hdr.ver<2			// Old version?
||  (flm_btr=btropen(db,hdr.flm_rhdl))==NULLHDL)
	m_finish("Error reading %s",fn);
btr_set_cmppart(flm_btr, (PFI_v_v)cp_emki);					// (db internal)
}

SMDB::SMDB(void)    // maintain smdb.dbf with each copy of each movie in 1 or more FilmsNN folders
{
int rct;
char fn[256];
if (access(get_conf_path(fn, "smdb.dbf"), F_OK ))		// mode=F_OK=0, where non-zero return value means file doesn't exist AT ALL
	{						// mode could be either or both (R_OK|W_OK) for "User has Read / Write access"
	Xecho("Creating database %s\r\n",fn);
	if (!dbist(fn) || (db=dbopen2(fn))==NULLHDL) goto err;
	memset(&hdr,0,sizeof(hdr));
	hdr.ver=2;													// V2 include short 'seen' in EMK (from rating file dttm)
	hdr.flm_rhdl=btrist(db,DT_USR,sizeof(EMKi));
	dbsetanchor(db,recadd(db,&hdr,sizeof(hdr)));
	dbsafeclose(db);
	}
db_open(fn);
return;
err:
m_finish("Error creating %s",fn);
}

bool SMDB::del(EMKi *k)
{
return(bkydel_rec(flm_btr,k));
}

void SMDB::add_or_upd(EMKi *k)
{
EMKi kk;
if (bkysrch(flm_btr,BK_EQ,NULL,memmove(&kk,k,sizeof(EMKi))))
	bkyupd(flm_btr,0,k);
else
	bkyadd(flm_btr,0,k);
}

DYNTBL* SMDB::get(int32_t imno)	// returns table of EMK items (1 per FilmsNN locn for passed imno)
{
DYNTBL *t = new DYNTBL(sizeof(EMK),cp_mem1);
EMKi e;
e.imno=imno;
e.e.locn=0;
while (bkysrch(flm_btr,BK_GT,NULL,&e))
	{
	if (e.imno==imno)
		t->put(&e.e);
	}
return(t);
}

DYNTBL* SMDB::get(uchar locn)	// returns table of all imno's for passed locn
{
DYNTBL *t = new DYNTBL(sizeof(int32_t),cp_ulong);
EMKi e;
int again=NO;
while (bkyscn_all(flm_btr,0,&e,&again))
	if (e.e.locn==locn)
		t->put(&e.imno);
return(t);
}

SMDB::~SMDB()
{
dbsafeclose(db);
}

void SMDB::list_media(int32_t imno)
{
Xecho("MovieNo:%-4d)\r\n",imno);
EMKi e;
e.imno=imno;
e.e.locn=0;
int ct;
for (ct=0; bkysrch(flm_btr,BK_GT,NULL,&e) && e.imno==imno; ct++)
	Xecho("Films%02d  %3.1fGb  Rated:%d  %ld",e.e.locn, 0.1 * (e.e.sz/100000000), e.e.rating, e.e.sz);
if (!ct) Xecho("(No media)");
Xecho("%s","\r\n");
}

void SMDB::list_dvd(void)
{
EMKi e;
int again=NO;
while (bkyscn_all(flm_btr,0,&e,&again))
	if (e.e.dvd) printf("I:tt%07d dvd in Films%02d\n",e.imno,e.e.locn);
}
