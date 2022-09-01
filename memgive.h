#ifndef MEMGIVE_H
#define MEMGIVE_H

void	*memadup(const void*,Uint);
void	*memgive(Uint);
void	*memrealloc(void*,Uint);
void	memtake(const void *data);
int	memtakeall(void);		// Release all blocks allocated by memgive/memrealloc

void	scrap(void **pointer);
												// If non-NULL, delete & zeroise...
#define Scrap(a) scrap((void**)(&(a)))			//		ptr-> allocated memory
#define SCRAP(a) {if (a) {delete (a); (a)=0;}}	//		ptr-> class object DON'T FOLLOW WITH SEMICOLON

// position of 'ky' in 'tbl', or NOTFND
int in_table(int *p, const void *ky, void *tbl, int ct, int sz, PFI_v_v cmp);

int _cdecl cp_str(char *a, char *b);

class TAG {			// A class to dynamically allocate space for any number
public:					// of variable (elemsz=0) or fixedlen (elemsz>0) strings
TAG	();	// and retrieve any item by number as requested
TAG (int first);
~TAG	();
int leaks(void);
private:
int	id;
};
extern int first_leak;
struct LEAK_CT {int classes, files, memblocks;};
int leak_tracker(int start);

class DYNAG: public TAG
{										// Class to dynamically allocate space for any number
public:								// of variable-length strings (_sz=0) or fixedlen items (_sz>0)
DYNAG	(int _sz, int _ct=0);	// and retrieve any item by number as requested
DYNAG (DYNAG *copyfrom);
~DYNAG	();
void	*put(const void *item, int n=NOTFND);	// Add item to table & return addr
void	*puti(int i);							// (just a little wrapper because we often store 1-2-4 byte int's in tables)
void	*get(int n);					// Retrieve address of item 'n'
void	del(int n);						// Delete item 'n' (use del(n)+put(n) to update)
int	ct;								// No. of items in Category (s/b read-only!)
int	sz;
int	in(const void *item);			// Return subscript of 'item' if in table, else NOTFND
int	in_or_add(const void *k);
void	*cargo(void *data, int sz=0);	// general-purpose storage area within the class object (default=NULL=unallocated)
protected:
int	find_str(int *p, const void *k);
char	*a;
int	len;
private:
void	init(int _sz, int _ct);
DYNAG	*slave;							// Offset of each entry in main table if vari-length (elemsz=0)
int	eot,cargo_sz;
void	*_cargo;						// (app code can't directly access the private cargo pointer)
};

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
void	merge(DYNTBL *add);
int		set_cp(PFI_v_v _cp);
private:
PFI_v_v	cp;
};

#endif
