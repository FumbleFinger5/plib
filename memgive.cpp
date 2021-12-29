#include <assert.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <cstdarg>

#include "pdef.h"
#include "memgive.h"
#include "str.h"


void *memrealloc(void *data, Uint siz)		// *data==PSEUDO pointer
{
if (!siz) throw SE_BADALLOC;
char *c=(char*)data;
c=(char*)realloc(c,siz);
return(c);
}
void memtake(const void *data)
{
if(data)
	{
	free((void*)(((char*)data)));
	}
}

void *memgive(Uint siz) 
{
if (!siz)
	throw SE_BADALLOC;
void *p=calloc(siz,1);
return(p);
}

void *memadup(const void *data, Uint siz)
{
return(memmove(memgive(siz),data,siz));
}


void	scrap(void **pointer)
{
if (*pointer)
	{
	memtake(*pointer);
	*pointer=0;
	}
}


#define T(i) &((char*)tbl)[(i)*sz]
int in_table(int *p, const void *ky, void *tbl, int c, int sz, PFI_v_v cmp)
{
int	m,lo,hi,i;
if (!p) p=&i;					// 'dummy' address so it's not NULLPTR
m=lo=0;
hi=c-1;
while (lo<=hi)
	{
	m=((long(hi)+lo)/2);
	if ((i=(cmp)(ky,T(m)))==0) return(*(p)=m);
	else if (i<0) hi=m-1; else lo=m+1;
	}
*(p)=m+(m<c && (cmp)(ky,T(m))>0);
return(NOTFND);
}


// Put 'ky' into table 'tbl', keeping the table sorted as per 'cmp()',
// if it isn't already present. Return the subscript of the existing
// position of 'ky' if it was already in table, or at which it was added
// if it wasn't (in which case '*ct' is incremented. Note that comparator
// doesn't necessarily compare 'sz' bytes (it may be less), but if a key
// is added, it WILL add that many bytes.

// WARNING!		Don't use the slot tbl[ct] as workspace for 'ky', because
//					it gets overwritten BEFORE being moved into the table!

int to_table(void *ky, void *tbl, int *ct, int sz, PFI_v_v cmp)
{
int	m,c;
if (in_table(&m,ky,tbl,c=*ct,sz,cmp)==NOTFND)
	{if (m<c) memmove(T(m+1),T(m),(c-m)*sz); 	(*ct)++; memmove(T(m),ky,sz);}
return(m);
}
#undef T

int to_table_s(void *ky, void *tbl, short *ct, int sz, PFI_v_v cmp)
{
int	c=*ct, ret=to_table(ky,tbl,&c,sz,cmp);
*ct=(short)c;
return(ret);
}

void DYNAG::init(int _sz, int _ct)
{
len=_sz;
eot=ct=0;
_cargo=0;
slave=NULL;
if (len)
	{
	if (!_ct) _ct=16; // start with space for 16 items unless we know how many we'll want
	sz=len * _ct;
	}
else		// create Slave dynag of offsets to strings if vari-length items
	{
	slave=new DYNAG(sizeof(int),_ct);
	sz=128;
	}
a=(char *)memgive(sz);
}

DYNAG::DYNAG(int _sz, int _ct)
{
init(_sz,_ct);
}

DYNAG::DYNAG(DYNAG *org)	// 'Copy' constructor initialised from existing DYNAG
{
init(org->len,org->ct);
for (int i=0;i<org->ct;i++) put(org->get(i),i);
if (org->_cargo) cargo(org->cargo(0),org->cargo_sz);
}


DYNAG::~DYNAG(void)
{
Scrap(_cargo);
memtake(a);
SCRAP(slave);
}


void* DYNAG::put(const void *item, int n)	// Put an item into array. If there's not enough room, increase by another chunk
{
int this_len;			// Avoids 'bitty' allocs, but minimises wasted 'slack'
char *ret;				// Use put(0) to eliminate slack completely at any time
if (!item)
	{
	a=(char *)memrealloc(a,(sz=eot)+!eot);
	return(0);
	}	// avoid 'eot==0'!
if (len) this_len=len; else this_len=strlen((char*)item)+1;
if (eot+this_len>sz)
	{
	int newsz=sz+(sz/4+this_len+(len?(len*8):64));
	a=(char *)memrealloc(a,sz=newsz);
	}
ret=&a[eot];	// save current end-of-table for return value
eot+=this_len;			// Increase current 'used' table length
if (n>=ct) n=NOTFND;
if (n>=0)
	{
	ret=(char *)get(n);
	memmove(&ret[this_len],ret,eot-(ret-a)-this_len);
	}
else n=ct;
if (!len)								// If adding entry to variable-length (len=0) DYNAG,
	{									// bump up offsets in any following entries in Slave
	int offset=ret-a;
	int *q=(int*)slave->put(&offset,n);
	while (n++<ct)
		(*(++q))+=this_len;
	}
ct++;
return(memmove(ret,item,this_len));	// (put new item in and return its address)
}

void* DYNAG::puti(int i) {return(put(&i));}		// (just a little wrapper because we often store 1-2-4 byte int's in tables)

void *DYNAG::get(int n)
{
void *addr=0;
if (n>=0 && n<ct)
	{
	if (len)
		addr=&a[n*len];
	else
		addr=&a[*(int*)slave->get(n)];
	}
return(addr);
}

void DYNAG::del(int n)
{
char *p=(char *)get(n);
if (p)
	{
	int to=long(p)-long(a),this_len=len;
	ct--;
	if (!this_len)
		{
		this_len=strlen(p)+1;
		slave->del(n);
		for (int x=n;x<ct;x++) *(int*)slave->get(x)-=this_len;
		}
	if ((n=eot-(to+this_len))>0) memmove(&a[to],&a[to+this_len],n);
	eot-=this_len;
	}
}

int DYNAG::in(const void *item)	// Return subscript of item in array, or NOTFND
{
char *p;
for (int n=0;(p=(char*)get(n))!=NULL;n++)
	{
	switch (len)
		{
		case 0: if (!strcmp(p, (char*)item)) return(n); break;
		case 1: if (p[0]==*(char*)item) return(n); break;
		case 2: if (SAME2BYTES(p,item)) return(n); break;
		case 4: if (SAME4BYTES(p,item)) return(n); break;
		default: if (!memcmp(p,item,len)) return(n); break;
		}
	}
return(NOTFND);
}

void* DYNAG::cargo(void *data, int sz)
{
if (sz)
	{
	Scrap(_cargo);
	if (data) _cargo=memadup(data,cargo_sz=sz);
	}
return(_cargo);
}



DYNTBL::DYNTBL(int _sz, PFI_v_v _cp, int _ct):DYNAG(_sz,_ct)
{
cp=_cp;
}

int DYNTBL::set_cp(PFI_v_v _cp)
{
qsort(a,ct,len,cp=_cp);
for (int i=0; i<ct-1; i++)
	if ( (cp)(&a[i*len],&a[(i+1)*len])>=0)
		return(NO);	// ??? - there must be a duplicate key!
return(YES);
}

void* DYNTBL::find(const void *k)
{
return(get(in(k)));
}


int DYNTBL::in(const void *k)
{
if (!len) return(DYNAG::in(k));
return(in_table(0,k,a,ct,len,cp));
}

int DYNAG::in_or_add(const void *k)
{
int i=in(k);
if (i==NOTFND) {i=ct; put(k);}
return(i);
}

int DYNTBL::in_or_add(const void *k)
{
int i=in(k);
if (i==NOTFND)
	{
	put(k);
	i=in(k);
	}
return(i);
}

int DYNTBL::in_GE(const void *k)
{
if (!len) m_finish("SysErr 963");
int p, exact=in_table(&p,k,a,ct,len,cp);	// If exact key doesn't exist, 'p' = position it would be at (i.e. BK_GE)
if (exact==NOTFND && p==ct) p=NOTFND;
return(p);
}

static const char *_aaa, *_kkk;
static int _cdecl cp_slave(int *a, int *b)
{
return(strcmp((*a==NOTFND)?_kkk:&_aaa[*a],(*b==NOTFND)?_kkk:&_aaa[*b]));
}

int DYNAG::find_str(int *p, const void *k)
{
_aaa=a;
_kkk=(const char*)k;
int fix=-1;
int exact=in_table(p,&fix,slave->a,ct,4,(PFI_v_v)cp_slave);	// If exact key doesn't exist, 'p' = position it would be at (i.e. BK_GE)
if (exact==NOTFND && *p==ct) *p=NOTFND;
return(exact);
}

void* DYNTBL::put(const void *k)
{
int p=NOTFND;
if (!len)			// TO DO: find where this is used (to maintain sorted table of text strings)
	{
	int exact=find_str(&p,k);	// If exact key doesn't exist, 'p' = position it would be at (i.e. BK_GE)
	if (exact!=NOTFND) return(get(exact));
	return(DYNAG::put(k,p));
	}
if (!cp || in_table(&p,k,a,ct,len,cp)==NOTFND)	// 050108 - just 'append' if comparator not yet set
	return(DYNAG::put(k,p));
return(&a[len*p]);
}

void* DYNTBL::puti(int i) {return(put(&i));}		// (just a little wrapper because we often store 1-2-4 byte int's in tables)

void DYNTBL::del(int n)
{
DYNAG::del(n);
}

void DYNTBL::merge(DYNTBL *add)
{
if (len!=add->len) throw(SE_MEMTBL);
for (int i=0;i<add->ct;i++) put(add->get(i));
}


void swap_data(void *a, void *b, int sz)
{
char wrk[8],*w;
if (sz<=8) w=wrk; else w=(char*)memgive(sz);
memmove(w,a,sz);
memmove(a,b,sz);
memmove(b,w,sz);
if (sz>8) memtake(w);
}

void	m_finish(const char *fmt,...)
{
char w[200];
va_list va;
va_start(va,fmt);
_strfmt(w,fmt,va);
}


int _cdecl cp_mem1(const void *a, const void *b) {return(memcmp(a,b,1));}
int _cdecl cp_mem2(const void *a, const void *b) {return(memcmp(a,b,2));}
int _cdecl cp_mem3(const void *a, const void *b) {return(memcmp(a,b,3));}
int _cdecl cp_mem4(const void *a, const void *b) {return(memcmp(a,b,4));}
int _cdecl cp_mem6(const void *a, const void *b) {return(memcmp(a,b,6));}
int _cdecl cp_mem8(const void *a, const void *b) {return(memcmp(a,b,8));}

int cp_short_v(short a, short b)	{return((a<b)?-1:(a>b));}
int cp_ushort_v(ushort a, ushort b){return((a<b)?-1:(a>b));}
int cp_long_v(long a, long b)		{return((a<b)?-1:(a>b));}
int cp_ulong_v(Ulong a, Ulong b)	{return((a<b)?-1:(a>b));}

int cp_short(const void *a, const void *b)
{return((*((short*)a)<*((short*)b))?-1:(*((short*)a)>*((short*)b)));}

int cp_ushort(const void *a, const void *b)
{return((*((ushort*)a)<*((ushort*)b))?-1:(*((ushort*)a)>*((ushort*)b)));}

int cp_long(const void *a, const void *b)
{return((*((long*)a)<*((long*)b))?-1:(*((long*)a)>*((long*)b)));}

int cp_ulong(const void *a, const void *b)
{return((*((Ulong*)a)<*((Ulong*)b))?-1:(*((Ulong*)a)>*((Ulong*)b)));}

static int _cdecl cp_ulongn(const Ulong *a, const Ulong *b, int n)
{
int i,cmp;
for (i=cmp=0;!cmp && i<n;i++) cmp=cp_ulong_v(a[i],b[i]);
return(cmp);
}

int _cdecl cp_ulong2(const void *a, const void *b) {return(cp_ulongn((Ulong*)a,(Ulong*)b,2));}
int _cdecl cp_ulong4(const void *a, const void *b) {return(cp_ulongn((Ulong*)a,(Ulong*)b,4));}

static int _cdecl cp_shortn(const short *a, const short *b, int n)
{
int i,cmp;
for (i=cmp=0;!cmp && i<n;i++) cmp=cp_short_v(a[i],b[i]);
return(cmp);
}

int _cdecl cp_short2(const void *a, const void *b) {return(cp_shortn((short*)a,(short*)b,2));}

