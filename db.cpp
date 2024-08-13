#include <assert.h>
// second line added just to see if GIT notices
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <cstdarg>

#include "pdef.h"
#include "db.h"
#include "memgive.h"
#include "flopen.h"
#include "str.h"
#include "drinfo.h"
#include "log.h"


//========
#include <unistd.h>
//#include <sys/wait.h>
//#include <stdio.h>
//#include <utime.h>

//#include <time.h>
//===========

static void bkysetpos(int last);
static short bkynxtkey(RHDL *rhdl, void *key);

#define XR_BIT 0x01000000 //Xrec Open modes
#define IS_XREC(rhdl) (((rhdl)&XR_BIT) ? YES : NO)

#define CELL_CONTAINS_DATA 0x4000
#define ENDFREE 0
#define FARCELL 0x8000
#define F_NEW_PGS 0x01
#define F_RDONLY 0x02
#define F_TEMP 0x08 // Delete on close (Database flag)
#define HIPGMASK 0x00ff0000
#define IN_LFC 0xf000
#define N_IDBITS 9
#define RH_IDMASK 0x01ff
#define RH_PGMASK 0x7fff
#define SIZPART 0x0fff

/*	Record types for predefined database records.	*/

#define RT_DBFHDR 0x8001 /* database file header record.	*/
#define RT_PGMAP 0x8002	 /* page map record.		*/
#define RT_BHDR 0x8005	 /* b-tree header record.	*/
#define RT_BNODE 0x8004	 /* b-tree node record.		*/

/*
*	the pages look as follows :
*	
*	[ <data>      byte 0
*	   |
*	   V downward 	    
*
*	  ^ upward
*	  |	  
*	cell list  
*	<ncells>		
*	<free area>
*/

/*	THIS STRUCTURE MUST NOT EXCEED ONE PARARGAPH IN LENGTH */
struct CELLTBL
{
	short ncells, vector_siz, lfc; /*list of free cells*/
	ushort free_off, free_sp;
};

struct BUF
{
	BUF *nxt, *prv;
	short pg_in_buf, bflags;
	HDL h_fl;
	char *addr;
};
// 'pg_in_buf' s/b ushort <<<<<<<<<<<<<<<<<????

struct PGVARS
{
	BUF *buf;
	short cell_id, data_siz;
	char *pg;
	CELLTBL *celltbl;
	short *vector_elm;
	short *pcell;
	short cell_siz;
	RHDL rh_far_cell;
};

#pragma pack(push, 1)
typedef struct
{
	short unused_rec_typ; // record type of stored pg tables
	short unused_max_pgs;
	short lst_pg; // subscript no of last page currently used within this map
	RHDL rh_nxt_pgmap;
	int32_t dummy2;
	int32_t dummy3;
	ushort map_free_sp[1]; // Number of free bytes in each page within this map
} PGMAP;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
	short unused_rec_typ;
	short fill;
	RHDL rh_anchor, unused_rh_pgtbl; /* free sector map */
	short versn, unused_rlse;
	short unused_pg_siz;
	int32_t unused_create_dttm, unused_upd_dttm;
	int32_t dum1, dum2;
} DBFHDR;
#pragma pack(pop)

#define LEAF 127
#define BRANCH 0
#define MAXDEPTH 10

#define H_BTR ((BTR *)h_btr)

struct BTR
{
	HDL h_nxtbtr, h_prvbtr,
		h_db;			   // HDL of database containing btree
	RHDL rh_bhdr,		   // RHDL of btree header
		rh_root,		   // RHDL of current root
		rh_node[MAXDEPTH], // stack
		actv_leaf;		   // HDL of leaf containing the active key
	int32_t keyct;			   // number of keys in this btree
	short keysiz, keytyp,  // size & type of key
		flg,			   // 1=Key exists, 2=Key has been processed
		depth;			   // current search depth
	PFI_v_v cmppart;	   // Comparator function (maybe app-supplied)
						   //#ifdef DB_VERIFY
	char *(_cdecl *formatter)(short prm, void *key);
	short formatter_parm;
	//#endif
	char actv_key[8]; // Actually, the key itself goes here, followed
};					  // by space for 2 RHDL's ( - why not just one?)

#pragma pack(push, 1)
struct BNODE
{
	ushort z_bnrec_typ;
	short node_typ, max_bkeys;
	char fill1[6];
	short bnkey_ct;
	int32_t rh_lf_sib, rh_rt_sib;
	char fill2[14];
	char keys[sizeof(int32_t)];
};

/*	Header for a btree.  Holds count of the keys in the tree */
/*	more important, it contains the RHDL of the btree root */
struct BHDR
{
	ushort bhrec_typ; /* 'bh' - because ci-86 needs unique names */
	RHDL bhrh_root;
	int32_t bhkey_ct;
	char cargo[22];
	short fill, keytyp, keysiz;
};
#pragma pack(pop)

static void _recdel(RHDL rhdl);		   // pre-declare for far_cell recursion
static void xrecrlsbuckets(RHDL rhdl); //AJB static() in block

int dbsafe;
int dbsafe_catch;

#define UNUSED_VECTOR ((ushort)0xffff)
#define DB struct _DB
#define _H_DB ((DB *)_h_db)
#define DLRING struct _dlring
#define MINRECSIZ 4 /* record must be able to hold another record handle*/
#define _200K 204800L
#define FST_PG_IN_MAP(x) ((x)*DBMPGSPMAP)
#define NOSUCHBUF (short)0xfffe
#define MAXPGCT 0x7fff
#define MAXPGMAPS 50	 /* allows 2M * MAXPGMAPS bytes/file==100Mb max */
#define XM_READ 0x0001	 /* extended record mode - read      */
#define XM_WRITE 0x0002	 /* extended record mode - write     */
#define XM_APPEND 0x0006 /* extended record mode - append.   */
#define SIZBUCKET (sizeof(RHDL) + sizeof(short))
#define ADDBUCKETS 4
#define MAXBUCKETS 300
#define RSVDID ((ushort)0xfff0)
#define DBMPGSPMAP (((MAXRECSIZ - sizeof(PGMAP)) / sizeof(short)) + 1)
#define RHDLPG(rhdl) ((ushort)((((rhdl & HIPGMASK) >> N_IDBITS) | ((rhdl >> N_IDBITS) & 0x7f)) & RH_PGMASK))
#define RHDLCELL(rhdl) ((ushort)(rhdl & RH_IDMASK))
#define DBPVECTOR(pg) ((short *)(((char *)(pg)) + (PGSIZ - FOOTERSIZ)))
#define DBPCELLTBL(pg) ((CELLTBL *)(((char *)(pg)) + (PGSIZ - sizeof(CELLTBL))))
#define SETLOCK(f, l) (f) = ((short)(((f) & ~LOCKED) | (l)))
#define BTRPSHNODE(rhnode) _h_btr->rh_node[++(_h_btr->depth)] = rhnode
#define BTRPOPNODE ((_h_btr->depth > 0) ? _h_btr->rh_node[_h_btr->depth--] : NULLRHDL)
#define BTRSTKLVL(dpth) (((dpth) > 0) ? _h_btr->rh_node[(dpth)] : NULLRHDL)
#define RHDLIST(p, c) ((RHDL)(((Ulong)(p) << N_IDBITS) | ((Ulong)(c)&RH_IDMASK)))
#define BNDKEY(b, p) (b->keys + (((p)-1) * _btr_ksz4))
#define BNDRHDL(b, p) *((RHDL *)(b->keys + ((p)*_btr_ksz4 - 4)))

#pragma pack(push, 1)
struct PGMAPS
{
	RHDL rh_pgmap;
	int32_t tot_free; // SJH 04/05/04 - This needs checking out - it's not being properly maintained
};

struct MAPMAP
{
	short nmaps, pgs_in_db;
	PGMAPS pgmaps[MAXPGMAPS]; // limits database size to 100Mb
};
#pragma pack(pop)

static MAPMAP *_mapmap;
static HDL _h_db;

DB
{
	HDL h_nxtdb, h_prvdb;
	HDL h_db_fl, btrs;
	MAPMAP mapmap;
	ushort db_flg; // 1=new pages, 2=Read Only, 4=No Rebalance in Del
				   //	ushort	pgsz;
	char db_fnam[1];
};

DLRING { DLRING *nxt, *prv; };
//static  HDL	_h_btr;
static BTR *_h_btr;
static HDL _dbt;
static HDL _dbfil;

static int _pgsz;

static int _lock; // ,failsafe;

#define BK_EXST 1
#define BK_PRCD 2
static int _btr_ks0,	// size of main btrkey (not incl. 4 for dup RHDL)
	_btr_ksz4,			// We often need this - it's just "_btr_ks0+4"
	_btr_kpn;			// Keys per Node
static short *_btr_flg; // ptr to flag for actv exists/processed

// Post-180598, ksz4 is 'Full KeyLength+sizeof(RHDL)', where KeyLength may
// be greater than the actual number of bytes used for key comparisons, and
// the extra bytes contain a small amount of data that's not worth putting
// in a separate record and storing under the keys's associated RHDL.

/////////// start of memtaker.c and dlr.c
/***
* name :	Memtakelist - release cells in a list of linked cells
*
* synopsis:	fst = Memtakelist(off,list,stop)
*		fst	- new first cell in list.  either STOP or NULL.
*		off	- offset of next ptr within the cell.
*		list	- first cell in the list.
*		stop	- cell on which to stop deletion.
*		
* description:	Memtakelist() expects a ptr to a list of cells
*		which contain a ptr to the next cell in the
*		first postion in the CELL, ie. struct {char *pnxt;}.
*		if LIST is NULL, then the list is empty.  if the 
*		list is not empty, then the PNXT ptr in the
*		final element in the list should contain a NULL
*		ptr.  Memtakelist() deletes all cells in the list
*		until the NULLPTR at the end of the list if found.
***/
#ifdef UNUSED_
static void memtakelist(void *plist)
{
	char *pnxt;
	while (plist)
	{
		pnxt = *((char **)((char *)plist));
		memtake(plist);
		plist = pnxt;
	}
}
#endif

/*  This is a set of routines which allow you to use a doubly
    linked ring of cells.  these cells may contain any information
    but the first two elements of the cell must be two ptr variables
    which will be manipulated by the DLR (dbl linked ring) functions.

    in addition, there is a ptr which indicates the 'first' element
    in the ring.  this is called the HEAD of the ring.  if the ring
    is empty, then the head will be NULL.  when you add or delete an element
    the (possibly changed) HEAD will be returned. */

/*  remove this element from the ring whose head is at PHD.
    if this element is currently at the head of the ring,
    then update the header ptr to the next element or
    to NULL if this was the only element in the ring. */
static void *dlrsnap(DLRING *phd, DLRING *pelm)
{
	if (phd)
	{
		if (phd == pelm && (phd = pelm->nxt) == pelm)
			phd = 0; /* PELM only elm in ring */
		if (pelm)
		{
			pelm->nxt->prv = pelm->prv;
			pelm->prv->nxt = pelm->nxt;
			pelm->nxt = pelm->prv = pelm;
		}
	}
	return (phd);
}

//	Link in a new element at the head of the ring.  This element must not currently be part of the ring.
//	phd==NULL if the ring is currently empty.
static void *dlrlink(DLRING *phd, DLRING *ins)
{
	if (phd)
	{
		phd->prv->nxt = ins;
		ins->nxt = phd;
		ins->prv = phd->prv;
		phd->prv = ins;
		return (phd);
	}
	return (ins->nxt = ins->prv = ins);
}

//	Moves an element already in this ring to become head of the ring.  (good for LRU algorithms.)
//	The element is moved from its current position to become the element before the current head of the ring,
//	then the head of the ring is changed to the moved element. The old first elm is now the second, etc.
static void *dlrtohd(DLRING *phd, DLRING *pelm)
{
	if (phd != pelm)
	{
		dlrsnap(phd, pelm);
		dlrlink(phd, pelm);
		return (pelm); /* pelm is now head of ring */
	}
	return (phd);
}

#ifdef dont_delete
static void *dlrnxt(DLRING *phd, DLRING *pelm)
{
	DLRING *nxt;
	if (phd)
	{
		if (pelm)
		{
			if ((nxt = pelm->nxt) == phd)
				nxt = 0;
		}
		else
			nxt = phd; /* pelm==NULLHDL, first request for next */
	}
	else
		nxt = 0; /* end of ring, ring is empty */
	return (nxt);
}
#endif
/////////// end of memtaker.c and dlr.c

#define DIRTY_PG 1
#define HAS_NEWPG 2
#define LOCKED ((short)4)
#define EMPTY -1

////////////^^^^^^^^^^^ THAT'S THE CLAG FOR 65Mb!!!!! - pg MUST BE USHORT!!!

static short _bf_ct;
static BUF *_bf_head, *_bf_addr;

static void bfspist(int nbufs) // create a ring of doubly-linked
{							   // Buffers (must be at least 4)
	BUF *buf;
	_bf_head = _bf_addr = buf = (BUF *)memgive(nbufs * sizeof(BUF)); // Make bufs contiguous
	(buf->prv = &buf[nbufs - 1])->nxt = buf;
	for (_bf_ct = 0; _bf_ct < nbufs; _bf_ct++)
	{
		if (_bf_ct)
			buf->prv = &_bf_addr[_bf_ct - 1];
		if (_bf_ct < nbufs - 1)
			buf->nxt = &_bf_addr[_bf_ct + 1];
		buf->pg_in_buf = EMPTY;
		buf->addr = (char *)memgive(PGSIZ);
		buf++;
	}
}

static void bfsprls(void)
{
	while (_bf_ct)
		memtake(_bf_addr[--_bf_ct].addr);
	Scrap(_bf_addr);
}

static void bftodisc(BUF *buf)
{
	if (buf->bflags & DIRTY_PG)
	{
		if (buf->pg_in_buf != EMPTY) // needs flushing
		{
			flputat(buf->addr, PGSIZ, (int32_t)(buf->pg_in_buf) * PGSIZ, buf->h_fl);
		}
		buf->bflags &= ~(HAS_NEWPG | DIRTY_PG); // Reset flags
	}
}

static BUF *bfsrch(int pg, int read) // Find/Read 'pg' in a Buffer
{									 // Scan ring from current 'head'.
	BUF *buf = _bf_head, *avail = 0; // If n/f, read in a free Buffer
	short got = NO;					 // Move the Buffer we found (or read) to ring head
	do
	{
		if (buf->h_fl == _dbfil && buf->pg_in_buf == pg)
		{
			got = YES;
			break;
		}
		if (!(buf->bflags & LOCKED))
			avail = buf;
	} while ((buf = buf->nxt) != _bf_head);
	if (!got) // didn't find this page already in a Buffer
	{		  // clean LRU Buffer so can read wanted page
		if ((buf = avail) == 0)
			throw SE_NOFREEBUFS; // all pages protectd!
		bftodisc(buf);
		buf->bflags &= ~LOCKED;
	}
	_bf_head = (BUF *)dlrtohd((DLRING *)_bf_head, (DLRING *)buf); // the LRU algorithm!

	if (!got && read) // this can only happen on what was a dbppgtomem() call
	{
		if (pg >= _mapmap->pgs_in_db)
			throw SE_BADPGID;
		flgetat(buf->addr, PGSIZ, (int32_t)(buf->pg_in_buf = (ushort)pg) * PGSIZ, buf->h_fl = _dbfil);
		buf->bflags &= ~(HAS_NEWPG | DIRTY_PG); // Reset flags
	}
	return (buf); // return ptr-> found (or read in free) Buffer
}

static void bffilflush(short bufrls)
{
	BUF *b = _bf_head;
	do
	{
		if (b->h_fl == _dbfil)
		{
			bftodisc(b);
			if (bufrls)
			{
				b->pg_in_buf = EMPTY;
				b->h_fl = 0;
				b->bflags = 0;
			}
		}
	} while ((b = b->nxt) != _bf_head); /* do until back at head */
}

static int dbcmptcellsiz(int data_siz)
{
	int cell_siz = (data_siz & SIZPART) + (data_siz & 0x01); // Force even number
	if (cell_siz < MINRECSIZ)
		cell_siz = MINRECSIZ;
	else if (cell_siz > MAXRECSIZ)
		cell_siz = MAXRECSIZ;
	return (cell_siz + sizeof(short)); /* add 2 for stored length */
}

/*
pg	ptr -> beginning of page
celltbl ptr -> last FOOTERSIZ of pg, holds cell info
vector ptr -> just after first byte of cell Vector (i.e. page_size - footersiz_siz)
the Vector runs toward the low memory of the cell while the data grows toward high memory from beg

get the new cell id first, since that might snarf up some extra free space.  so do before checking if enough free space.
(the cell_id is the Vector entry) */
static void dbpsetvars(PGVARS *pv, BUF *buf, short cell_id)
{
	int f_far, ofst;
	ushort pve;
	char *pg;
	pv->buf = buf;
	pv->cell_id = cell_id;
	short *vector = DBPVECTOR(pg = pv->pg = buf->addr);
	pv->celltbl = DBPCELLTBL(pg);
	pve = *(pv->vector_elm = vector - cell_id);
	if (pve == UNUSED_VECTOR || pve == RSVDID)
	{
		pv->pcell = (short *)pg;
		f_far = pv->cell_siz = 0;
	}
	else
	{
		pv->pcell = (short *)(pg + pve);
		f_far = (pv->data_siz = *pv->pcell) & FARCELL;
		pv->cell_siz = (short)dbcmptcellsiz(pv->data_siz &= SIZPART);
	}
	pv->rh_far_cell = f_far ? *((RHDL *)(pv->pcell + 1)) : 0;
	if (cell_id <= 0 || cell_id > ((CELLTBL *)pv->celltbl)->vector_siz || (ofst = ((char *)pv->pcell - pg)) < 0 || ofst > PGSIZ)
		throw SE_BADCELLID;
}
// pg pve vector

#define dbpsetvars_r(pv, rhdl) dbpsetvars((pv), bfsrch(RHDLPG(rhdl), YES), RHDLCELL(rhdl))
#define set_dirty(buff_flag_addr) (*(buff_flag_addr) |= DIRTY_PG)

static void dbpmrkpgdirty(RHDL rh_rec)
{
	PGVARS pv;
	short *flg;
	dbpsetvars_r(&pv, rh_rec);
	flg = &pv.buf->bflags;
	if (pv.rh_far_cell) // if cell was moved to far page
	{
		short prv = (short)(*flg & LOCKED); // save existing lock Status
		*flg |= LOCKED;						// lock 'base' record temporarily
		dbpmrkpgdirty(pv.rh_far_cell);		// mark far cell as dirty
		SETLOCK(*flg, prv);					// reinstate lock Status of base record
	}
	else
		set_dirty(flg);
}

static void dbactv(HDL h_db)
{
	if ((_h_db = h_db) == 0)
	{
		_dbfil = 0;
		throw SE_NULLHDL;
	}
	_dbfil = _H_DB->h_db_fl;
	//_pgsz=_H_DB->pgsz;
	_mapmap = &_H_DB->mapmap;
}

static void *_recadr(RHDL rh_rec, short *psiz, short **flg)
{
	PGVARS pv;
	dbpsetvars_r(&pv, rh_rec);
	if (pv.rh_far_cell)
		return (_recadr(pv.rh_far_cell, psiz, flg));
	if (flg)
		*flg = &pv.buf->bflags;
	if (psiz)
		*psiz = pv.data_siz; /* retrieve cell size less stored len*/
	return (pv.pcell + 1);	 /* move to data after stored len     */
}

static void *_recadr0(RHDL rh_rec, short *psiz) { return (_recadr(rh_rec, psiz, 0)); }
static void *_recadr00(RHDL rh_rec) { return (_recadr(rh_rec, 0, 0)); }
static void *_recadr1(RHDL rh_rec, short **flg) { return (_recadr(rh_rec, 0, flg)); }

static void *_recadr1d(RHDL rh_rec)
{
	short *flg;
	void *ad = _recadr(rh_rec, 0, &flg);
	set_dirty(flg);
	return (ad);
}

static void bhdrupd(RHDL rhdl, RHDL root, int32_t key_ct)
{
	short *flg;
	BHDR *b = (BHDR *)_recadr1(rhdl, &flg);
	if (b->bhrh_root != root || b->bhkey_ct != key_ct)
	{
		b->bhrh_root = root;
		b->bhkey_ct = key_ct;
		set_dirty(flg);
	}
}

int cp_bytes(const void *a, const void *b)
{
	return (memcmp(a, b, _btr_ks0));
}

int cp_strn_ks0(const void *a, const void *b)
{
	return (strncmp((const char *)a, (const char *)b, _btr_ks0));
}

static void btrbind(HDL h_btr)
{
	if ((HDL)_h_btr != h_btr)
	{
		_h_btr = (BTR *)h_btr;
		_btr_ksz4 = (short)((_btr_ks0 = _h_btr->keysiz) + 4);
		_btr_flg = &_h_btr->flg;
		_btr_kpn = (short)((MAXRECSIZ - (sizeof(BNODE) - sizeof(RHDL))) / _btr_ksz4);
	}
	dbactv(_h_btr->h_db);
}

// short btrkeysiz(HDL h_btr) {return(H_BTR->keysiz);}

// Copy the found key value and rhdl out to (the user's) addresses
static void btrakeyret(RHDL *rhdl, void *ky)
{
	if (ky)
		memmove(ky, _h_btr->actv_key, _btr_ks0);
	if (rhdl)
		*rhdl = *(RHDL *)(_h_btr->actv_key + _btr_ks0);
}

short btrkeytyp(HDL h_btr)
{
	return (H_BTR->keytyp);
}

#define FIX_KEYCT 1
int32_t btrnkeys(HDL h_btr) // *again
{
#ifdef FIX_KEYCT
	if (H_BTR->keyct < 2)
	{
		int32_t ct;
		btrbind(h_btr);
		for (bkysetpos(ct = NO); bkynxtkey(0, 0); ct++)
		{
			;
		}
		if (ct != H_BTR->keyct)
		{
			H_BTR->keyct = ct;
			extern int flopen_status;
			flopen_status = 2;
			HDL f = flopen("\\SJH_Err.log", "a");
			if (f)
			{
				char wrk[256];
				strfmt(wrk, "KeyCount error in %s", flnam(_dbfil));
				flputln(wrk, f);
				flclose(f);
			}
		}
	}
#endif
	return (H_BTR->keyct);
}

// Macro is slightly faster, but a bit costly on space!
#ifndef usemac
static void bndinskey(BNODE *b, int pos, void *key)
{
	memmove(&b->keys[(pos - 1) * _btr_ksz4], key, _btr_ks0);
}
#else
#define bndinskey(b, p, k) memmove(&b->keys[(p - 1) * _btr_ksz4], k, _btr_ks0)
#endif

static void btrupdrt(RHDL new_root)
{
	_h_btr->rh_root = ((BHDR *)_recadr1d(_h_btr->rh_bhdr))->bhrh_root = new_root;
}

void btr_set_cmppart(HDL h_btr, PFI_v_v func)
{
	H_BTR->cmppart = func;
}

HDL btropen(HDL h_db, RHDL rh_bhdr)
{
	BHDR *bhdr;
	BTR *b;
	dbactv(h_db);
	if ((bhdr = (BHDR *)_recadr00(rh_bhdr)) == 0)
		return (0);
	b = (BTR *)memgive(sizeof(BTR) + bhdr->keysiz);
	b->rh_bhdr = rh_bhdr;
	b->h_db = _h_db;
	b->keyct = bhdr->bhkey_ct;
	b->keysiz = bhdr->keysiz; // key0 size
	b->rh_root = bhdr->bhrh_root;
	switch (b->keytyp = bhdr->keytyp)
	{
	case DT_BYTES:
		b->cmppart = cp_bytes;
		break;
	case DT_STR:
		b->cmppart = cp_strn_ks0;
		break;
	case DT_SHORT:
		b->cmppart = cp_short;
		break;
	case DT_USHORT:
		b->cmppart = cp_ushort;
		break;
	case DT_LONG:
		b->cmppart = cp_int32_t;
		break;
	case DT_ULONG:
		b->cmppart = cp_ulong;
		break;
	}
	dlrlink((DLRING *)_H_DB->btrs, (DLRING *)b);
	_h_btr = 0; // Make sure it doesn't seem to be 'currently bound'!
	return (_H_DB->btrs = (HDL)b);
} // Ins this btr at front of this db's linked list of btr's and return it

void *btrcargo(HDL h_btr, void *cargo)
{
	short *flg;
	BHDR *b;
	btrbind(h_btr);
	if ((b = (BHDR *)_recadr1(_h_btr->rh_bhdr, &flg)) == 0)
		return (0);
	if (cargo)
	{
		memmove(b->cargo, cargo, 22);
		set_dirty(flg);
	}
	return (b->cargo);
}

void btrclose(HDL h_btr)
{
	btrbind(h_btr);
	bhdrupd(_h_btr->rh_bhdr, _h_btr->rh_root, _h_btr->keyct);
	bffilflush(NO);
	_H_DB->btrs = (char *)dlrsnap((DLRING *)_H_DB->btrs, (DLRING *)_h_btr);
	memtake(_h_btr);
	_h_btr = 0;
}

// Flush out any unclosed b-trees for the database being closed
static void btrallclose(void)
{
	HDL h_btr;
	while ((h_btr = _H_DB->btrs) != 0)
		btrclose(h_btr);
}

// Return a page number within this pagemap with at least 'siz' bytes free, or NOTFND if no page has enough space
static int _pgmscnpg(int siz, int nmap)
{
	PGMAP *pm = (PGMAP *)_recadr00(_mapmap->pgmaps[nmap].rh_pgmap);
	for (int i = 0; i <= pm->lst_pg; i++)
		if ((pm->map_free_sp[i] & SIZPART) >= siz)
			return (i + FST_PG_IN_MAP(nmap));
	return (NOTFND);
}

static void dbpinit(void *pg)
{
	CELLTBL *celltbl = DBPCELLTBL(pg);
	celltbl->free_sp = (PGSIZ - FOOTERSIZ);
	celltbl->lfc = celltbl->free_off = celltbl->ncells = celltbl->vector_siz = 0;
}

static void *dbdummypg(char *pg, short data_siz)
{
	dbpinit(pg);
	int cell_siz = dbcmptcellsiz(data_siz);
	CELLTBL *celltbl = DBPCELLTBL(pg);
#pragma warning(push, 3)
	celltbl->free_sp -= (cell_siz + sizeof(short)); // gobble cell_siz, plus one short for vectore elm
#pragma warning(pop)
	*(short *)&pg[PGSIZ - FOOTERSIZ - sizeof(short)] = 0;
#pragma warning(push, 3)
	celltbl->free_off += cell_siz; /* advance free ptr over used space */
#pragma warning(pop)
	celltbl->ncells++;
	celltbl->vector_siz++;
	*(short *)pg = data_siz; /* store cell size  and advance cell ptr to data */
	return (&pg[2]);
}

// Initialise a new page map
static void dbmist(PGMAP *pm)
{
	pm->lst_pg = -1;
	for (int i = DBMPGSPMAP; i--;)
		pm->map_free_sp[i] = PGSIZ - FOOTERSIZ;
	pm->rh_nxt_pgmap = 0;
}

static void _dbckpt(void)
{
	bffilflush(NO);
	if (_H_DB->db_flg & F_NEW_PGS)
	{
		flckpt(_dbfil);
		_H_DB->db_flg &= ~F_NEW_PGS;
	}
}

static PGMAP *_pgmappnewmap(PGMAP *pm)
{
	short pgnum, prv;
	BUF *buf;
	RHDL rh_new_pgmap,
		rh_cur_pgmap = _mapmap->pgmaps[_mapmap->nmaps - 1].rh_pgmap;
	_mapmap->pgs_in_db++; // figure out page and HDL of new page map
	pm->map_free_sp[++(pm->lst_pg)] = 0;
#pragma warning(push, 3)
	pgnum = pm->lst_pg + FST_PG_IN_MAP(_mapmap->nmaps - 1);
#pragma warning(pop)
	pm->rh_nxt_pgmap = rh_new_pgmap = RHDLIST(pgnum, 1); // update mapmap & set chainlink in old pgmap
	set_dirty(&bfsrch(RHDLPG(rh_cur_pgmap), NO)->bflags);
	_mapmap->pgmaps[_mapmap->nmaps++].rh_pgmap = rh_new_pgmap;
#pragma warning(push, 3)
	buf = bfsrch(NOSUCHBUF, NO); // get a free Buffer to work in
	prv = buf->bflags & LOCKED;
#pragma warning(pop)
	buf->bflags |= LOCKED; // protect the working Buffer
	buf->h_fl = _dbfil;
	buf->pg_in_buf = pgnum;
	dbmist((PGMAP *)dbdummypg(buf->addr, MAXRECSIZ)); // build page with 1 record in it
	set_dirty(&buf->bflags);						  // make sure new map is output
	SETLOCK(buf->bflags, prv);
	_H_DB->db_flg |= F_NEW_PGS;
	return ((PGMAP *)_recadr00(rh_new_pgmap));
}

static void _applogpg(short pgnum)
{
	BUF *buf;
	_mapmap->pgs_in_db++;
#pragma warning(push, 3)
	buf = bfsrch(NOSUCHBUF, NO);
#pragma warning(pop)
	dbpinit(buf->addr);
	buf->h_fl = _dbfil;
	buf->pg_in_buf = pgnum;
	buf->bflags |= (DIRTY_PG | HAS_NEWPG); //jjjJ
	_H_DB->db_flg |= F_NEW_PGS;
}

static short _pgmapppg(void)
{
	RHDL rh_pgmap = _mapmap->pgmaps[_mapmap->nmaps - 1].rh_pgmap;
	PGMAP *pm = (PGMAP *)_recadr00(rh_pgmap);
	if (pm->lst_pg == DBMPGSPMAP - 2) // current pagemap full, so make a new one
	{
		pm = _pgmappnewmap(pm);
		rh_pgmap = _mapmap->pgmaps[_mapmap->nmaps - 1].rh_pgmap;
	}
#pragma warning(push, 3)
	short pgnum = ++(pm->lst_pg) + FST_PG_IN_MAP(_mapmap->nmaps - 1);
#pragma warning(pop)
	set_dirty(&bfsrch(RHDLPG(rh_pgmap), NO)->bflags); // force map flush
	_applogpg(pgnum);
	return (pgnum);
}

#ifdef tetsonly
void __stdcall Xecho(const char *fmt, ...);
static void list_free(void)
{
	Xecho("freespace:%d\n", Bugbug);
	if (!zz)
		zz = &_mapmap->pgmaps[0].tot_free;

	int pm;
	for (pm = 0; pm < _mapmap->nmaps; pm++)
	{
		Xecho("PM%3d: %d\n", pm, _mapmap->pgmaps[pm].tot_free);

		PGMAP *pgm = (PGMAP *)_recadr00(_mapmap->pgmaps[pm].rh_pgmap);
		for (int i = 0; i <= pgm->lst_pg; i++)
		{
			ushort sp = pgm->map_free_sp[i];
			Xecho("   pg:%-5d %5u:%5u\n", i, (sp & SIZPART), sp);
		}
	}
	Xecho("\n");
}
#endif

static int dbmfndsp(int siz)
{
	int pgnum, nmap = _mapmap->nmaps - 1;
	siz = dbcmptcellsiz(siz) + sizeof(short);	  // add 2 bytes in case have to create a new portion of the cell ptr Vector
	if ((pgnum = _pgmscnpg(siz, nmap)) == NOTFND) // Not enough space in current map?
	{
		while (--nmap >= 0)								// Check any prior page maps that have at least 200K free in total
			if (_mapmap->pgmaps[nmap].tot_free > _200K) // Does it have a page with space for 'siz' bytes?
				if ((pgnum = _pgmscnpg(siz, nmap)) != NOTFND)
					return (pgnum);
		if ((pgnum = _pgmapppg()) == NOTFND) // No space in current or porior pages, so try to add a new pagemap
			throw SE_DBOVRFLOW;
	}
	return (pgnum);
}

static void dbpcmpct(char *pg, CELLTBL *celltbl)
{
	int cell_siz;
	short prv_sz, fre, scn, vsz, i, cur_siz, data_siz, vi,
		*V, *pdatasiz, *vector = DBPVECTOR(pg);
	for (i = celltbl->lfc; i; i = scn)
	{
#pragma warning(push, 3)
		scn = (*(vector - i)) & (~IN_LFC);
#pragma warning(pop)
		*(vector - i) = UNUSED_VECTOR;
	}
	vsz = celltbl->vector_siz;
	for (i = 1; i <= vsz; i++) /* for each Vector elm */
		if (((ushort)(vi = *(vector - i)) != UNUSED_VECTOR) && ((ushort)(vi) != RSVDID))
		{													  /* ptr to a free cell && rsvd id, but no data*/
			*(vector - i) = *(pdatasiz = (short *)(pg + vi)); // put data size in Vector
#pragma warning(push, 3)
			*pdatasiz = (i | CELL_CONTAINS_DATA); // & Vector # in cell
#pragma warning(pop)
		}
	prv_sz = celltbl->free_off; /* first byte of free heap*/
	fre = scn = 0;				/* size of new free heap*/
	while (scn < prv_sz)		/* this loop compresses the page */
		if ((cur_siz = *((short *)(&pg[scn]))) & CELL_CONTAINS_DATA)
		{ // if not free space, move it
			data_siz = *(V = vector - (cur_siz & ~CELL_CONTAINS_DATA));
			cell_siz = dbcmptcellsiz(data_siz);
			if (scn != fre)
				memmove(&pg[fre], &pg[scn], cell_siz);
			*V = fre; /* update cell Vector elm*/
			*((short *)(pg + fre)) = data_siz;
#pragma warning(push, 3)
			scn += cell_siz;
			fre += cell_siz;
		}
		else
			scn += dbcmptcellsiz(cur_siz); // skip over fragmented free space
#pragma warning(pop)

	if (celltbl->free_sp != PGSIZ - FOOTERSIZ - vsz * sizeof(short) - (celltbl->free_off = fre))
		throw SE_DBPGBAD;
	for (celltbl->lfc = i = 0; ++i <= vsz;) // rebuild list of free cells (lfc)
		if ((ushort)(*(vector - i)) == UNUSED_VECTOR)
		{
#pragma warning(push, 3)
			*(vector - i) = (celltbl->lfc | IN_LFC);
#pragma warning(pop)
			celltbl->lfc = i;
		}
}

static short *dbpmaksp(char *pg, int cell_siz)
{
	CELLTBL *celltbl = DBPCELLTBL(pg);
	char *pfree;
	if ((pfree = pg + celltbl->free_off) >= // ptr -> free space in cell
		(((char *)DBPVECTOR(pg)) - (celltbl->vector_siz * sizeof(short)) - cell_siz))
	{						   // If free space is fragemented, compact it
		dbpcmpct(pg, celltbl); // (alters the value of <free_off>)
		pfree = pg + celltbl->free_off;
	}
	return ((short *)pfree);
}

static short dbpnewid(char *pg)
{
	short cell_id, *vector = DBPVECTOR(pg);
	CELLTBL *celltbl = DBPCELLTBL(pg);
	if (celltbl->lfc)
#pragma warning(push, 3)
		celltbl->lfc = (*(vector - (cell_id = celltbl->lfc))) & (~IN_LFC);
#pragma warning(pop)
	else
	{ // no free cell list - extend Vector
		dbpmaksp(pg, sizeof(short));
		celltbl->free_sp -= sizeof(short); /*  gobble another short cell */
		cell_id = ++celltbl->vector_siz;
	}
	*(vector - cell_id) = RSVDID;
	celltbl->ncells++; /* add newly stored to cell short */
	return (cell_id);
}

static short _pgmmapnum(short pgnum, short *ppg_in_map)
{
	short pm_num;
	for (pm_num = 0; pgnum >= DBMPGSPMAP; pm_num++)
		pgnum -= DBMPGSPMAP;
	*ppg_in_map = pgnum;
	return (pm_num);
}

static void dbmupd(PGVARS *pv)
{
	ushort free_sp = ((CELLTBL *)(pv->celltbl))->free_sp;
	short old_sp, pg_in_map, pm_num = _pgmmapnum(pv->buf->pg_in_buf, &pg_in_map);
	PGMAP *pm = (PGMAP *)_recadr1d(_mapmap->pgmaps[pm_num].rh_pgmap);
#pragma warning(push, 3)
	old_sp = pm->map_free_sp[pg_in_map] & SIZPART;
#pragma warning(pop)
	pm->map_free_sp[pg_in_map] = free_sp;
	_mapmap->pgmaps[pm_num].tot_free += (free_sp - old_sp);
}

static short *dbpsnarfsp(PGVARS *pv)
{
	CELLTBL *celltbl = pv->celltbl;
	int cell_siz = dbcmptcellsiz(pv->data_siz); /* 2 extra bytes to store len */
	if (cell_siz > celltbl->free_sp)
		throw SE_RECTOOBIG;
	pv->pcell = dbpmaksp(pv->pg, cell_siz);
#pragma warning(push, 3)
	celltbl->free_sp -= cell_siz;		   // snarf cell_siz
	*(pv->vector_elm) = celltbl->free_off; //cell offset from beg of page
	celltbl->free_off += cell_siz;		   // advance free ptr over used space
#pragma warning(pop)
	*pv->pcell = pv->data_siz; // store SIZE OF DATA
	pv->cell_siz = (short)cell_siz;
	set_dirty(&pv->buf->bflags); // make sure map is written out
	dbmupd(pv);
	return (pv->pcell);
}

static int dbpistcell(BUF *buf, int data_siz)
{
	PGVARS pv;
	pv.data_siz = (short)data_siz;
	dbpsetvars(&pv, buf, dbpnewid(buf->addr));
	dbpsnarfsp(&pv);
	return (pv.cell_id); /* pcell=ptr to where data should go on page */
}

static RHDL _recist(int siz)	// Find a page with enough space
{								// for this record, Load to mem,
	int pg_num = dbmfndsp(siz); // & return RHDL to the record
	return (RHDLIST(pg_num, dbpistcell(bfsrch(pg_num, YES), siz)));
}

static short dbplockpage(RHDL rh_rec)
{
	PGVARS pv;
	short prv;
	dbpsetvars_r(&pv, rh_rec);
	if (pv.rh_far_cell)
		return (dbplockpage(pv.rh_far_cell));
#pragma warning(push, 3)
	prv = pv.buf->bflags & LOCKED;
#pragma warning(pop)
	pv.buf->bflags |= LOCKED;
	return (prv);
}

static void dbpfreepage(RHDL rh_rec, int lck)
{
	PGVARS pv;
	dbpsetvars_r(&pv, rh_rec);
	if (pv.rh_far_cell)
		dbpfreepage(pv.rh_far_cell, lck);
	else
		SETLOCK(pv.buf->bflags, lck);
}

static RHDL reccpy(RHDL rh) // rh = Original record
{
	short siz;
	void *ad = _recadr0(rh, &siz);
	memmove(_recadr1d(rh = _recist(siz)), ad, siz); // rh = New Copy record
	return (rh);
}

static RHDL _recadd(const void *data, short siz)
{
	RHDL rh_rec;
	if (siz > MAXRECSIZ)
		siz = MAXRECSIZ;
	else if (siz < 0)
		siz = 0;
	dbpmrkpgdirty(rh_rec = _recist(siz));
	memmove(_recadr00(rh_rec), data, siz); /* copy new data to buf addr */
	return (rh_rec);
}

RHDL recadd(HDL h_db, const void *data, short siz)
{
	dbactv(h_db);
	return (_recadd(data, siz));
}

static void dbprlssp(PGVARS *pv)
{
	short prv, *flg = &pv->buf->bflags;
#pragma warning(push, 3)
	pv->celltbl->free_sp += pv->cell_siz;
	*pv->vector_elm = RSVDID; // mark as taken, but no data
	prv = *flg & LOCKED;	  // make sure map is written out
#pragma warning(pop)
	*flg |= (LOCKED | DIRTY_PG); //jjjJ
	dbmupd(pv);
	SETLOCK(*flg, prv);
}

static void dbprlscell(BUF *buf, short cell_id)
{
	PGVARS pv;
	dbpsetvars(&pv, buf, cell_id);
	dbprlssp(&pv);
	pv.celltbl->ncells--;
#pragma warning(push, 3)
	*pv.vector_elm = pv.celltbl->lfc | IN_LFC; // add Vector to free cell list
#pragma warning(pop)
	pv.celltbl->lfc = cell_id;
	if (pv.rh_far_cell)
		_recdel(pv.rh_far_cell); // if a far cell, destroy it too
}

static void _recdel(RHDL rhdl)
{
	dbprlscell(bfsrch(RHDLPG(rhdl), YES), RHDLCELL(rhdl));
}

static short dbmfreesp(short pgnum)
{
	short pg_in_map, pm_num = _pgmmapnum(pgnum, &pg_in_map);
	PGMAP *pm = (PGMAP *)_recadr00(_mapmap->pgmaps[pm_num].rh_pgmap);
	return ((short)(pm->map_free_sp[pg_in_map] & SIZPART));
}

static RHDL dbpupdcell(RHDL rh_rec, int upd_siz)
{ /* returns rh_rec to cell to be updated */
	PGVARS pv;
	short *pcell, prv;
	int new_siz = dbcmptcellsiz(upd_siz);
	dbpsetvars_r(&pv, rh_rec);
#pragma warning(push, 3)
	prv = pv.buf->bflags & LOCKED;
#pragma warning(pop)
	pv.buf->bflags |= LOCKED;
	if (pv.rh_far_cell)
		_recdel(pv.rh_far_cell); // (delete far cell if used)
	*pv.pcell &= ~FARCELL;		 // turn off far cell bit
	if (upd_siz != pv.data_siz)	 // if cannot overwrite in place
	{
		dbprlssp(&pv);							  // release cur cell
		if (dbmfreesp(RHDLPG(rh_rec)) >= new_siz) // if can fit in base pg
		{
			pv.data_siz = (short)upd_siz;
			dbpsnarfsp(&pv);
		}
		else // updated cell doesn't fit in this pg
		{	 // have to make far_cell and ptr
			pv.data_siz = sizeof(RHDL);
			pcell = dbpsnarfsp(&pv);
			*((RHDL *)(pcell + 1)) = rh_rec = _recist(upd_siz);
			*pcell |= FARCELL;
			set_dirty(&pv.buf->bflags); // make sure map is written out
		}
	} // advance cell ptr past stored size
	SETLOCK(pv.buf->bflags, prv);
	return (rh_rec);
}

/* return YES/NO on whether RH_REC is a handle to an existing record in database 'h_db'.
	This is done by trying to read the record via RH_REC.
	if the handle is not valid, the database functions throw an error that we catch here */
static int dbisrhdl(HDL h_db, RHDL rh_rec)
{
	int er = 0;
	try
	{
		dbactv(h_db);
		_recadr00(rh_rec);
	}
	catch (int e)
	{
		er = e;
	}
	return (!er);
}

void recdel(HDL h_db, RHDL rhdl)
{
	dbactv(h_db);
	if (IS_XREC(rhdl))
		xrecrlsbuckets(rhdl);
if (rhdl)							// Added 13/04/23
	_recdel(rhdl);
}

static void _recupd(RHDL rhdl, const void *data, int siz)
{
	if (rhdl)
	{
		if (siz < 0)
			siz = 0;
		else if (siz > MAXRECSIZ)
			siz = MAXRECSIZ;
		memmove(_recadr1d(dbpupdcell(rhdl, siz)), data, siz);
	} // (set dirtyflag - rhdl might have changed from normal to far cell)
}

void recupd(HDL h_db, RHDL rhdl, const void *data, int siz)
{
	dbactv(h_db);
	_recupd(rhdl, data, siz);
}

int recget(HDL h_db, RHDL rhdl, void *data, int maxsiz)	// Returns NUMBYTES read
{
	dbactv(h_db);
	short siz;
	void *p = _recadr0(rhdl, &siz);
	if (maxsiz==0) maxsiz=siz;
	if (siz > maxsiz)
		siz = (short)maxsiz;
	if (siz <= 0)
		siz = 0;
	else
		memmove(data, p, siz);
	return (siz);
}

#ifdef inf_fixed
#define cmpdocmp(p1, p2) ((_h_btr->cmppart)((p1), (p2)))
#else
static int cmpdocmp(char *a, char *b)
{
	if (_h_btr->keytyp == DT_USR)
	{
		int i = _btr_ks0, ia = 1, ib = 1;
		while (i-- && (ia || ib))
		{
			if (((unsigned char*)a)[i] != 0xFF)
				ia = 0;
			if (((unsigned char*)b)[i] != 0xFF)
				ib = 0;
		}
		if (ia || ib)
			return (ia - ib);
	}
	return ((_h_btr->cmppart)(a, b));
}
#endif

static int cmpbinsrch(int n, char *elm, char *ky, int *ppos)
{
	int delta, idx, rc;
#pragma warning(push, 3)
	delta = idx = DIV2(n);
	do
	{
		if (idx == n)
		{
			rc = GT;
			break;
		}
		if ((rc = cmpdocmp(elm + idx * _btr_ksz4, ky)) == EQ)
			break;
		if (rc < 0)
			idx += DIV2(delta + 1);
		else
			idx -= DIV2(delta + 1);
		if (!delta)
			break; /* going to check same elm as last time thru */
		delta = DIV2(delta);
	} while (true);
	if (rc < 0 && idx < n)
		idx++;
#pragma warning(pop)
	*ppos = idx + 1; // the keys count from 1 to n, not 0 to n-1
	return (!rc);	 // YES if key found
}

/*    NOTE : functions calling these functions MUST mark the
      tree node as dirty or changes will not be updated in the database. */
static void bndmv(BNODE *b, int nkeys, int to, int from)
{
	memmove(&b->keys[(to - 1) * _btr_ksz4], &b->keys[(from - 1) * _btr_ksz4], nkeys * _btr_ksz4);
}

static void bndmvrtkeys(BNODE *b, int from, int pos_to_shift)
{
	int mv;
	if ((mv = b->bnkey_ct - from + 1) > 0)
		bndmv(b, mv, from + pos_to_shift, from);
#pragma warning(push, 3)
	b->bnkey_ct += pos_to_shift;
#pragma warning(pop)
}

static int bndmvlfkeys(BNODE *b, int from, int delct)
{
	int mv;
	if ((mv = b->bnkey_ct - from + 1) > 0)
		bndmv(b, mv, from - delct, from);
	if (delct <= b->bnkey_ct)
#pragma warning(push, 3)
		b->bnkey_ct -= delct;
#pragma warning(pop)
	return (b->bnkey_ct);
}

static void bndins(BNODE *b, int nkeys, int at, BNODE *bf, int from)
{
	memmove(&b->keys[(at - 1) * _btr_ksz4], &bf->keys[(from - 1) * _btr_ksz4], nkeys * _btr_ksz4);
}

// Starting with the node as passed, find 'key' within it.
// If not present, return last node searched
static RHDL bndscan(void *key, RHDL rh_node, int *got, int *ppos, BNODE **bn)
{
	do
	{ // if n/f, & > highest in node, & this isn't rightmost (highest) node
		BNODE *b = (BNODE *)_recadr00(rh_node);
		int ct = b->bnkey_ct - !b->rh_rt_sib;
		*got = cmpbinsrch(ct, b->keys, (char *)key, ppos);
		if (*got || *ppos <= ct || !b->rh_rt_sib)
		{
			*bn = b;
			return (rh_node);
		}
		rh_node = b->rh_rt_sib;
#pragma warning(push, 3)
	} while (true);
#pragma warning(pop)
}

// Create a new empty root node for current btree, and put the highest
// possible key value in it. This key won't be found by Bkysrch(), etc.,
// but it has to give EQ or GT when compared to any actual key value.
// The hi-value is all 0xFF, UNLESS keytyp is a known binary signed type,
// in which case the rightmost byte has sign-bit turned OFF.
// WATCH OUT! - if keytyp is DT_USr, it won't work if 'all 0xFF' is less
// than the highest possible value for the key.

static RHDL bndist(int key_typ)
{
	RHDL rh_node;
	BNODE *b;
	memset(b = (BNODE *)_recadr1d(rh_node = _recist(MAXRECSIZ)), 0, MAXRECSIZ);
	b->node_typ = LEAF;							   // make new control record
	memset(b->keys, 255, _btr_ks0);				   // Create an 'NOSUCHBUF key'
	if (key_typ == DT_LONG || key_typ == DT_SHORT) // If key is 'signed binary',
		b->keys[_btr_ks0 - 1] = 0x7F;			   // make highest possible +ve
	BNDRHDL(b, b->bnkey_ct = 1) = 0;
	return (rh_node);
}

// We've got the RHDL of a specific btr 'leaf' - read the leaf record
// and copy the current key value (+associated RHDL) to actv_leaf
static void bkykeyset(int kpos)
{
	BNODE *b = (BNODE *)_recadr00(_h_btr->actv_leaf);
	if (kpos > b->bnkey_ct)
		kpos = b->bnkey_ct;
	if (kpos > 0)
		memmove(_h_btr->actv_key, &b->keys[(kpos - 1) * _btr_ksz4], _btr_ksz4);
}

/* Traverse the tree from root to leaf. Save the branch traversed on
	the btree stack.  The leaf contains the sought key or, if the key is
	not in the tree, the key which is next higher than the sought key.
	Returns YES if the sought key exists, otherwise NO  */
static int travsbranch(void)
{
int kpos, got;
RHDL rh_lvl = _h_btr->rh_root;
BNODE *b;		   // Scan the cur lvl for the rec containing node that
_h_btr->depth = 0; // leads to leaf containing key. Until hit leaf node
if (!rh_lvl)
	throw SE_BADBROOT;
do
	{
	rh_lvl = bndscan(_h_btr->actv_key, rh_lvl, &got, &kpos, &b);
	BTRPSHNODE(rh_lvl);						// and get address of node on nxt level
	if (b->node_typ == LEAF) break; 		// down to a leaf - Quit search
	rh_lvl = BNDRHDL(b, kpos);
#pragma warning(push, 3)
	} while (true); 							// Make h_bnode=ptr to leaf holding bkey
#pragma warning(pop)
_h_btr->actv_leaf = rh_lvl;
*_btr_flg &= ~(BK_EXST | BK_PRCD); 		// posit key doesn't exst + not processed
if (kpos >= b->bnkey_ct && !b->rh_rt_sib)
	{
	kpos = b->bnkey_ct;
	*_btr_flg |= BK_PRCD;
	got = NO;
	}
if (got) *_btr_flg |= BK_EXST;
bkykeyset(kpos); 								// Updates actv_key (+RHDL after it)
return (got);
}

static void bndupd(int pos, void *key, RHDL child, RHDL parent)
{
BNODE *b = (BNODE *)_recadr1d(parent);
bndinskey(b, pos, key);
if (child) BNDRHDL(b, pos) = child;
}

// Use Bndscan() to locate position in parent to be overwritten with the new
// key value (Used to be the highest key in node, but deleted now).
// Complain if new key already there, or > parent's hi-key.
static void fixup_parent(RHDL parent, RHDL child, void *key)
{
	int got, kpos;
	BNODE *b;
	if (bndscan(key, parent, &got, &kpos, &b) != parent || got)
		throw SE_BADPARENT;
	if (cmpdocmp((char *)key, BNDKEY(b, kpos)) < 0)
		bndupd(kpos, key, child, parent);
}

static int _delkeyinnode(int dpth, int kpos, RHDL rh_descnd)
{
	RHDL rhdl = BTRSTKLVL(dpth); /* get next node from stack*/
	BNODE *b = (BNODE *)_recadr1d(rhdl);
	int nkeys = bndmvlfkeys(b, kpos + 1, 1);
	if (rh_descnd)
		BNDRHDL(b, kpos) = rh_descnd;
	if (--dpth <= 0) // If at root, root only has one child and the root
	{				 // is not a leaf, then make the child the new root
		if (nkeys == 1 && rh_descnd)
		{
			btrupdrt(BNDRHDL(b, 1));
			_recdel(rhdl);
		} // Release old root
		return (NO);
	}
	int f_uflow = (b->bnkey_ct < _btr_kpn / 2);
	if (kpos > nkeys)
		fixup_parent(BTRSTKLVL(dpth), rhdl, BNDKEY(b, kpos - 1));
	return (f_uflow);
}
/* The new active key is the one moving to the deleted slot. If that's in */
/* the next node, mark key as gotten. Next scan call will move to next node */

static void bndbalance(BNODE *lf, BNODE *rt)
{
	int lct = lf->bnkey_ct, rct = rt->bnkey_ct, half = (lct + rct) / 2, mv_ct;
	if (lct < rct) /* move to emptier node */
	{
		bndins(lf, mv_ct = half - lct, lct + 1, rt, 1);
#pragma warning(push, 3)
		lf->bnkey_ct += mv_ct;
#pragma warning(pop)
		bndmvlfkeys(rt, mv_ct + 1, mv_ct);
	}
	else
	{											// update left node count
		bndmvrtkeys(rt, 1, mv_ct = half - rct); // + shift keys to right node
#pragma warning(push, 3)
		bndins(rt, mv_ct, 1, lf, (lf->bnkey_ct -= mv_ct) + 1);
#pragma warning(pop)
	}
}

static short getsibling(RHDL parent, RHDL look, int *kpos, RHDL *got)
{
	short i;
	BNODE *b = (BNODE *)_recadr00(parent);
	if (BNDRHDL(b, 1) == look)
	{
		*kpos = 1;
		*got = BNDRHDL(b, 2);
		return (NO);
	}								  // No leaf
	for (i = 1; i < b->bnkey_ct; i++) // find the leaf
		if (BNDRHDL(b, i + 1) == look)
		{
			*kpos = i;
			*got = BNDRHDL(b, i);
			return (YES);
		}
	throw SE_BADDELKEY;
}

static RHDL bnduflow(int dpth)
{
	RHDL parent, lf_sib, rt_sib, uflow = BTRSTKLVL(dpth);
	int kpos;
	short *flgl, *flgr, fpl, fpr;
	BNODE *rt, *lf;
	dpth--;
	if (getsibling(parent = BTRSTKLVL(dpth), uflow, &kpos, &lf_sib))
		rt_sib = uflow;
	else
	{
		rt_sib = lf_sib;
		lf_sib = uflow;
	}
	rt = (BNODE *)_recadr1(rt_sib, &flgr);
	fpr = *flgr;
	*flgr |= LOCKED; // don't let rt swap out while getting lf!
	lf = (BNODE *)_recadr1(lf_sib, &flgl);
	*flgr = fpr;
	set_dirty(flgl); // lf_sib gets updated whichever way we go...
	if (rt->bnkey_ct + lf->bnkey_ct < _btr_kpn)
	{
		bndins(lf, rt->bnkey_ct, lf->bnkey_ct + 1, rt, 1);
#pragma warning(push, 3)
		lf->bnkey_ct += rt->bnkey_ct;
#pragma warning(pop)
		if ((uflow = lf->rh_rt_sib = rt->rh_rt_sib) != NULLRHDL)
		{
			((BNODE *)_recadr1(uflow, &flgr))->rh_lf_sib = lf_sib;
			set_dirty(flgr);
		}
		_recdel(rt_sib); // was merged in to left node
		if (_delkeyinnode(dpth, kpos, lf_sib))
			bnduflow(dpth);
	}
	else
	{
		bndbalance(lf, rt); // Spread the keys equally between siblings
		set_dirty(flgr);
		fpl = *flgl;
		*flgl |= LOCKED; // Don't let lf get swapped by parent...
		bndupd(kpos, &lf->keys[(lf->bnkey_ct - 1) * _btr_ksz4], 0, parent);
		*flgl = fpl;
	}
	return (lf_sib);
}

//	Split current node in to left and right nodes. Current node becomes the right node and its copy becomes the left node.
//	lf_rec		Record containing node to be split, this record will contain the left node after the split.
//	p_lst_key	(Output) highest key in left node of the pair which is created.
//	return		The stored record address of the right node
static RHDL split_node(RHDL rh_lf_node, void *p_lst_key)
{
	BNODE *lf, *rt;
	RHDL rh_rt_node;
	short *flgl, *flgr, prv_flgl, prv_flgr;
	int half;
	lf = (BNODE *)_recadr1(rh_lf_node, &flgl);
	prv_flgl = *flgl;
	*flgl |= LOCKED;				 // lock old (LF) node in memory
	rh_rt_node = reccpy(rh_lf_node); // Copy old (LF) node as (RT) & lock
	rt = (BNODE *)_recadr1(rh_rt_node, &flgr);
	prv_flgr = *flgr;
	*flgr |= LOCKED;
	rt->rh_lf_sib = rh_lf_node; // link old & new nodes as siblings
	lf->rh_rt_sib = rh_rt_node;
#pragma warning(push, 3)
	half = lf->bnkey_ct = DIV2(rt->bnkey_ct) + 1; // keep how many keys each side?
#pragma warning(pop)
	/* Get rid of (lower) keys in RT that will be kept in LF now */
	/* i.e. - shift left, and reduce RT keyct by 'half' */
	bndmvlfkeys(rt, half + 1, half);
	/* return THE VALUE OF THE HIGHEST KEY IN THE DIVIDED LEFT SECTOR */
	memmove(p_lst_key, BNDKEY(lf, lf->bnkey_ct), _btr_ksz4);
	// If there's another node to right of the new pair, change its
	// left_sib ptr to the new right node, not the original (left) one
	if (rt->rh_rt_sib)
		((BNODE *)_recadr1d(rt->rh_rt_sib))->rh_lf_sib = rh_rt_node;
	*flgl = (short)(prv_flgl | DIRTY_PG);
	*flgr = (short)(prv_flgr | DIRTY_PG);
	return (rh_rt_node);
}

static int bndrepos(void)
{
	int got, kpos;
	BNODE *b;
	_h_btr->actv_leaf = bndscan(_h_btr->actv_key, _h_btr->actv_leaf, &got, &kpos, &b);
	if (!b->rh_rt_sib && kpos >= b->bnkey_ct) // v-- can never find NOSUCHBUF key
	{
		kpos = b->bnkey_ct;
		got = NO;
	}
	if (!got)
	{
		bkykeyset(kpos);
		*_btr_flg &= ~BK_PRCD;
	}
	return (kpos);
}

static void _bkydel(void)
{
	if (_delkeyinnode(_h_btr->depth, bndrepos(), 0))
	{
		_h_btr->actv_leaf = bnduflow(_h_btr->depth);
		bndrepos();
	}
	*_btr_flg &= ~(BK_EXST | BK_PRCD); // key doesn't exst + not processed
	_h_btr->keyct--;
}

// Bkyfnd can be called with rhdl and/or ky as NULLPTR
static int bkyfnd(void *ky)
{
	if (ky)
		memmove(_h_btr->actv_key, ky, _btr_ks0);
	return (travsbranch());
}

/* param :   rec - record containing node in to which key is to be added
*     insrt_pos - position at which key will be added
*     key - ptr to the key being added
*     rh_child - RHDL   of the child associated with this key
* synopsis  shift all keys to right to make room for new key
*     compute offset position of new key
*     move new key in to node
*     if  (this node is a leaf)
*         the child goes with the new key
*         the child is the new active record for the tree
*     else the child goes with the next key since
*           this key was the highest key in the left
*           node and the child pnts to the right node
*     set node as altered */
static void add_bkey_to_node(BNODE *b, RHDL rh_bnode, int pos, void *key, RHDL child)
{
	bndmvrtkeys(b, pos, 1); /* make room for the new key */
	bndinskey(b, pos, key);
	if (b->node_typ == LEAF) /* make it the active key */
	{
		_h_btr->actv_leaf = rh_bnode;
		bkykeyset(pos);
		*_btr_flg |= (BK_EXST | BK_PRCD);
	}
	else
		pos++;
	BNDRHDL(b, pos) = child;
	dbpmrkpgdirty(rh_bnode);
}

// Return the address of the root, being the active node in
// which to insert the key which caused the root to split.
static RHDL bld_new_root(void)
{
	RHDL rh_old_root = _h_btr->rh_root, rh_new_root = bndist(_h_btr->keytyp);
	BNODE *root = (BNODE *)_recadr1d(rh_new_root);
	root->node_typ = BRANCH;
	BNDRHDL(root, 1) = rh_old_root;
	btrupdrt(rh_new_root);
	_dbckpt();
	return (rh_new_root);
}

/* BKYINS(void *key, RHDL rh_child)
*        key - ptr to key, rh_child - HDL to child record to be inserted
*
* synopsis  Bkyfnd must be called before this to
*     search the btree for the key.  while doing so
*     it locates the branch on which this key must lie.
*     it then pushes the nodes on this branch onto the
*     btree stack.  We then use the stack to determine
*     which nodes to insert the current key.  first, the node
*     for the leaf of the btree is popped and the new key is
*     inserted.  however, this may cause the leaf to split.
*     in that case, it is necessary to insert the middle key
*     for the split in the leaf's parent. the parent may split
*     etc and so on up to the root.  the root itself may split
*     causing the btree to grow by a level. To insert keys
*     in a nodes parent, this fn is called recursively until
*     the newly inserted key does not split its node or the
*     root is reached.  if the latter happens, then a new root
*     is built and the insertion is complete.
*
*     pop the next node off the stack
*     if  (node is NULL) we are at the root, so build new root
*     search this level for the key
*     if  (the key was found) insert the child at this key
*     else search gets ptr to first key > sought key
*         if (node is full) split the node
*           if (sought key is in new right node)
*           then find position in right node
*           else already have position in left node
*           add the key to the node
*           insert the middle key in this node's parent
*         else add key to partially filled node */

static void bkyins(void *key, RHDL rh_child)
{
	RHDL rh_bnode, rh_rt_node;
	BNODE *b;
	int got, insrt_pos; /* get next node from stack */
	if ((rh_bnode = BTRPOPNODE) == 0)
		rh_bnode = bld_new_root(); /* at root */
	rh_bnode = bndscan(key, rh_bnode, &got, &insrt_pos, &b);
	if (got) /* if found, then insert child */
	{
		_recadr1d(rh_bnode);
		BNDRHDL(b, insrt_pos) = rh_child;
	}
	else
	{								 /*ptr to stored key is greater than new bkey*/
		if (b->bnkey_ct >= _btr_kpn) /*node filled*/
		{
			char *lfhky; // dummy space for 'last key in lf node'
			rh_rt_node = split_node(rh_bnode, lfhky = (char *)memgive(_btr_ksz4));
			/* if key went in to new right node then we have to search new right */
			/* node and reset 'insrt_pos' originally set for (old) left node */
			if (cmpdocmp((char *)key, lfhky) > 0) // add middle ky to parent
				rh_bnode = bndscan(key, rh_rt_node, &got, &insrt_pos, &b);
			add_bkey_to_node(b, rh_bnode, insrt_pos, key, rh_child);
			bkyins(lfhky, rh_rt_node); /*RECURSIVE*/
			memtake(lfhky);
		}
		else
			add_bkey_to_node(b, rh_bnode, insrt_pos, key, rh_child);
	}
}

// If 'key' is already present, we wouldn't want to overwrite the
// one passed to Bkyadd(), in case there's XTN'd data after key
// As of 14/12/98, it's still up to the calling routine to make sure
// the key doesn't already exist to avoid this. If this is a problem
// then we could 'memadup()' the key here before calling 'Bkyfnd()'.
int bkyadd(HDL h_btr, RHDL rhdl, void *key)
{
	btrbind(h_btr);
	if (bkyfnd(key)) // Then the key already exists (can't add)
	{
		*_btr_flg |= BK_PRCD;
		return (NO);
	}
	bkyins(key, rhdl);
	_h_btr->keyct++;
	return (YES);
}

// Bkyupd is really a special for updating keys with XTNd'ed data after the
// key proper, but can also be used to just alter the key's associated rhdl
// As with Bkyadd(), we need to call Bkyfnd() first WITHOUT updating 'key',
// then really do the update with the data at 'key' (which had better be EQ
// to whatever we located via Bkyfnd(), or we'll be in trouble!)
// Feb 2024 - I'm SURE "rhdl" can be actual value, not pointer
int bkyupd(HDL h_btr, RHDL rhdl, void *key)
{
	btrbind(h_btr);
	if (bkyfnd(key))
	{
		bndupd(bndrepos(), key, rhdl, _h_btr->actv_leaf);
		return (YES);
	}
	return (NO);
}

// 'rhdl' can be passed as NULLPTR, but normally is a ptr, 'cos there's
// usually an associated record, and we need the rhdl to be filled as we
// delete the key, so we know the associated record to delete.
// 'key' is ALWAYS passed, 'cos we've obviously got to locate this key in
// the tree before deleting it. We should copy the actual ky as found, in
// case it's an XTNd'ed key (with data after the part used for matching),
// otherwise we don't know what we deleted. (I think it doesn't do that yet)
// IT DOES NOW!! 10/6/98 - calls btrakeyret() if OK
bool bkydel(HDL h_btr, RHDL *rhdl, void *key)
{
btrbind(h_btr);
if (bkyfnd(key))
	{
	btrakeyret(rhdl, key);
	_bkydel();
	return(true);
	}
return(false);
}

bool bkydel_rec(HDL btr, void *key)
{
RHDL rh;
bool ok=bkydel(btr, &rh, key);
if (ok && dbisrhdl(_h_db, rh))
	recdel(_h_db, rh);
return(ok);
}

/* probably this is the place where things clag if Buffers is too low!!! */
/* looks as if the Buffer Btrpshnode()'ed ought to be LOCKED temporarily */
static void bkysetpos(int last)
{
	RHDL rh_node = _h_btr->rh_root;
	BNODE *b;
	_h_btr->depth = 0;
	do
	{
		BTRPSHNODE(rh_node); /* save branch nodes on stack & get nxt level addr */
		b = (BNODE *)_recadr00(rh_node);
		if (b->node_typ == LEAF)
			break;
		rh_node = BNDRHDL(b, last ? b->bnkey_ct : 1);
#pragma warning(push, 3)
	} while (true);
#pragma warning(pop)
	_h_btr->actv_leaf = rh_node;
	bkykeyset(last ? b->bnkey_ct : 1);
	*_btr_flg &= ~(BK_EXST | BK_PRCD); // key doesn't exst + not processed
}

static int _bkysetnxthdl(void) /* RECURSIVE FUNCTION */
{
	int ok = YES, ct, kpos = bndrepos() + ((*_btr_flg & BK_PRCD) != 0);
	BNODE *b = (BNODE *)_recadr00(_h_btr->actv_leaf);
	if (kpos < 1)
		kpos = 1;
	if (kpos > (ct = b->bnkey_ct))
	{
		*_btr_flg &= ~BK_PRCD;
		if (b->rh_rt_sib) // ky not in cur leaf, & more leaves left? - goto next
		{
			_h_btr->actv_leaf = b->rh_rt_sib;
			bkykeyset(1);
			return (_bkysetnxthdl()); // RECURSE
		}
	}
	*_btr_flg |= (BK_EXST | BK_PRCD); // posit key exsts + has been processed
	if (kpos >= ct && !b->rh_rt_sib)
	{
		*_btr_flg &= ~BK_EXST;
		kpos = ct;
		ok = NO;
	}
	bkykeyset(kpos);
	return (ok);
}

static short bkynxtkey(RHDL *rhdl, void *key)
{
	if (!_h_btr->actv_leaf)
		throw SE_NXTKEY;
	if (_bkysetnxthdl())
	{
		btrakeyret(rhdl, key);
		return (YES);
	}
	return (NO);
}

static int _bkysetprvhdl(void) /* RECURSIVE call.   */
{							   // v--  decrement if exists & not processed
	int kpos = bndrepos() - (((*_btr_flg) & (BK_EXST | BK_PRCD)) != BK_EXST);
	BNODE *b = (BNODE *)_recadr00(_h_btr->actv_leaf);
	if (kpos < 1) /* Activkey n/f */
	{
		*_btr_flg &= ~BK_PRCD;
		if (b->rh_lf_sib) /* Activkey n/f, & more leaves ? */
		{
			bkykeyset(((BNODE *)_recadr00(_h_btr->actv_leaf = b->rh_lf_sib))->bnkey_ct);
			*_btr_flg |= BK_EXST;
			return (_bkysetprvhdl()); // (recursive)
		}
		*_btr_flg &= ~BK_EXST;
		bkykeyset(1);
		return (NO);
	}
	*_btr_flg |= (BK_EXST | BK_PRCD);
	bkykeyset(kpos);
	return (YES);
}

static short bkyprvkey(RHDL *rhdl, void *key)
{
	if (!_h_btr->actv_leaf)
		throw SE_PRVKEY;
	if (_bkysetprvhdl())
	{
		btrakeyret(rhdl, key);
		return (YES);
	}
	return (NO);
}

// On 1st call (*again==0), position at the start of the btree
// If there's a NXT key, read it and return YES, else return NO
// 15/10/03 - support again=NULLPTR to step forward from current position
// If called with again=NULLPTR immediately after after bkysrch(), returns the sought key (or 0 if search failed)
// (following calls with again=NULLPTR simply step forward within the btree)
int bkyscn_all(HDL btr, RHDL *rhdl, void *key, int *again)
{
	btrbind(btr);
	if (again)
		if (!*again)
		{
			bkysetpos(NO);
			*again = YES;
		}
	return (_h_btr->keyct && bkynxtkey(rhdl, key));
}

// We ALWAYS need 'key' to be passed here (I think)
int bkysrch(HDL h_btr, int mode, RHDL *rhdl, void *key)
{
	int ok;
	btrbind(h_btr);
	ok = bkyfnd(key);
	if (ok && ABS(mode) < 2)
		btrakeyret(rhdl, key);
	switch (mode)
	{
	case BK_LT:
		*_btr_flg |= BK_PRCD;
		ok = bkyprvkey(rhdl, key);
		break;
	case BK_LE:
		if (!ok)
			ok = bkyprvkey(rhdl, key);
		break;
	case BK_GE:
		if (!ok)
			ok = bkynxtkey(rhdl, key);
		break;
	case BK_GT:
		if (ok)
			*_btr_flg |= BK_PRCD;
		ok = bkynxtkey(rhdl, key);
		break;
	}
	if (ok)
		*_btr_flg &= ~BK_PRCD;
	return (ok);
}

RHDL btrist(HDL h_db, int key_typ, int key_siz)
{
	RHDL rh_root, rh_hdr = 0;
	BHDR bh;
	dbactv(h_db);
	_btr_ksz4 = (_btr_ks0 = key_siz) + 4;
	if ((rh_root = bndist(key_typ)) != 0)
	{
		memset(&bh, 0, sizeof(BHDR));
		bh.bhrec_typ = RT_BHDR;
		bh.keytyp = (short)key_typ;
		bh.keysiz = (short)key_siz;
		bhdrupd(rh_hdr = _recadd(&bh, sizeof(BHDR)), rh_root, 0L);
	}
	return (rh_hdr);
}

// Delete all keys in btree, and if del_rhdl==YES, delete all rhdl's as well
void bkydel_all(HDL btr, int del_rhdl)
{
	int again = false;
	RHDL rh;
	while (bkyscn_all(btr, del_rhdl ? &rh : 0, 0, &again))
		if (bkydel(btr, 0, _h_btr->actv_key))
			if (del_rhdl && dbisrhdl(_h_db, rh))
				recdel(_h_db, rh);
}

void btrrls(HDL h_btr)
{
	bkydel_all(h_btr, NO); // (this calls btrbind)
	RHDL rh_bhdr = _h_btr->rh_bhdr;
	RHDL rh_root = _h_btr->rh_root;
	btrclose(h_btr);
	_recdel(rh_bhdr);
	_recdel(rh_root);
}

unsigned short dbpgct(HDL _h_db)
{
	return (_H_DB->mapmap.pgs_in_db);
}

char *dbfnam(HDL db)
{
	if (!db)
		db = _h_db;
	return (((DB *)db)->db_fnam);
}

/*  you can set Buffer swapping off for batch processing.
    otherwise the database system tries to keep relatively
    clean Buffers.  this slows things down a bit and is
    satisfactory for transaction input by users.  but
    for batch, it significantly increases the run time. */

void dbckpt(HDL h_db)
{
	dbactv(h_db);
	_dbckpt();
	flsafe(_dbfil); // added 12/01/04 ############### shouldn't be needed if VB always calls dllshutdown
}

// Read in the 'MAPMAP' map of pagemaps for current database
static void dbmrd(void)
{
	int n;
	PGMAP *pm;
	_mapmap = &_H_DB->mapmap;
	_mapmap->nmaps = 1;			  // 29/4/03 I think this 'pre-initialisation' is redundant
	_mapmap->pgs_in_db = MAXPGCT; // 29/4/03 I think this 'pre-initialisation' is redundant
	PGMAPS *pms = _mapmap->pgmaps;
	pms->rh_pgmap = RHDLIST(1, 1); // 1st page map is always in page 1, cell 1
	for (n = MAXPGMAPS; n--;)	   // 29/4/03 I think this 'pre-initialisation' is redundant
		pms[n].tot_free = ((int32_t)(DBMPGSPMAP)) * MAXRECSIZ;
	for (RHDL rhm = _mapmap->pgmaps[_mapmap->nmaps = 0].rh_pgmap; rhm; rhm = pm->rh_nxt_pgmap) // for each map...
	{
		pms = &_mapmap->pgmaps[_mapmap->nmaps];
		pm = (PGMAP *)_recadr00(rhm);
		n = pm->lst_pg + 1; // number of pages in this map
		//if (n<1) break; //, sjh this needs looking at!
		if (_mapmap->nmaps++) //
		{
//#pragma warning(push, 3)
			_mapmap->pgs_in_db += n;
//#pragma warning(pop)
			pms->rh_pgmap = rhm;
		}
		else
			_mapmap->pgs_in_db = (short)n;
		while (n--)
			pms->tot_free += ((pm->map_free_sp[n] & SIZPART) - MAXRECSIZ);
	}
}


static bool dbstart(int nbufs)
{
//nbufs=12;
if (_bf_ct) return(NO);		// Nothing to do if db system already started
_dbt = 0;
bfspist(nbufs);
dbsafe_catch = DB_IS_DIRTY;
return(YES);
}


//#define READWRITE 2
#define FIL_DIRTY 8

// All databases have Db_IS_CLEAN at offset Db_DIRTYPOS ('xc' contains "(c) Softworks 1992")
// which would have been written at start of the file when it was created.
// Database safety ON (DbSafe_Catch==DB_IS_DIRTY), tells FlOpen() it's a DBF file,
// making flput() change this byte to DB_IS_DIRTY when FIRST called, and flclose()
// change it back to DB_IS_CLEAN when we close the file under program control.
// If this flag byte isn't DB_IS_CLEAN when we open the file, we're obviously in
// deep doo-doo's, but we'll keep a 'back-door' Alt/Fn9 in dbcorrupt().
static HDL dbopen(const char *dbase)
{
	char fullpath[FNAMSIZ], mode[4] = {'r', '+', 0, 0};
	if (_lock)
		mode[0] = 'R';
	mode[2] = (char)dbsafe_catch;
	_h_db = 0;
//	if (!_bf_addr) throw SE_NOTACTIVE;  // Oct21 - caller didn't invoke Dbstart for buffers!
	if (!_bf_addr) dbstart(32);  // Mar'23
	drfullpath(fullpath, dbase);
	_dbfil = flopen(fullpath, mode);
	if (_dbfil)
	{
		_h_db = (char *)memgive(sizeof(DB) + strlen(fullpath) +1);
		_H_DB->h_db_fl = _dbfil;

		if (_lock)
			_H_DB->db_flg |= F_RDONLY;
		strcpy(_H_DB->db_fnam, fullpath);
		_dbt = (char *)dlrlink((DLRING *)_dbt, (DLRING *)_h_db);
		dbmrd();
		char safe;
		if (!(dbsafe_catch & 128) && (flgetat(&safe, 1, DB_DIRTYPOS, _dbfil) != 1 || safe != DB_IS_CLEAN))
		{
			SetErrorText("Database %s is corrupt!", fullpath);
			/*if (1)
{
Pf_FIL *fil=(Pf_FIL*)_dbfil;
fil->flag|=FIL_DIRTY;
} else */
			;	//		throw SE_DBSAFE;
				//char w[1024];DllGetLastError(w);sjhloG("%s",w);
				//		Pf_FIL *fil=(Pf_FIL*)_dbfil;
				// We never actually get here - but if we did, this unsets the "corrupt" flag even if program didn't set DbSafe_Catch on.
				// if	(fil->mode==READWRITE)	// If file is R/W, trick DbClose() into writing DB_IS_CLEAN if closed safely,
				//	(fil->flag|=FIL_DIRTY;		// - even if program didn't set DbSafe_Catch on.
		}
	}
	return (_h_db);
}


HDL dbopen2(const char *fn)
{
int no_write=access(fn,W_OK);
int prvlock=dbsetlock(no_write?YES:NOTFND);
//sjhlog("NO_write=%d for %s",no_write,fn);
HDL db=dbopen(fn);
if (db==NULLHDL)
    m_finish("Error opening %s",fn);
dbsetlock(prvlock);
return(db);
}


static void dbstop(void);	// seem to have to pre-declare after AUTO start/stop database subsystem
// The code in FLOPEN.C only sets the FIL_DIRTY bit in Pf_FIL.flag if the
// database file was opened with mode="r+[", AND it's been updated.
// The application must set global variable DbSafe_Catch to DB_IS_DIRTY to turn
// this stuff on, otherwise it'll be a NULL, so nothing gets checked.
// If DbSafe_Catch is set, the application is responsible for making
// sure that DbSafe is set non-zero when it closes ANY database properly,
// which will cause the flagbyte at offset Db_DIRTYPOS to be reset to DB_IS_CLEAN.
// Note that once DbSafe is set, ALL databases accessed by the program
// get checked, including *.HLP and Archive files, so the modules that
// manage those files must set DbSafe before DbClose(), even though this
// won't have any effect when included in an application that didn't set
// DbSafe_Catch in the first place (because FIL_DIRTY won't be set).
static void dbclose(HDL db)
{
	dbactv(db);
	btrallclose();
	bffilflush(YES);
	if (dbsafe > 0)
		flsafe(_dbfil);
	flclose(_dbfil);
	_dbt = (char *)dlrsnap((DLRING *)_dbt, (DLRING *)db);
	if (((DB *)db)->db_flg & F_TEMP)
		drunlink(((DB *)db)->db_fnam);
	memtake(db);
	_dbfil = 0;
if (_dbt==0) dbstop();
}

static void dbcloseall(void)
{while ((_h_db = _dbt) != 0) dbclose(_h_db);}

static void dbstop(void)
{
dbcloseall();
bfsprls();
dbsafe_catch = 0;
}

void dbsafeclose(HDL db)
{
	dbsafe++;
	dbclose(db);
	dbsafe--;
}

int dbreadonly(HDL db)
{
	if (((DB *)db)->db_flg & F_RDONLY)
		return (YES);
	return (NO);
}

static void dbzap(HDL db, char *name)
{
if (name) strcpy(name, ((DB *)db)->db_fnam);
((DB *)db)->db_flg |= F_TEMP;
dbclose(db);
}

void db_set_temp(HDL db)
{((DB *)db)->db_flg |= F_TEMP;}

/*  put rh_rec of header in to file header, where we can always find it */
RHDL dbsetanchor(HDL h_db, RHDL rh_anchor)
{
	RHDL rh_rec = RHDLIST(0, 1); // Anchor record is always page 0, cell 1
	dbactv(h_db);
	DBFHDR *h = (DBFHDR *)_recadr1d(rh_rec);
	RHDL old_anchor = h->rh_anchor;
	h->rh_anchor = rh_anchor;
	return (old_anchor);
}

RHDL dbgetanchor(HDL h_db) /* Read page 0/record 1 record */
{						   /* return database anchor RHDL */
	dbactv(h_db);
	return (((DBFHDR *)_recadr00(RHDLIST(0, 1)))->rh_anchor);
}

// Initialise a database by writing the first 2 pages directly to file without page management
static void dbinitfil(void)
{
	char *pg = (char *)memgive(PGSIZ);
	PGMAP *pm;
	DBFHDR *h = (DBFHDR *)dbdummypg(pg, sizeof(DBFHDR));
	memset(h, 0, sizeof(DBFHDR));
	h->versn = 1; // DB format version number
	short fhdr_free = DBPCELLTBL(pg)->free_sp;
	flput(pg, PGSIZ, _dbfil);
	dbmist(pm = (PGMAP *)dbdummypg(pg, MAXRECSIZ));
	pm->map_free_sp[0] = fhdr_free;
	pm->map_free_sp[1] = 0;
	pm->lst_pg = 1;
	flput(pg, PGSIZ, _dbfil);
	memtake(pg);
}

int dbist(const char *dbfnam)
{
if (!_bf_addr) dbstart(32);  // Mar10, 2023

	if (_bf_addr && (_dbfil = flopen(dbfnam, "w+")) != 0)
	{
		char xc[] = "(c) SOFTWORKS 2000";
		dbsafe_catch |= 128; // Inhibit DbSafe trapping temporarily
		dbinitfil();
		flclose(_dbfil);
		dbopen(dbfnam);			 // open database (dbactv(), set _dbfil, etc...)
		_recadd(xc, sizeof(xc)); // was 'recadd(h_db,...)'
		dbclose(_h_db);
		dbsafe_catch &= 127; // Re-instate original value (0 or DB_IS_DIRTY)
		return (YES);
	}
	return (NO);
}

int dbsetlock(int on)
{
	int prv = _lock;
	if (on != NOTFND)
		_lock = on;
	return (prv);
}

#pragma pack(push, 1)
struct XREC
{
	int32_t totbytes;	   // total datasize of this xrec
	short nbuckets, nused; // tot/used buckets in this xrec
	struct
	{
		short nbytes; // NumBytes stored under this rh_rec
		RHDL rh_rec;  // HDL of data stored in this bucket
	} bucket[4];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct XRFCB
{
	short _unused_tag; // (unused)
	HDL h_db;
	uint32_t xrh_rec; /* 'x's needed because of CI-86 */
	uint32_t xtotbytes, tot_off;
	ushort xflags;
	short xnbuckets, xnused, bkt;
	struct
	{
		RHDL rh_bucket;
		short xnbytes, off;
	} xbucket;
};
#pragma pack(pop)

//static void rdbkt(RHDL rh_xrec, short bkt, RHDL *p_rhdl, short *psiz)
static void rdbkt(RHDL rh_xrec, short bkt, uint32_t *p_rhdl, short *psiz)
{
	XREC *x = (XREC *)_recadr00(rh_xrec);
	if ((bkt > 0) && (bkt-- <= x->nused))
	{
		*p_rhdl = x->bucket[bkt].rh_rec;
		*psiz = x->bucket[bkt].nbytes;
	}
}

static void xtnd(XRFCB *h, void *data, int siz)
{
	int32_t remain = h->xbucket.xnbytes - h->xbucket.off;
	int mov = 0;
	if (remain > 0)
	{
		mov = ushort(MIN(remain, siz));
		memmove(data, ((char *)_recadr00(h->xbucket.rh_bucket)) + h->xbucket.off, mov);
#pragma warning(push, 3)
		h->xbucket.off += mov;
#pragma warning(pop)
		remain -= mov;
		siz -= mov;
	}
	if (siz) /* need to go to next bucket */
	{
		h->bkt++;
		rdbkt(h->xrh_rec, h->bkt, &h->xbucket.rh_bucket, &h->xbucket.xnbytes);
		h->xbucket.off = 0;
		if (h->bkt <= h->xnused)
			xtnd(h, ((char *)data) + mov, siz); /* RECURSIVE CALL */
	}
}

static RHDL xrecist(HDL h_db)
{
	XREC xrec;
	memset(&xrec, 0, sizeof(XREC));
	xrec.nbuckets = 4;
	return (recadd(h_db, &xrec, sizeof(XREC)) | XR_BIT);
}

static void xrecrlsbuckets(RHDL rh_xrec)
{
	int i;
	short pv, *flg;
	XREC *x = (XREC *)_recadr1(rh_xrec, &flg);
	pv = *flg;
	*flg |= LOCKED;
	for (i = 0; i < x->nused; i++)
		if (x->bucket[i].nbytes > 0)
		{
			_recdel(x->bucket[i].rh_rec);
			x->bucket[i].rh_rec = x->bucket[i].nbytes = 0;
		}
	x->totbytes = x->nused = 0;
	*flg = (short)(pv | DIRTY_PG); //jjjJ
}

static HDL xrecopen(HDL h_db, RHDL rhdl, const char *mode)
{
	int m = TOLOWER(*mode);
	XRFCB *x;
	XREC *xr;
	if (!IS_XREC(rhdl) || (m != 'a' && m != 'r' && m != 'w'))
		return (0);
	x = (XRFCB *)memgive(sizeof(XRFCB));
	dbactv(x->h_db = h_db);
	x->xrh_rec = rhdl;
	if (m == 'w')
		xrecrlsbuckets(rhdl);
	xr = (XREC *)_recadr00(rhdl);
	x->xtotbytes = xr->totbytes;
	x->xnbuckets = xr->nbuckets;
	x->xnused = xr->nused;
	x->bkt = 1; /* this might change if we're appending to existing xrec data */
	switch (m)
	{
	case 'a': /* append   */
		x->xflags = XM_APPEND;
		if (x->xnused > 0)
			x->bkt = x->xnused;
		x->tot_off = x->xtotbytes;
		rdbkt(rhdl, x->bkt, &x->xbucket.rh_bucket, &x->xbucket.xnbytes);
		break;
	case 'r': /* get      */
		x->xflags = XM_READ;
		rdbkt(rhdl, 1, &x->xbucket.rh_bucket, &x->xbucket.xnbytes);
		break;
	default: /*   case 'w':   put      */
		x->xflags = XM_WRITE;
		break;
	}
	return ((HDL)x);
}

static int xrecsetbucket(RHDL rh_xrec, int bkt, RHDL rh_rec, int siz)
{
	short xrec_siz;
	int i, ok = NO;
	XREC *x = (XREC *)_recadr0(rh_xrec, &xrec_siz), *xx;
	if ((bkt > 0) && (bkt <= x->nused + 1))
	{
		if (bkt > x->nused)
		{
			if (bkt > x->nbuckets)
			{
				if (x->nbuckets >= MAXBUCKETS)
					goto OVFLW;
				i = xrec_siz + ADDBUCKETS * SIZBUCKET;
				_recupd(rh_xrec, memmove(xx = (XREC *)memgive(i), x, xrec_siz), i);
				memtake(xx);
				x = (XREC *)_recadr0(rh_xrec, &xrec_siz);
				x->nbuckets += ADDBUCKETS;
			}
			x->nused = (short)bkt;
		}
		bkt--;
		x->bucket[bkt].rh_rec = rh_rec;
		x->totbytes += (siz - x->bucket[bkt].nbytes);
		x->bucket[bkt].nbytes = (short)siz;
		ok = YES;
		dbpmrkpgdirty(rh_xrec);
	}
OVFLW:
	return (ok);
}

static void _xrecwrite(XRFCB *h, const void *data, int siz)
{
	int left, mv = 0;
	char *new_bucket;
	RHDL new_rhdl, old_rhdl;
	int new_sz, old_sz, prv;
	left = (MAXRECSIZ - 32) - h->xbucket.xnbytes;
	if (left > 0)
	{
		new_rhdl = _recist(new_sz = (old_sz = h->xbucket.xnbytes) + (mv = MIN(left, siz)));
		prv = dbplockpage(new_rhdl);
		new_bucket = (char *)_recadr00(new_rhdl);
		if (old_sz > 0)
		{
			memmove(new_bucket, _recadr00(old_rhdl = h->xbucket.rh_bucket), old_sz);
			_recdel(old_rhdl);
		}
		memmove(new_bucket + old_sz, data, mv);
		dbpfreepage(new_rhdl, prv);
		xrecsetbucket(h->xrh_rec, h->bkt, new_rhdl, new_sz);
		h->xbucket.xnbytes = (short)new_sz;
		h->xbucket.rh_bucket = new_rhdl;
		siz -= mv;
	}
	if (siz)
	{
		h->xbucket.rh_bucket = h->xbucket.xnbytes = 0;
		h->bkt++;
		_xrecwrite(h, ((char *)data) + mv, siz); // RECURSIVE CALL
	}
}

static int xrecput(const void *data, int siz, XRFCB *x)
{
	if (IS_XREC(x->xrh_rec) && (x->xflags & XM_WRITE))
	{
		dbactv(x->h_db);
		x->xtotbytes += siz;
		x->tot_off += siz;
		_xrecwrite(x, data, siz);
		return (YES);
	}
	return (NO);
}

static int xrecget(void *data, int siz, XRFCB *x)
{
	int32_t left;
	if (IS_XREC(x->xrh_rec) && (x->xflags & XM_READ) && x->tot_off < x->xtotbytes)
	{
		dbactv(x->h_db);
		if ((left = x->xtotbytes - x->tot_off) < siz)
			siz = short(left);
		x->tot_off += siz;
		xtnd(x, data, siz);
	}
	else
		siz = 0;
	return (siz);
}

int zrecsizof(HDL db, RHDL rhdl)
{
if (rhdl==0) return(0);
dbactv(db);
if (IS_XREC(rhdl))
	return (((XREC *)_recadr00(rhdl))->totbytes);
short siz;
_recadr0(rhdl, &siz);
return (siz);
}

int zrecget(HDL db, RHDL rhdl, void *data, int sz)
{
	if (IS_XREC(rhdl))
	{
		XRFCB *xr = (XRFCB *)xrecopen(db, rhdl, "r");
		sz = xrecget(data, sz, xr);
		memtake(xr);
	}
	else
		sz = recget(db, rhdl, data, sz);
	return (sz);
}

static void zrecwrite(HDL db, const void *data, int sz, RHDL rhdl)
{
	XRFCB *xr = (XRFCB *)xrecopen(db, rhdl, "w");
	if (!xr || !xrecput(data, sz, xr))
		throw SE_REC_WRITE;
	memtake(xr);
}

// 10/03/19 watch out for rh=0
void *zrecmem(HDL db, RHDL rhdl, int *sz)
{
	void *data = 0;
	int size = 0;
	if (rhdl)
		size = zrecsizof(db, rhdl);
	if (size)
		if (zrecget(db, rhdl, data = memgive(size), size) != size)
		{
			Scrap(data);
			size = 0;
		}
	if (sz!=NULL) *sz = size;
	return (data);
}

RHDL zrecadd(HDL db, const void *data, int sz)
{
	RHDL rhdl;
	if (sz > MAXRECSIZ)
		zrecwrite(db, data, sz, rhdl = xrecist(db));
	else
		rhdl = recadd(db, data, (short)sz);
	return (rhdl);
}

RHDL zrecupd(HDL db, RHDL rhdl, const void *data, int sz)
{
	int xrec = (sz > MAXRECSIZ);
	if (IS_XREC(rhdl) != xrec) // changed rec to xrec or viceversa
	{
		recdel(db, rhdl);
		return (zrecadd(db, data, sz));
	}
	if (xrec)
		zrecwrite(db, data, sz, rhdl);
	else
		recupd(db, rhdl, data, sz);
	return (rhdl);
}

RHDL zrecadd_or_upd(HDL db, RHDL rh, void *data, int sz, bool *rhupd)
{
bool dummy; if (rhupd==NULL) rhupd=&dummy;
if (rh==0) {*rhupd=true; return(zrecadd(db,data,sz));}
RHDL nrh=zrecupd(db,rh,data,sz);
if (nrh!=rh) *rhupd=true;
return(nrh);
}


#ifdef DBERROR_STUFF
DBERROR::DBERROR(int *_err)
{
	err = _err;
	SetErrorText("");
}

// Unless the db subsystem already set some useful error text,
// copy in the name of most recently active db file.
DBERROR::~DBERROR()
{
	if (*err <= -300 && *err > -511) // database subsystem error
		SetErrorText_if_blank("Database error %d accessing %s", *err, dbfnam(0));
}
#endif // DBERROR_STUFF

#ifdef docum
Functions SETTING / testing actv_prcd BNDREPOS(if n / f set NO, else no change) TRAVSBRANCH(set YES if n / f and no higher keys, else NO) _BKYDEL(set NO) ADD_BKEY_TO_NODE(if node is a leaf set YES, else no change) BKYADD(set YES) BKYSETPOS(set NO) _Bkysetnxthdl(tests, sets NO if recurse, else YES) _Bkysetprvhdl(always tests and sets) Bkypos(temporarily sets YES if LST_DUP) Bkyscn(temporarily sets NO if BK_CUR) Bkysrch(always sets NO if key is found)

						Functions SETTING
	/ testing actv_exists TRAVSBRANCH(set YES if found, else NO) _BKYDEL(set NO) ADD_BKEY_TO_NODE(if node is a leaf set YES, else no change) BKYSETPOS(set NO) _Bkysetnxthdl(no check.If no recurse - NO if n / f & no higher, else YES) _Bkysetprvhdl(always checks and sets) Bkyscn(check only if BK_CUR)

		  ...... is called by... bndrepos _Bkydel BKYDEL _Bkysetnxthdl(recurses) Bkynxtkey _Bkysetprvhdl(recurses) Bkyprvkey Travsbranch Bkyfnd(this one never cares about prcd / actv flags) BKYADD BKYDEL BKYPOS BKYSRCH Add_bkey_to_node Bkyins BKYADD Bkysetpos Bkynxtkey BKYSRCH Bkyprvkey BKYSRCH BKYPOS BTRRLS BKYSCN BTRRLS......
#endif //docum
