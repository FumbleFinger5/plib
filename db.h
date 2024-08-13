#ifndef DB_H
#define DB_H

//	Seek mode parameters for B-tree Keys
#define	BK_LT		-2	// Seek key <  KEY
#define	BK_LE		-1	// Seek key <= KEY
#define	BK_EQ		0	// Seek key == KEY
#define	BK_GE		1	// Seek key >= KEY
#define	BK_GT		2	// Seek key >  KEY

//	Data type codes for search functions and B-trees
#define	DT_STR	 	1
#define	DT_USHORT	2
#define	DT_LONG		3
#define	DT_BYTES	4
#define	DT_SHORT	6
#define	DT_ULONG 	7 
#define	DT_USR 		128		// User-defined comparator


#define	DB_DIRTYPOS	38			// Offset of "dirty" flag byte database header
#define	DB_IS_CLEAN	'('		// Value in header while database is CLEAN
#define	DB_IS_DIRTY	'['		// Value in header while database is DIRTY
#define	PGSIZ		2048			// database page size
#define	FOOTERSIZ	24
#define	MAXRECSIZ	(PGSIZ-FOOTERSIZ-(3*sizeof(short)))		// Max size of (efficient) "single block" records
#define	MAXZRECSIZ	595800											// Even inefficient "multi-block" xrec's can't be bigger than this!



// -350 to -448	Database system
#define SE_NXTKEY		(-333)	// bad bkynxtkey()
#define SE_PRVKEY		(-334)	// bad bkyprvkey()
#define SE_BADPGID		(-385)	// bad page id in record handle
#define SE_BADCELLID	(-386)	// bad cell id in record handle
#define SE_NOFREEBUFS	(-387)	// no free buffers in buffer pool
#define SE_DBPGBAD		(-388)	// smashed database page
#define SE_DBOVRFLOW	(-389)	// database file too big (>64 megabytes)
#define SE_RECTOOBIG	(-390)	// Size of stored record bigger than page
#define SE_REC_WRITE	(-391)	// Error writing database record
#define SE_NULLHDL		(-392)	// Attempt to activate 'db' handle 0
#define	SE_NOTACTIVE	(-393)	// Database system not started
#define SE_DBCORRUPT	(-398)	// Database record is obviously invalid (unexpected size, etc)
#define SE_DBSAFE		(-399)	// [DB_SAFE] failure


//	-449 to -512	B-tree Indexes
#define SE_BADKEYTYP	(-449)	// bad key type in compare
#define SE_BADDELKEY	(-450)	// error during key deletion
#define SE_BADBROOT		(-451)	// Root of tree appears bad
#define SE_BADPARENT	(-452)	// Error rebuilding parent after Bkydel()


int	bkyadd(HDL btr,RHDL rhdl,void *key);					// YES/NO = OK/Fail
bool	bkydel(HDL btr,RHDL *rhdl,void *key);	// delete key only			// BOTH silently return false
bool	bkydel_rec(HDL btr, void *key);			// delete key AND rhdl		// if key doesn't exist
void	bkydel_all(HDL h_btr, int del_rhdl);					// del ALL keys and maybe recs
int	bkyupd(HDL btr,RHDL rhdl,void *key);					// YES/NO = OK/Fail
int	bkyscn_all(HDL btr,RHDL *rhdl,void *key, int *again);
int	bkysrch(HDL btr, int mode,RHDL *rhdl,void *key);		// YES/NO = OK/Fail
void	*btrcargo(HDL btr,void *cargo);							// get/update 10-byte btr cargo
void	btrclose(HDL btr);
int32_t	btrnkeys(HDL btr);
void	btrrls(HDL btr);
void	btr_set_cmppart(HDL btr, PFI_v_v func);					// (db internal)
RHDL	btrist(HDL btr, int keytyp, int keysiz);
HDL		btropen(HDL btr,RHDL rhdl_btr);

char	*dbfnam(HDL db);				// Fully-qualified pathname of *.DBF file
void	dbckpt(HDL db);
void	dbsafeclose(HDL db);			// Close database AND unset the db.hdr 'safe' flag if it was set to say 'dirty'
RHDL	dbgetanchor(HDL db);
int		dbist(const char *nam);			// YES/NO = OK/Fail
int		dbsetlock(int on);				// Lock db system against ALL file updates
//HDL		dbopen(const char *nam);
HDL		dbopen2(const char *nam);	// Check 'access()' and automatically open ReadOnly if necessary 
ushort	dbpgct(HDL db);
RHDL	dbsetanchor(HDL db,RHDL rhdl);	// note - returns RHDL, not void
void	db_set_temp(HDL db);			// Deletes on close
int		dbreadonly(HDL db);

RHDL	recadd(HDL db, const void *rec,short sz);
void	recdel(HDL,RHDL);
int		recget(HDL db,RHDL rhdl,void *rec, int mxsz);
void	recupd(HDL db,RHDL rhdl,const void *rec, int sz);

int		zrecsizof(HDL db,RHDL rhdl);
int		zrecget(HDL db, RHDL rhdl, void *data, int sz);	// Returns number of bytes read
RHDL	zrecadd(HDL db, const void *data, int sz);
RHDL	zrecupd(HDL db, RHDL rhdl, const void *data, int sz);
void*	zrecmem(HDL db, RHDL rhdl, int *sz);
RHDL zrecadd_or_upd(HDL db, RHDL rh, void *data, int sz, bool *rhupd);	// Add if rh=0, else update (may switch rec/xrec)


class DBX {    // base class for my databases to share filename() method
protected:
	HDL db;
public:
	const char *filename(void) {return(dbfnam(db));}
};


#endif // DB_H
