#include <assert.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <cstdarg>

#include "pdef.h"
#include "memgive.h"
#include "str.h"
#include "flopen.h"
#include "log.h"
#include "db.h"
#include "drinfo.h"

// Only define MEMLOG when troubleshooting (don't want the overrhead in normal operation)
//#define MEMLOG 1

int first_leak;

static int		uniq;

static TAG		*logger;
static DYNTBL	*log;


#define MAXCT 65000		// 02/12/02

#ifdef MEMLOG		// see pdecs.h
#define FST	12		// pre-25/09/02 was 6 (size was 2-byte short, not 4-byte int32_t)
#define LST 4		// 4
#else
#define FST	0
#define LST 0
#endif
#define EXTRA_BYTES (FST+LST)

static void **a;
static int ct,c1;

//int do_memchk;
void memchk(char *txt)
{
//if (!do_memchk) return;
#ifdef MEMLOG
int totsiz=0;
for (int i=0;i<ct;i++)
	{
	char *c=(char*)a[i];
	int32_t siz=*(int32_t*)c;
	totsiz+=siz;
	if ((*(int32_t*)&c[FST-4]!=0x12345678 || *(int32_t*)&c[siz+FST]!=0x87654321))
		{
		SetErrorText("Corrupt memory:%s",txt);
		throw SE_MEMBAD;
		}
	}
int open_file_handles_ct(void);
char w[128];
strfmt(w,"Open Files:%ld, Objects:%ld, MemUsed:%ld",open_file_handles_ct(),log->ct,totsiz);
sjhlog("%s %s",txt,w);
#endif
}

void give_first_leak(int sz)	// breakpoint HERE after setting sought uniq value 10 lines down...
{
sz=ct;
}

#ifdef MEMLOG
#define LUMP 1024		// Chunk size for extending 'a'
static void*  log_give(void *p, Uint siz)	// *p==REAL pointer	// Log this block as 'allocated'
{																// and return address of "app-level data portion" of block
char *c=(char*)p;
Uint *u=(Uint*)p;
u[0]=siz;
u[1]=++uniq; 
if (uniq==67)				// set to value of first_leak as reported by dllclosedown
give_first_leak(siz);	// breakpoint HERE when unique sequence number of the mem_leak is allocated
*(int32_t*)&c[FST-4]=0x12345678;				// Put our special values before and after the address returned to app,
*(int32_t*)&c[siz+FST]=0x87654321;				// so we can check for buffer over/underrun when it's released

if (ct>=MAXCT) throw SE_OUTOFHEAP;
if (c1<=ct)
	{
	c1+=LUMP;
	a=(void**)(ct?realloc(a,c1*sizeof(void*)):calloc(c1,sizeof(void*)));
	}
a[ct++]=p;
return(&c[FST]);
}

static void  log_take(const void *p)	// *p==PSEUDO pointer from App	// Remove this block from the
{																		// table of 'allocated' addresses
char *c=((char*)p)-FST;					// (get the REAL pointer to allocated memory block)
int32_t siz=*(Uint*)c;
if ((*(int32_t*)&c[FST-4]!=0x12345678 || *(int32_t*)&c[siz+FST]!=0x87654321))
	{
	SetErrorText("Corrupt memory block over/underrun");
	throw SE_MEMBAD;
	}
for (int	i=ct;i--;)
	if (a[i]==c)
		{
		if (--ct>i) memmove(&a[i],&a[i+1],(ct-i)*sizeof(void*));
		return;
		}
throw SE_MEMPTR;
}
#else
#define log_give(p,siz) (p)
#endif


static void* give(Uint siz)
{
if (!siz)
	throw SE_BADALLOC;
void *p=calloc(siz+EXTRA_BYTES,1);
return(log_give(p,siz));
}

void *memrealloc(void *data, Uint siz)		// *data==PSEUDO pointer
{
#ifdef MEMLOG
if (data) log_take(data);
#endif
if (!siz) throw SE_BADALLOC;
char *c=(char*)data; if (c) c-=FST;
c=(char*)realloc(c,siz+EXTRA_BYTES);
return(log_give(c,siz));
}

void memtake(const void *data)
{
if(data)
	{
#ifdef MEMLOG
	log_take(data);
#endif
	free((void*)(((char*)data)-FST));
	}
}


void *memgive(Uint siz) 
{
if (!siz)
	throw SE_BADALLOC;
return(give(siz));
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

int memtakeall(void)						// Release all blocks allocated by
{											// by memgive() or memrealloc()
int leak=ct;
while (ct)
	{
#ifdef MEMLOG
		{
		int32_t *pi=(int32_t*)a[ct-1];
//		int sz=pi[0];				// get the (app-requested) allocated block size,
		int32_t sq=pi[1];				// - unique sequence number, (of this mem_leak)
		if (!first_leak)
			first_leak=sq;			// LINK01 
//		char *data=(char*)&pi[3];	// - and 'pointer to app-perceived data area'
		}							// so we can track down where it got allocated by the app
#endif
	memtake(((char*)(a[ct-1]))+FST);		// (memtake decrements ct)
	}
if (c1)
	{
	c1=0;
	free(a);
	}
return(leak);
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
	m=((int32_t(hi)+lo)/2);
	if ((i=(cmp)(ky,T(m)))==0) return(*(p)=m);
	if (i<0) hi=m-1; else lo=m+1;
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

DYNAG::DYNAG(char *multistring, int sz)	// constructor (for len=0 = strings) initialised from multistring
{
init(0,0);
while (sz>0)
    {
    put(multistring);
    int len=strlen(multistring)+1;
    multistring+=len;
    sz-=len;
    }
}

DYNAG::DYNAG(HDL db, RHDL rh)	// constructor (for len=0 = strings) initialised from multistring ZREC
{
init(0,0);
int sz=0, len;
char *multistring=(char*)zrecmem(db,rh,&sz);
for (char *p=multistring; sz>0; sz-=len)
    {
    put(p);
    p+=(len=strlen(p)+1);
    }
memtake(multistring);
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


int DYNAG::total_size(void)
{
if (len) return(len*ct);
if (ct==0) return(0);
return(strend((char*)get(ct-1)) - (char*)get(0) +1);
}

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
	int to=(int)(long(p)-long(a)), this_len=len;
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

void* DYNAG::cargo(const void *data, int sz)
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



FDYNTBL::FDYNTBL(const char *_filename, int _len, PFI_v_v _cp, int _ct):DYNTBL(_len, _cp, _ct)
{
int fsz=dr_filesize(strcpy(fn,_filename));
int i, c=fsz/len;
if ((c*len)!=fsz) m_finish("FDYNTBL size error");
void *k=memgive(len);
HDL f=flopen(fn,"r");
for (i=0;i<c;i++)
	{
	int bytes=flget(k,len,f);
	if (bytes!=len)  m_finish("FDYNTBL readf error");
	put(k);
	}
if (f!=NULL) flclose(f);
memtake(k);
}

void FDYNTBL::save_to_disc(void)
{
HDL f=flopen(fn,"w");
//printf("\nsave %d elements len:%d total %d\n",ct,len,ct*len);
flput(get(0),len*ct,f);
flclose(f);
}

FDYNTBL::~FDYNTBL()
{
if (!no_save) save_to_disc();
}


PATHDYNAG::PATHDYNAG(const char *fullpath):DYNAG(0,0)
{
char wrk[512], *p;
int i;
for (strcpy(p=wrk,fullpath); *p=='/' && !SAME2BYTES(p,"/"); p+=i)
	{
	i=stridxc('/',strdel(p,1));
	if (i!=NOTFND) p[i]=0;		// change backslash to EOS to isolate one specific path element
	put(p);
	if (i==NOTFND) break;
	p[i]='/';						// replace the backslash so it can be skipped over by next pass
	}
if (ct==0) m_finish("Invalid fully-qualified path\n%s",fullpath);
}

int PATHDYNAG::is_mount(void)
{
if (!strcmp((char*)get(0),"mnt")) return(1);
if (!strcmp((char*)get(0),"media")) return(2);
return(8);	
}

void swap_data(void *ad1, void *ad2, int sz)
{
char wrk[8],*w;
if (sz<=8) w=wrk; else w=(char*)memgive(sz);
memmove(w,ad1,sz);
memmove(ad1,ad2,sz);
memmove(ad2,w,sz);
if (sz>8) memtake(w);
}

void	m_finish(const char *fmt,...)
{
char w[200];
va_list va;
va_start(va,fmt);
_strfmt(w,fmt,va);
printf("%s\r\n",w);
sjhlog("%s",w);
throw 222;
}


int _cdecl cp_mem1(const void *a, const void *b) {return(memcmp(a,b,1));}
int _cdecl cp_mem2(const void *a, const void *b) {return(memcmp(a,b,2));}
int _cdecl cp_mem3(const void *a, const void *b) {return(memcmp(a,b,3));}
int _cdecl cp_mem4(const void *a, const void *b) {return(memcmp(a,b,4));}
int _cdecl cp_mem6(const void *a, const void *b) {return(memcmp(a,b,6));}
int _cdecl cp_mem8(const void *a, const void *b) {return(memcmp(a,b,8));}

int cp_short_v(short a, short b)	{return((a<b)?-1:(a>b));}
int cp_ushort_v(ushort a, ushort b){return((a<b)?-1:(a>b));}
int cp_long_v(int32_t a, int32_t b)		{return((a<b)?-1:(a>b));}
int cp_ulong_v(Ulong a, Ulong b)	{return((a<b)?-1:(a>b));}

int cp_short(const void *a, const void *b)
{return((*((short*)a)<*((short*)b))?-1:(*((short*)a)>*((short*)b)));}

int cp_ushort(const void *a, const void *b)
{return((*((ushort*)a)<*((ushort*)b))?-1:(*((ushort*)a)>*((ushort*)b)));}

int cp_long(const void *a, const void *b)
{return((*((int32_t*)a)<*((int32_t*)b))?-1:(*((int32_t*)a)>*((int32_t*)b)));}

int cp_int32_t(const void *a, const void *b)
{return((*((int32_t*)a)<*((int32_t*)b))?-1:(*((int32_t*)a)>*((int32_t*)b)));}

int cp_int64_t(const void *a, const void *b)
{return((*((int64_t*)a)<*((int64_t*)b))?-1:(*((int64_t*)a)>*((int64_t*)b)));}

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

static int zct;

TAG::TAG()	// This is the 'implicit' TAG constructor (no parameter passed to constructor)
{				// that gets called whenever a derived class (DYNAG or DYNTBL) is initialised
if (log)
	{
	id=++zct;
if (id==32)
{
//assert(0);
id=32;						// run to cursor HERE to Find when class_leak 'id' was instantiated
}
	void *ptr=(void*)this;
	log->put(&ptr);
	}
else id=NO;
}

TAG::TAG(int first)	// FIRST call is 'explicit' - invoked by leak_tracker(YES) at program start
{		// Subsequent DYNAG/DYNTBL constructors 'implicitly' create TAGs without parameter, FOR LOGGING
first=first; // ??????? what's this param for ?????
log=new DYNTBL(8,cp_int64_t);
id=NOTFND;
}

int TAG::leaks(void)
{
DYNTBL *w=log;
log=0;
int i,ct;
for (i=ct=0;i<w->ct;i++)
	{
	void *v=w->get(i);
	TAG *t=*(TAG**)v;
	int leak=t->id;
if (!first_leak)
{
first_leak=leak*10+1;
//assert(first_leak!=11);
}
	ct++;
	}
delete w;
return(ct);
}

TAG::~TAG()
{
if (log)
	{
	if (id>0)
		{
		void *ptr=(void*)this;
		int ck=log->ct;
		log->del(log->in(&ptr));
		if (log->ct!=ck-1) m_finish("dammit");
		}
	if (id==NOTFND)
		{
		if (leaks())
			m_finish("Logged Class instances not released!");
		}
	}
}

int leak_tracker(int start)
{
#ifndef MEMLOG
return(0);
#endif
if (start)
	{
	assert(logger==0);
	logger=new TAG(YES);
	return(NO);
	}
assert(logger!=0);
static LEAK_CT lk;
lk.classes=logger->leaks();
SCRAP(logger);
lk.files=flcloseall();				// any non=closed files?
lk.memblocks=memtakeall();
if (lk.classes!=0 || lk.files!=0 || lk.memblocks!=0)
	{
	printf("FirstLeak:%d Class:%d Files:%d MemBlks:%d\r\n", first_leak, lk.classes, lk.files, lk.memblocks);
	return(-77);
	}
return(NO);
}

/*
NAMBAG::NAMBAG()
{
d=new DYNAG(sizeof(int16_t));
}

NAMBAG::~NAMBAG()
{
memtake(ad);
delete d;
}

int NAMBAG::put(const char *s)  // find or add 's' and return its subscript
{
int i;
for (i=0;i<d->ct;i++)
    if (!strcmp(s,get(i))) return(i); // string already present
d->put(&sz);
int len=strlen(s)+1;
ad=(char*)memrealloc(ad,sz+len);
strcpy(&ad[sz],s);
sz+=len;
return(i);  // returns subscript of newly-appended final string
}

const char*NAMBAG::get(int i)   // return ptr to stored string element 'i'
{
return(&ad[*((int16_t*)d->get(i))]);
}
*/

//struct TUPLE {char *name, *value;};

DYNTUPLE::DYNTUPLE()
{
tbl=new DYNAG(sizeof(TUPLE));
}
DYNTUPLE::~DYNTUPLE()
{
while (tbl->ct)
	{
	TUPLE *t=(TUPLE*)tbl->get(0);
	memtake(t->name); memtake(t->value);
	tbl->del(0);
	}
delete tbl;
}

void	DYNTUPLE::put(const char *name, const char *value)
{
TUPLE t;
t.name=stradup(name);
t.value=stradup(value);
tbl->put(&t);
}

void	DYNTUPLE::load(const char *blob)
{
int i=0,j;
TUPLE t;
while (blob[i])
	{
	if (blob[i]!=CHR_QTDOUBLE) {i++; continue;}
	for (j=1; blob[i+j]!=CHR_QTDOUBLE; j++)
		if (j>=14) m_finish("Invalid blob element name");
	if (j==1) m_finish("Invalid Blob element name");
	t.name=(char*)memgive(j);
	memmove(t.name, &blob[i+1], j-1);	// (leaves the EOS nullbyte untouched)
	}
tbl->put(&t);
}

const char*	DYNTUPLE::get(const char *name)
{
for (int i=0; i<tbl->ct; i++)
	{
	TUPLE *t=(TUPLE*)tbl->get(i);
	if (!strcmp(t->name, name)) return(t->value);
	}
return(NULL);
}
