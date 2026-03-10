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

static int first_mem_leak;
int sought_mem_leak;


static int		uniq;

#define MAXCT 65000		// 02/12/02

#ifdef MEMLOG		// see pdecs.h
#define FST	12		// pre-25/09/02 was 6 (size was 2-byte short, not 4-byte int32_t)
#define LST 4		// 4
#else
#define FST	0
#define LST 0
#endif

#define EXTRA_BYTES (FST+LST)

static void **mcad;
static int ct,c1;

//int do_memchk;
void memchk(char *txt)
{
//if (!do_memchk) return;
#ifdef MEMLOG
int totsiz=0;
for (int i=0;i<ct;i++)
	{
	char *c=(char*)mcad[i];
	int32_t siz=*(int32_t*)c;
	totsiz+=siz;
	if ((*(int32_t*)&c[FST-4]!=0x12345678 || *(int32_t*)&c[siz+FST]!=0x87654321))
		{
		SJHLOG("Corrupt memory:%s",txt);
		throw SE_MEMBAD;
		}
	}
int open_file_handles_ct(void);
char w[128];
strfmt(w,"MemUsed:%ld",totsiz);
sjhlog("%s %s",txt,w);
#endif
}

#ifdef MEMLOG
#define LUMP 1024		// Chunk size for extending 'mcad'
static void*  log_give(void *p, Uint siz)	// *p==REAL pointer	// Log this block as 'allocated'
{																// and return address of "app-level data portion" of block
char *c=(char*)p;
Uint *u=(Uint*)p;
u[0]=siz;
u[1]=++uniq; 
if (uniq==sought_mem_leak)			// set to value of First_leak given by leak_tracker from global_closedown
   gbreak;            // breakpoint HERE to trap when unique sequence number of the mem_leak is allocated
*(int32_t*)&c[FST-4]=0x12345678;				// Put our special values before and after the address returned to app,
*(int32_t*)&c[siz+FST]=0x87654321;				// so we can check for buffer over/underrun when it's released

if (ct>=MAXCT)
   throw SE_OUTOFHEAP;
if (c1<=ct)
	{
	c1+=LUMP;
	mcad=(void**)(ct?realloc(mcad,c1*sizeof(void*)):calloc(c1,sizeof(void*)));
	}
mcad[ct++]=p;
return(&c[FST]);
}

static void  log_take(const void *p)	// *p==PSEUDO pointer from App
{													// Remove this block from the table of 'allocated' addresses
char *c=((char*)p)-FST;					   // (get the REAL pointer to allocated memory block)
int32_t siz=*(Uint*)c;
if ((*(int32_t*)&c[FST-4]!=0x12345678 || *(int32_t*)&c[siz+FST]!=0x87654321))
	{
	SJHLOG("Corrupt memory block over/underrun");
	throw SE_MEMBAD;
	}
for (int	i=ct;i--;)
	if (mcad[i]==c)
		{
		if (--ct>i) memmove(&mcad[i],&mcad[i+1],(ct-i)*sizeof(void*));
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

int memtakeall(void)		// Release any blocks allocated by memgive/memrealloc but not released by memtake
{								// grab the ID of the first such unreleased block so we can examine it in debugger 
int leak=ct;
while (ct)
	{
#ifdef MEMLOG
		{
		int32_t *pi=(int32_t*)mcad[ct-1];
//		int sz=pi[0];				// get the (app-requested) allocated block size,
		int32_t sq=pi[1];				// - unique sequence number, (of this mem_leak)
		if (!first_mem_leak)
			first_mem_leak=sq;			// LINK01 
//		char *data=(char*)&pi[3];	// - and 'pointer to app-perceived data area'
		}							// so we can track down where it got allocated by the app
#endif
	memtake(((char*)(mcad[ct-1]))+FST);		// (memtake decrements ct)
	}
if (c1)
	{
	c1=0;
	free(mcad);
	}
return(leak);
}

#define T(i) &((char*)tbl)[(i)*sz]
// Is 'ky' in SORTED 'tbl'? If so return subscript into table, else NOTFND
int in_table(int *pos, const void *ky, void *tbl, int ct, int sz, PFI_v_v cmp)
{
int	m,lo,hi,i;
if (!pos) pos=&i;					// 'dummy' address so it's not NULLPTR
m=lo=0;
hi=ct-1;
while (lo<=hi)
	{
	m=((int32_t(hi)+lo)/2);
	if ((i=(cmp)(ky,T(m)))==0) return(*(pos)=m);
	if (i<0) hi=m-1; else lo=m+1;
	}
*(pos)=m+(m<ct && (cmp)(ky,T(m))>0);
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
dta=(char *)memgive(sz);
}

DYNAG::DYNAG(int _sz, int _ct)
{
init(_sz,_ct);
}

DYNAG::DYNAG(DYNAG *org)	// 'Copy' constructor initialised from existing DYNAG
{
init(org->len,org->ct);
for (int i=0;i<org->ct;i++) put(org->get(i),i);
if (org->_cargo) cargo(org->cargo(0),org->cargo_sz);  // Make a COPY of source cargo, if non-null
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
char *multistring=(char*)zrecmem(db,rh,&sz); // reliably free'd 6 lines below
for (char *p=multistring; sz>0; sz-=len)
    {
    put(p);
    p+=(len=strlen(p)+1);
    }
memtake(multistring);
}

void zrec2dynag(DYNAG *tbl, HDL db, RHDL rh)
{
int tot_sz, ct, i;
char *w=(char*)zrecmem(db,rh,&tot_sz);
ct=tot_sz/tbl->len;
if (tbl->len*ct!=tot_sz)
	{
	SJHLOG("ct:%d tot_sz:%d tbl->len:%d",ct,tot_sz,tbl->len);
	m_finish("sizze rror!");
	}
for (i=0; i<ct; i++) tbl->put(&w[i*tbl->len]);
memtake(w);
}

// (not yet used)
DYNAG::DYNAG(int sz, HDL db, RHDL rh)	// constructor (for fixedlen recs) initialised from zrec array
{
int tot_sz, ct, i;
char *w=(char*)zrecmem(db,rh,&tot_sz);
ct=tot_sz/sz;
if (sz*ct!=tot_sz) m_finish("sizze rror!");
init(sz,ct);
for (i=0; i<ct; i++) put(&w[i*sz]);
memtake(w);
}

char *dynag2multistring(DYNAG *d, int *sz)  // extract variable-length DYNAG records into allocated multistring
{
char *buf;
for (int i=*sz=0;i<d->ct;i++)
   {
   char *p=(char*)d->get(i);
   int len1=strlen(p)+1;
   if (*sz==0) buf=stradup(p);
   else
      {
      buf=(char*)memrealloc(buf,*sz + len1);
      strcpy(&buf[*sz],p);
      }
   *sz+=len1;
   }
return(buf);
}

DYNAG::~DYNAG(void)
{
Scrap(_cargo);
memtake(dta);
SCRAP(slave);
}


void* DYNAG::put(const void *item, int n)	// Put an item into array. If there's not enough room, increase by another chunk
{
int this_len;			// Avoids 'bitty' allocs, but minimises wasted 'slack'
char *ret;				// Use put(0) to eliminate slack completely at any time
if (!item)
	{
	dta=(char *)memrealloc(dta,(sz=eot)+!eot);
	return(0);
	}	// avoid 'eot==0'!
if (len) this_len=len; else this_len=strlen((char*)item)+1;
if (eot+this_len>sz)
	{
	int newsz=sz+(sz/4+this_len+(len?(len*8):64));
	dta=(char *)memrealloc(dta,sz=newsz);
	}
ret=&dta[eot];	// save current end-of-table for return value
eot+=this_len;			// Increase current 'used' table length
if (n>=ct) n=NOTFND;
if (n>=0)
	{
	ret=(char *)get(n);
	memmove(&ret[this_len],ret,eot-(ret-dta)-this_len);
	}
else n=ct;
if (!len)								// If adding entry to variable-length (len=0) DYNAG,
	{									// bump up offsets in any following entries in Slave
	int offset=ret-dta;
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
		addr=&dta[n*len];
	else
		addr=&dta[*(int*)slave->get(n)];
	}
return(addr);
}

void DYNAG::del(int n)
{
char *p=(char *)get(n);
if (p)
	{
	int to=(int)(long(p)-long(dta)), this_len=len;
	ct--;
	if (!this_len)
		{
		this_len=strlen(p)+1;
		slave->del(n);
		for (int x=n;x<ct;x++) *(int*)slave->get(x)-=this_len;
		}
	if ((n=eot-(to+this_len))>0) memmove(&dta[to],&dta[to+this_len],n);
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

// If called with data!=NULL AND sz!=0, release any existing cargo & return allocated ptr -> copy of passed data
// !!! Shouldn't currently be called with data!=NULL and sz==0 !!!
// If called with data==NULL and sz!=0, set _cargo to zeroised allocated memory of specified size 
// If called with data!=NULL but sz==0 (i.e. - not passed), just return the current cargo pointer
void* DYNAG::cargo(const void *data, int sz)
{
if (sz)
	{
	Scrap(_cargo);
	if (data) _cargo=memadup(data,sz);
   else _cargo=memgive(sz);
   cargo_sz=sz;
	}
return(_cargo);
}



DYNTBL::DYNTBL(int _sz, PFI_v_v _cp, int _ct):DYNAG(_sz,_ct)
{
cp=_cp;
}

int DYNTBL::set_cp(PFI_v_v _cp)
{
qsort(dta,ct,len,cp=_cp);
for (int i=0; i<ct-1; i++)
	if ( (cp)(&dta[i*len],&dta[(i+1)*len])>=0)
		return(NO);	// ??? - there must be a duplicate key!
return(YES);
}

void* DYNTBL::find(const void *k)
{
return(get(in(k)));
}

void* DYNTBL::find_or_add(const void *k)
{
void *p=get(in(k));
if (p==NULL) p=put(k);
return(p);
}


int DYNTBL::in(const void *k)
{
if (!len) return(DYNAG::in(k));
return(in_table(0,k,dta,ct,len,cp));
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
int p, exact=in_table(&p,k,dta,ct,len,cp);	// If exact key doesn't exist, 'p' = position it would be at (i.e. BK_GE)
if (exact==NOTFND && p==ct) p=NOTFND;
return(p);
}

static const char *_aaa, *_kkk;
static int  cp_slave(int *aa, int *bb)
{
return(strcmp((*aa==NOTFND)?_kkk:&_aaa[*aa],(*bb==NOTFND)?_kkk:&_aaa[*bb]));
}

int DYNAG::find_str(int *p, const void *k)
{
_aaa=dta;
_kkk=(const char*)k;
int fix=-1;
int exact=in_table(p,&fix,slave->dta,ct,4,(PFI_v_v)cp_slave);	// If exact key doesn't exist, 'p' = position it would be at (i.e. BK_GE)
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
if (!cp || in_table(&p,k,dta,ct,len,cp)==NOTFND)	// 050108 - just 'append' if comparator not yet set
	return(DYNAG::put(k,p));
return(&dta[len*p]);
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
flput(get(0),len*ct,f);
flclose(f);
}

FDYNTBL::~FDYNTBL()
{
if (!no_save) save_to_disc();
}


PATHDYNAG::PATHDYNAG(const char *fullpath):DYNAG(0,0) // Split all the subdir legs within passed path into a DYNAG
{
char wrk[PATH_MAX], *p;
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

int PATHDYNAG::is_mount(void) // returns 1 if path starts with "mnt" or 2 if "media/<user>" ELSE ret=0
{
char *pth=(char*)get(0);
if (!strcmp(pth,"mnt")) return(1);
if (!strcmp(pth,"media")) return(2);
return(0);	
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
printf("\n%s\n\n",w);
sjhlog("%s",w);
throw 222;
}


int  cp_mem1(const void *a, const void *b) {return(memcmp(a,b,1));}
int  cp_mem2(const void *a, const void *b) {return(memcmp(a,b,2));}
int  cp_mem3(const void *a, const void *b) {return(memcmp(a,b,3));}
int  cp_mem4(const void *a, const void *b) {return(memcmp(a,b,4));}
int  cp_mem6(const void *a, const void *b) {return(memcmp(a,b,6));}
int  cp_mem8(const void *a, const void *b) {return(memcmp(a,b,8));}

int cp_short_v(short a, short b)	{return((a<b)?-1:(a>b));}
int cp_ushort_v(ushort a, ushort b){return((a<b)?-1:(a>b));}
int cp_long_v(int32_t a, int32_t b)		{return((a<b)?-1:(a>b));}
int cp_ulong_v(uint32_t a, uint32_t b)	{return((a<b)?-1:(a>b));}

int cp_short(const void *a, const void *b)
{return((*((short*)a)<*((short*)b))?-1:(*((short*)a)>*((short*)b)));}

int cp_ushort(const void *a, const void *b)
{return((*((ushort*)a)<*((ushort*)b))?-1:(*((ushort*)a)>*((ushort*)b)));}

int cp_long(const void *a, const void *b)
{return((*((int32_t*)a)<*((int32_t*)b))?-1:(*((int32_t*)a)>*((int32_t*)b)));}

int cp_ulong(const void *a, const void *b)
{return((*((uint32_t*)a)<*((uint32_t*)b))?-1:(*((uint32_t*)a)>*((uint32_t*)b)));}

int cp_int64_t(const void *a, const void *b)
{return((*((int64_t*)a)<*((int64_t*)b))?-1:(*((int64_t*)a)>*((int64_t*)b)));}

static int  cp_ulongn(const uint32_t *a, const uint32_t *b, int n)
{
int i,cmp;
for (i=cmp=0;!cmp && i<n;i++) cmp=cp_ulong_v(a[i],b[i]);
return(cmp);
}

int  cp_ulong2(const void *a, const void *b) {return(cp_ulongn((uint32_t*)a,(uint32_t*)b,2));}
int  cp_ulong4(const void *a, const void *b) {return(cp_ulongn((uint32_t*)a,(uint32_t*)b,4));}

static int  cp_shortn(const short *a, const short *b, int n)
{
int i,cmp;
for (i=cmp=0;!cmp && i<n;i++) cmp=cp_short_v(a[i],b[i]);
return(cmp);
}

int  cp_short2(const void *a, const void *b) {return(cp_shortn((short*)a,(short*)b,2));}

int  cp_str(char *a, char *b)
{
int cmp=strcmp(a,b);
if (cmp<0) return(-1);
if (cmp>0) return(1);
return(0);
}


static int tag_id;   // incrementing unique id assigned to each TaG object (DYNag/tbl) for leak checks


int leak_tracker(int start)   // Called with start=YES before running the code (whole program?) being monitored
{                             // Called AFTER the code has finished with start=NO (check all alloc's were freed)
#ifndef MEMLOG
return(0);
#endif
if (start)
	{
	return(NO);
	}
int mem_leak_ct;
mem_leak_ct=memtakeall();
if (mem_leak_ct==0) return(NO);  // no leaks!
   printf("MEMGIVE leaks:%d  First:%d\r\n", mem_leak_ct, first_mem_leak);
return(-77);
}

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
	if (blob[i]!=QTDOUBLE) {i++; continue;}
	for (j=1; blob[i+j]!=QTDOUBLE; j++)
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


#include <cmath>
#include <cstdint>

const double MIN_MB = 10.0;
const double MAX_GB = 10.0;
const int MIN_CVAL = 1;
const int MAX_CVAL = 250;

// Convert boundaries to bytes and use log() for the base values
const double MIN_SIZE_BYTES = MIN_MB * 1024.0 * 1024.0;
const double MAX_SIZE_BYTES = MAX_GB * 1024.0 * 1024.0 * 1024.0;

const double LOG_MIN = std::log(MIN_SIZE_BYTES);
const double LOG_MAX = std::log(MAX_SIZE_BYTES);
const double LOG_SPAN = LOG_MAX - LOG_MIN;
const double CVAL_SPAN = static_cast<double>(MAX_CVAL - MIN_CVAL);

// condense approximate (video) filesize into a value between 1 and 250 (1 <=10Mb, 250 >= 10Gb)
uchar condense_filesize(int64_t filesize)
{
if (filesize <= MIN_SIZE_BYTES) return MIN_CVAL;
if (filesize >= MAX_SIZE_BYTES) return MAX_CVAL;
// Scale the logarithmic value of the filesize to the 1-250 range
double normalized_log = (std::log(static_cast<double>(filesize)) - LOG_MIN) / LOG_SPAN;
double result = MIN_CVAL + (normalized_log * CVAL_SPAN);
// Round and clamp the result to ensure it's within the 1-250 range
return static_cast<uchar>(std::round(std::fmin(std::fmax(result, MIN_CVAL), MAX_CVAL)));
}

/* // expand condensed value 1-250 back to an approximate filesize in MB
short expand_filesize(uchar condensed_value)
{
if (condensed_value <= MIN_CVAL) return static_cast<short>(MIN_MB);
if (condensed_value >= MAX_CVAL) return static_cast<short>(1024.0 * MAX_GB);
// Scale the condensed value back to the logarithmic range
double normalized_cval = (static_cast<double>(condensed_value) - MIN_CVAL) / CVAL_SPAN;
double log_reconstructed = LOG_MIN + (normalized_cval * LOG_SPAN);
// Use exp() to convert back from the logarithmic value, then to MB
double bytes = std::exp(log_reconstructed);
return static_cast<short>(std::round(bytes / (1024.0 * 1024.0)));
}*/
int64_t expand_filesize(uchar condensed_value)
{
if (condensed_value <= MIN_CVAL) return static_cast<short>(MIN_MB);
if (condensed_value >= MAX_CVAL) return static_cast<short>(1024.0 * MAX_GB);
// Normalize the input condensed value to a 0.0-1.0 range
double normalized_cval = (static_cast<double>(condensed_value) - MIN_CVAL) / CVAL_SPAN;
// Scale this normalized value back to the logarithmic range of the file sizes
double log_reconstructed = LOG_MIN + (normalized_cval * LOG_SPAN);
// Use exp() to convert from the logarithmic value back to the actual byte value
double estimated_bytes = std::exp(log_reconstructed);
// Round the result to the nearest whole byte and cast to int64_t for accuracy
return static_cast<int64_t>(std::round(estimated_bytes));
}

