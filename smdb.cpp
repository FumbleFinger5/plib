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
#include "cal.h"
#include "log.h"
#include "smdb.h"

int _cdecl cp_em_key(EM_KEY *a, EM_KEY *b)
{
int cmp=stricmp(a->nam,b->nam);
if (!cmp) cmp=cp_short(&a->year,&b->year);
return(cmp);
}

DYNTBL *emkludge;

int _cdecl cp_em_kludge(EM_KEY *a, EM_KEY *b)		// Sort (movies in current folder) by descending user rating
{
int cmp=cp_short_v(b->rating,a->rating);			// 'a' and 'b' reversed, to sort on DESCENDING rating
if (!cmp) cmp=cp_em_key(a,b);
return(cmp);
}

int _cdecl cp_chr(char *a, char *b)
{
if (a[0] < b[0]) return(-1);
if (a[0] > b[0]) return(1);
return(0);
}

int _cdecl cp_em_key_rating(EM_KEY *a, EM_KEY *b)
{
int cmp=cp_chr(&a->rating,&b->rating);
if (!cmp) cmp=cp_em_key(a,b);
return(cmp);
}


void dbsafeclose(HDL db);


void SMDB::db_open(char *fn)
{
if ((db=dbopen(fn))==NULLHDL) m_finish("Error opening %s",fn);
recget(db,dbgetanchor(db),&hdr,sizeof(hdr));						// Read anchor record
if ( !hdr.ver			// No version?
||  (em_btr=btropen(db,hdr.em_rhdl))==NULLHDL)
	m_finish("Error reading %s",fn);
btr_set_cmppart(em_btr,(PFI_v_v)cp_em_key);
}



SMDB::SMDB(const char *epth)
{
char fn[100];
strfmt(fn,"%s/%s",epth,"SMDB.dbf");
//Xecho("Try to open [%s]\r\n",fn);
if (access(fn, F_OK ))		// mode=F_OK=0, where non-zero return value means file doesn't exist AT ALL
	{						// mode could be either or both (R_OK|W_OK) for "User has Read / Write access"
	Xecho("Creating database %s\r\n",fn);
	if (!dbist(fn) || (db=dbopen(fn))==NULLHDL) goto err;
	memset(&hdr,0,sizeof(hdr));
	hdr.ver=1;
	hdr.em_rhdl=btrist(db,DT_USR,sizeof(EM_KEY));
	dbsetanchor(db,recadd(db,&hdr,sizeof(hdr)));
	dbsafeclose(db);
	}
db_open(fn);
return;
err:
m_finish("Error creating %s",fn);
}

void SMDB::locn_upd(char locn)
{
ULOCN u={locn, calnow()};
if (!hdr.locn_upd_rhdl)
	{
	hdr.locn_upd_rhdl=zrecadd(db,&u,sizeof(ULOCN));
	recupd(db,dbgetanchor(db),&hdr,sizeof(hdr));
	return;
	}
int i,sz, ct;
ULOCN *uu=(ULOCN*)zrecmem(db,hdr.locn_upd_rhdl,&sz);
ct=sz/sizeof(ULOCN);
for (i=0; i<ct; i++)
	if (uu[i].locn==u.locn)
		{
		memmove(&uu[i],&u,sizeof(ULOCN));
		hdr.locn_upd_rhdl=zrecupd(db,hdr.locn_upd_rhdl,uu,sz);
		recupd(db,dbgetanchor(db),&hdr,sizeof(hdr));
		memtake(uu);
		return;
		}
uu=(ULOCN*)memrealloc(uu,sz+=sizeof(ULOCN));
memmove(&uu[ct],&u,sizeof(ULOCN));
hdr.locn_upd_rhdl=zrecupd(db,hdr.locn_upd_rhdl,uu,sz);
recupd(db,dbgetanchor(db),&hdr,sizeof(hdr));
memtake(uu);
}

void SMDB::locn_list(void)
{
if (!hdr.locn_upd_rhdl) {sjhlog("No location update record"); return;}
int i,sz, ct;
char s[50];
ULOCN *uu=(ULOCN*)zrecmem(db,hdr.locn_upd_rhdl,&sz);
ct=sz/sizeof(ULOCN);
for (i=0; i<ct; i++)
	sjhlog("Slot:%d updated %s",uu[i].locn,calfmt(s,"%02D-%02O-%04C %02T%02I",uu[i].dttm));
memtake(uu);
}


SMDB::~SMDB()
{
dbsafeclose(db);
};

void SMDB::list_media(EM_KEY *e, RHDL rh, int dets)
{
Xecho("MovieNo:%-4d  %s (%4.4d)  Rated %3.1f\r\n",e->emdb_num, e->nam,e->year,0.1*e->rating);
if (!dets) return;
if (!rh) Xecho("(No media)");
else
	{
	int i, sz, ct;
	EM_MEDIA *mm = (EM_MEDIA*)zrecmem(db,rh,&sz);
	ct=sz/sizeof(EM_MEDIA);
	for (i=0;i<ct;i++) Xecho("%2d:%dMb  ",mm[i].locn,(int)mm[i].sz);
	memtake(mm);
	}
Xecho("%s","\r\n");
}

struct EMTOT {char slot; short ct; __int64 tot;};


void SMDB::list_slots(void)
{
int		again=NO, missing=0;
int		i, j, sz, ct;
EM_KEY	e;
RHDL	rh;
DYNTBL	*t=new DYNTBL(sizeof(EMTOT),(PFI_v_v) cp_chr);
EMTOT	*ett;
long highest=0;
while (bkyscn_all(em_btr,&rh,&e,&again))
	{
	EM_MEDIA *mm = (EM_MEDIA*)zrecmem(db,rh,&sz);	// Returns a nullptr & sets *sz=0 if RHDL is zero
	ct=sz/sizeof(EM_MEDIA);
	if (!ct) missing++;
	for (i=0;i<ct;i++)
		{
		EMTOT et={mm[i].locn,0,0};
		j=t->in_or_add(&et);
		ett=(EMTOT*)t->get(j);
		ett->tot += mm[i].sz;
		ett->ct++;
long zz=mm[i].sz / 1024;
if (zz>100)
{
Xecho("Highest= %ldGb\r\n", highest=zz);
}
		}
	memtake(mm);
	}
Xecho("Tracking %d EMDB Movies, %d with no media\r\n", btrnkeys(em_btr), missing);
__int64 grand=0;
for (ett=(EMTOT*)t->get(i=0);i++ < t->ct;ett++)
	{
	grand+=ett->tot;
	Xecho("Films%02d  %d films - total size %ldGb\r\n", ett->slot, ett->ct,(long)ett->tot/1024);
	}
Xecho("Grand total size %ldGb\r\n",(long)(grand/1024));
delete t;
}

void SMDB::list_numcopies(void)
{
int		again=NO;
int		i,ct=0;
RHDL	rh;
EM_KEY	*e, *ee=(EM_KEY*)memgive(sizeof(EM_KEY)*btrnkeys(em_btr));
for (ct=0;bkyscn_all(em_btr,&rh,&ee[ct],&again);ct++)
	{
	ee[ct].rating = rh ? zrecsizof(db,rh)/sizeof(EM_MEDIA) : 0;	// KLUDGE!!! - use 'rating' field to hold 'number of copies' from EM_MEDIA
																// If rating (=media_ct) > 1, it should be just 1 big file and 1 smaller one
	if (ee[ct].rating==2)
		{
		int sz;
		EM_MEDIA *mm = (EM_MEDIA*)zrecmem(db,rh,&sz);	// Returns a nullptr & sets *sz=0 if RHDL is zero
		__int64 lo=mm[0].sz, hi;
		if (lo < mm[1].sz) hi=mm[1].sz;
		else {hi=lo; lo=mm[1].sz;}
		if (lo > (hi/2))
			m_finish("Fuck");
		}
	}
qsort(ee,ct,sizeof(EM_KEY),(PFI_v_v)cp_em_key_rating);
Xecho("Listing %d EMDB Movies\r\n", ct);
for (e=&ee[i=0];i++<ct;e++)
	Xecho("%d %s (%d) %d\r\n", e->rating, e->nam, e->year, e->imdb_num);
memtake(ee);
}

void SMDB::list_no_media(void)
{
int		again=NO;
int		i;
EM_KEY	e;
RHDL	rh;
DYNTBL	*t=new DYNTBL(sizeof(EM_KEY),(PFI_v_v) cp_em_key_rating);
while (bkyscn_all(em_btr,&rh,&e,&again))
	{
	if (!strcmp(e.nam,"45 Years"))
		{
		i=99;
		i++;
		}
	if (!rh) t->put(&e);
	}

Xecho("Tracking %d EMDB Movies, %d with no media\r\n", btrnkeys(em_btr), t->ct);
for (i=0;i<t->ct;i++)
	list_media((EM_KEY*)(t->get(i)),0,NO);	// NO details of slot+size, 'cos we know there are none
delete t;
}

void SMDB::list_selection(void)
{
int		again=NO;
int		i, sz, tct=0, minrate=90;
EM_KEY	e;
RHDL	rh;
__int64 total=0, biggest;
while (bkyscn_all(em_btr,&rh,&e,&again))
	{
	if (e.rating<minrate) continue;
	EM_MEDIA *mm = (EM_MEDIA*)zrecmem(db,rh,&sz);	// Returns a nullptr & sets *sz=0 if RHDL is zero
	ct=sz/sizeof(EM_MEDIA);
	if (!ct) continue; // no media (movie was streamed)
	for (i=biggest=0;i<ct;i++)
		if (mm[i].sz>biggest) biggest=mm[i].sz;
memtake(mm);
	if (biggest==0) m_finish("Fuckit!");
	Xecho("%7dMb  %s (%4.4d)\r\n", (Ulong)biggest, e.nam, e.year);
	tct++;
	total+=biggest;
	}
Xecho("%7dGb  %d movies rated %2.1f\r\n", (Ulong)(total / 1024), tct, (0.1 * minrate));
// now need to print final line saying grand total disc space + number of high-rated movies
}

void SMDB::josh_copy(DYNTBL *wanted_imdb_numbers)
{
int		again=NO;
__int64 total=0;
int		i;
EM_KEY	e;
RHDL	rh;
DYNTBL	*t=new DYNTBL(sizeof(EM_KEY),(PFI_v_v) cp_em_key_rating);
while (bkyscn_all(em_btr,&rh,&e,&again))
	{
	if (wanted_imdb_numbers->in(&e.imdb_num) != NOTFND)
		{
		int i, sz, ct;
		int biggest_i;
		__int64 biggest;
		EM_MEDIA *mm = (EM_MEDIA*)zrecmem(db,rh,&sz);
		ct=sz/sizeof(EM_MEDIA);
		for (i=biggest=biggest_i=0;i<ct;i++) 
			if (mm[i].sz>biggest) biggest=mm[biggest_i=i].sz;
		if (!biggest) continue; // Eeek! - no media for requested movie!
		Xecho("%2d  %7dMb  %s (%4.4d)\r\n", mm[biggest_i].locn, (Ulong)mm[biggest_i].sz, e.nam, e.year);
		total+=mm[biggest_i].sz;
//		Xecho("Running total size %dMb\r\n", (Ulong)total);
		}
	}
delete t;
}


RHDL SMDB::add_or_upd_media(RHDL rh, EM_MEDIA *m, EM_MEDIA *mm, int ct)	// 'm' is ONE MediaEntry from current scan, 'mm' is current LIST of MediaEntries in *.dbf
{
int i, upd=0;
for (i=0;i<ct;i++)
	if (mm[i].locn==m->locn)
		{
		if (!memcmp(&mm[i],m,sizeof(EM_MEDIA))) goto done; // nothing to do - existing location already present with same newly-scanned filesize
		break;
		}
if (i<ct)		// we did find an existing entry for current locn, but size values are different
	{
	if (m->sz) memmove(&mm[i],m,sizeof(EM_MEDIA));	// If latest scan DID find a copy of this movie (sz!=0), just overwrite previous entry
	else	// DIDN'T find the movie in scan, so DELETE this element from the array
		{
		ct--;		// There's gonna be one LESS entry in the table
		if (i<ct)	// Don't bother doing anything if it's the last (or ONLY) entry in the table
			{
			memmove(&mm[i],&mm[i+1],sizeof(EM_MEDIA)*(ct-i));	// slide all following entries back to cover the element being deleted
			}
		}
	}
else			// this locn wasn't in passed 'mm' array from *.dbf (which might be nullptr if there was no prevrec
	{
/////////////////////////////////////////////////////////////// COULD m->sz be ZERO here??????? If so, we don't want to add it!
//	if (!m->sz) m_finish("ZERO SIZE ELEMENT!");
	if (m->sz)
		{
		ct++;			// There's gonna be one MORE entry in the table
		mm = (EM_MEDIA*)memrealloc(mm,sizeof(EM_MEDIA)*ct);
		memmove(&mm[i],m,sizeof(EM_MEDIA));
		}
	}
if (rh)
	if (ct)
		recupd(db,rh,mm,ct*sizeof(EM_MEDIA));
	else
		{recdel(db,rh); rh=0;}
else if (ct) rh=recadd(db,mm,ct*sizeof(EM_MEDIA));
done:
memtake(mm);		// Don't care if it's a nullptr, 'cos memtake won't attempt to free it anyway
return(rh);			// Returns same RHDL originally passed (data possibly updated), OR a newly-added one, OR 0 if passed RHDL deleted
}

////////////////////////////////////////////////////////
/// For each call, add / upd / remove EM_MEDIA element in RHDL as required (m->sz will be 0 if not found by current scan)
/// AFTER this process, caller will arrange to DELETE any *.dbf key values not in the current active array of valid EM_KEY values
int SMDB::add_or_upd(EM_KEY *e, EM_MEDIA *m)	// Return 0 = Not Updated (key already exists, non-key data unchanged
{												// Return 1 = New key added using passed non-key values (RHDL set to 0)
RHDL rh=0;										// Return 2 = Existing key updated with passed non-key values (RHDL preserved)
EM_KEY ew; // This will be the *.dbf value if it exists, otherwise just a copy of "sought key" from passed 'e'
int ret=0, sz=0, ct=0;
EM_MEDIA *mm=0;
memcpy(&ew,e,sizeof(EM_KEY));

if (m->sz)
	ct=ct;

int fnd=bkysrch(em_btr,BK_EQ,&rh,&ew);	// YES if key already exists
if (!fnd)
	{
	if (m->sz) rh=recadd(db,m,sizeof(EM_MEDIA));
	bkyadd(em_btr,rh,e);
	return(m->sz?3:4);	// 3=Movie NAME+MEDIA added, 4=Only NAME added (no media found in current scan)
	}
if (rh) mm = (EM_MEDIA*)zrecmem(db,rh,&sz);
rh=add_or_upd_media(rh, m, mm, sz/sizeof(EM_MEDIA));			// (Note: this call ALWAYS releases 'mm'
bkyupd(em_btr,&rh,e);
return(2);
}

void SMDB::remove_unmatched(DYNTBL *emk)	// delete any records for keys not in passed array
{
int again=0;
RHDL rh;
EM_KEY e;
memset(&e,0,sizeof(EM_KEY));
while (bkysrch(em_btr,BK_GT,&rh,&e)) // Read every key in database
	if (emk->in(&e)==NOTFND)
		{
		if (!again++) Xecho("%s\r\n","DELETED OBSOLETE DATABASE ENTRIES");
		list_media(&e,rh,YES);
		bkydel_rec(em_btr,&e);
		}
}

int SMDB::return_all_keys(DYNTBL *emk)	// Populate passed table wth all key values in *.dbf (return 'count', or 0 if failed)
{
int again=NO;
EM_KEY e;
while (bkyscn_all(em_btr,0,&e,&again))
	emk->put(&e);
return(emk->ct);
}


int SMDB::scan_all(EM_KEY *e, int reverse_order, int *again)
{
if (!(*again)++)
	memset(e,reverse_order?255:0,sizeof(EM_KEY));
return(bkysrch(em_btr,reverse_order?BK_LT:BK_GT,0,e));
}

void SMDB::find_movie_by_imdb_num(int imdb_num)	// Find which discs have this movie
{
int i, again=NO, sz=0;
EM_KEY e;
while (scan_all(&e, YES, &again))
	{
	if (e.imdb_num!=imdb_num) continue;
	RHDL rh;
	if (!bkysrch(em_btr,BK_EQ,&rh,&e)) m_finish("bums!");
	list_media(&e,rh,YES);
	return;
	}
Xecho("IMDB MovieNumber %d not in database\r\n",imdb_num);
}

static int cp_sz_locn(__int64 sz_a, __int64 sz_b, char locn_a, char locn_b) 
{
int cp=0;
if (sz_a>sz_b) cp=1;
else if (sz_a<sz_b) cp=-1;
else if (locn_a>locn_b) cp=1;
else if (locn_a<locn_b) cp=-1;
return(cp);
}


// HERE'S where the donkey work gets done (decide which if any MOVE command to write to PURGE.BAT)
static int get_move_type(int current_slot, EM_MEDIA *mm, int ct)	// return 0 if current "slot" is the biggest file on the lowest-numbered disc (KEEP)
{							// ...ELSE return 1 if it's a "wanted" smaller copy of the movie to be kept in \ALT, or 2 if it's just "unwanted" \SPARE
EM_MEDIA *m=0;
int i, fnd=NOTFND;	// 'bi' = subscript into biggest media + lowest disc
for (i=0;i<ct;i++)	// First, make sure there's a copy of this movie in the "current_slot" location
	{
	if (mm[i].locn==current_slot) fnd=i;	// Flag the fact that a copy of this movie exists in the current movie folder
	if (!m || mm[i].sz>m->sz || (mm[i].sz==m->sz && mm[i].locn<m->locn)) m=&mm[i];	// 'm' points to 'most wanted' copy, which might not be 'current'
	}
if (fnd==NOTFND || m->locn==current_slot) return(0);		// Do nothing - this movie isn't in the current folder anyway, OR it's the 'most wanted' copy
// We don't want this copy of the movie, so RETURN 1 (caller will move it to \SPARE)
// UNLESS it's the 2nd biggest copy that's no more than HALF size of biggest copy - IN WHICH CASE RETURN 2 (caller will move it to \ALT)

int j=NOTFND;	// set 'j' to point to 2nd biggest copy (if there is an elligible one)
for (i=0;i<ct;i++)
	if (mm[i].sz < (m->sz/2))
		if (j==NOTFND || mm[i].sz>mm[j].sz || (mm[i].sz==mm[j].sz && mm[i].locn<mm[j].locn)) j=i;

if (j==fnd) return(1);	// "MOVE" to \ALT (keep for posterity, it's the SECOND-BIGGEST copy of this movie)
return(2);				// "move" to \SPARE (to be deleted manually later)
}

// Write a command file into the root directory of the passed "movie_folder_pth" drive.
// For each movie present in the current folder, write a DOS MOVE command to move the files to either \ALT or \SPARE...
// ...IF...
// This is the biggest copy (AND it's in the lowest "\FILMSnn foldernumber" if > 1 "biggest copy"), DO NOTHING (leave it where it is)
// ...ELSE...
// write a command into \SPARE.BAT to move it to \spare\*.*
// ...UNLESS...
// it's the lowest-numbered foldernumber for the largest sub-5Gb copy AND there's a copy OVER 5Gb on some other disc,...
// IN WHICH CASE write a command into \ALT.BAT to move it to \alt\*.*
//
// As a nicety, write the command as uppercase MOVE "[movie]" \SPARE or lowercase move "[movie]" \ALT
void SMDB::purge(char *movie_folder_pth)	// Passed path (to media folder) includes drive letter, NOT wanted in PURGE.BAT
{
char	s[256];
strcpy(s,movie_folder_pth);
int		again=0, i=strlen(s)-2;
int		current_slot=a2i(&s[i],2);
int		ct_a=0, ct_p=0;
strcpy(&s[2],"/PURGE.BAT");
HDL		f=0;
RHDL	rh;
EM_KEY e;
while (bkyscn_all(em_btr,&rh,&e,&again))
	if (rh)
	{
	int sz, ct;
	EM_MEDIA *mm = (EM_MEDIA*)zrecmem(db,rh,&sz);
	ct=sz/sizeof(EM_MEDIA);
	int move=get_move_type(current_slot, mm,ct);
	memtake(mm);
	if (!move) continue;	// Either this is the biggest ("master") copy, or this movie isn't in the passed path at all, so DO NOTHING
	if (!f)
		{
		f=flopen(s,"w");	// CAREFUL! - MIGHT RE-USE 's'
		flputln("IF NOT EXIST /ALT MD /ALT",f);
		flputln("IF NOT EXIST /SPARE MD /SPARE",f);
		}
	if (move==1) ct_a++; else ct_p++;
	const char *mv,*mv1;
	if (move==1) {mv="MOVE"; mv1="ALT";} else {mv="move"; mv1="SPARE";}
	strfmt(s,"%s %c%s%c%s (%04d)%c %s ",mv, 34, &movie_folder_pth[2], 92, e.nam, e.year, 34, mv1);
	flputln(s,f);
	}
if (f)
	{
	flclose(f);
	Xecho("PURGE.BAT = Move %d movies to /ALT, %d to /SPARE\r\n",ct_a,ct_p);
	}
else Xecho("No movies need to be moved from %s to /ALT or /SPARE\r\n", movie_folder_pth);
}

int SMDB::get(EM_KEY *e, const char *nam, int year)
{
if (nam)
	{
	strcpy(e->nam,nam);
	e->year=year;
	}
if (bkysrch(em_btr,BK_EQ,&rhdl,e))
	{
	return(YES);
	}
return(NO);
}
