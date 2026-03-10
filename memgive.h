#ifndef MEMGIVE_H
#define MEMGIVE_H

void	*memadup(const void*,Uint);
void	*memgive(Uint);
void	*memrealloc(void*,Uint);
void	memtake(const void *data);
int	memtakeall(void);		// Release all blocks allocated by memgive/memrealloc

void swap_data(void *ad1, void *ad2, int sz);
void	scrap(void **pointer);		// If non-NULL, delete & zeroise... BUT USE MACRO Scrap(memptr) or SCRAP(class)
#define Scrap(a) scrap((void**)(&(a)))				//		ptr-> allocated memory
#define SCRAP(a) {if (a) {delete (a); (a)=0;}}	//		ptr-> class object DON'T FOLLOW WITH SEMICOLON

// position of 'ky' in 'tbl', or NOTFND. If non-null, set to position ky WOULD go in, if not already present
int in_table(int *pos, const void *ky, void *tbl, int ct, int sz, PFI_v_v cmp);

int  cp_str(char *a, char *b);

// These globals allow app to 'remotely' change the value we want to trap for without recompiling
extern int sought_mem_leak;

int leak_tracker(int start);

class DYNAG
{										// Class to dynamically allocate space for any number
public:								// of variable-length strings (_sz=0) or fixedlen items (_sz>0)
DYNAG	(int _sz, int _ct=0);	// and retrieve any item by number as requested
DYNAG (DYNAG *copyfrom);
DYNAG (char *multistring, int sz);
DYNAG (HDL db, RHDL rh);
DYNAG (int sz, HDL db, RHDL rh);
~DYNAG	();
void	*put(const void *item, int n=NOTFND);	// Add item to table & return addr
void	*puti(int i);							// (just a little wrapper because we often store 1-2-4 byte int's in tables)
void	*get(int n);					// Retrieve address of item 'n'
void	del(int n);						// Delete item 'n' (use del(n)+put(n) to update)
int	ct;								// No. of items in Category (s/b read-only!)
int	len;
int	sz;                  // WHY IS THIS PUBLIC?????????????????????????????????????????????????
int	in(const void *item);			// Return subscript of 'item' if in table, else NOTFND
int	in_or_add(const void *k);
void	*cargo(const void *data, int sz=0);
int	total_size(void);		// total size of table (incl EOS of final string if sz=0)
protected:
int	find_str(int *p, const void *k);
char	*dta;
private:
void	init(int _sz, int _ct);
DYNAG	*slave;							// Offset of each entry in main table if vari-length (elemsz=0)
int	eot,cargo_sz;
void	*_cargo;						// (app code can't directly access the private cargo pointer)
};

// cargo = 	// general-purpose storage area within the class object (default=NULL=unallocated)
// **** Note - 'put(0)' squeezes table to eliminate alloc'ed slack at end


class DYNTBL: public DYNAG		// Derived DYNAG class with sorted table
{								// put() doesn't add items already present
public:							// Contructor takes comparator as well as size
DYNTBL(int _sz, PFI_v_v _cp, int _ct=0);
void	*put(const void *k);
void	*puti(int i);			// (just a little wrapper because we often store 1-2-4 byte int's in tables)
void	del(int n);				// Delete item 'n' (use del(n)+put(n) to update)
int	in(const void *k);
int	in_or_add(const void *k);
int	in_GE(const void *k);	// EQV in() except it returns subscript as per BK_GE (if there is such a key in table)
void	*find(const void *k);
void	*find_or_add(const void *k);
void	merge(DYNTBL *add);
int		set_cp(PFI_v_v _cp);
private:
PFI_v_v	cp;
};

class FDYNTBL: public DYNTBL		// "Persistent" file-based derived DYNTBL class
{
public:							// Contructor takes comparator as well as size
FDYNTBL(const char *filename, int _len, PFI_v_v _cp, int _ct=0);
~FDYNTBL();
void save_to_disc(void);
bool no_save=false;
private:
char fn[256];
};

class PATHDYNAG: public DYNAG		// table holding all the legs within a passed path, FROM ROOT DOWN
{
public:
PATHDYNAG(const char *path);
int is_mount();   // return 1 if /mnt, 2 if /media/<user>, 0 if neither (not a mounted device?)
};


struct TUPLE {char *name, *value;};

class DYNTUPLE		// class of tuples (pair of ALLOCATED string pointers, name+value)
{						// put() doesn't add items already present
public:
DYNTUPLE(void);
~DYNTUPLE();
void	put(const char *name, const char *value);	// delete if value==NULL
const	char *get(const char *name);	// return fully-formatted allocated blob if name==NULL
void	load(const char *blob);
private:
DYNAG *tbl;
};

char *dynag2multistring(DYNAG *d, int *sz);  // extract variable-length DYNAG records into allocated multistring
void zrec2dynag(DYNAG *tbl, HDL db, RHDL rh);

uchar condense_filesize(int64_t filesize);         // squash filesize (min 10Mb, max 10Gb) into a single byte 
int64_t expand_filesize(uchar condensed_value);    // convert squashed single byte back to approx size >= 10Mb, <= 10Gb

#endif
