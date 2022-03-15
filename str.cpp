#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <cstdarg>


#include "pdef.h"
#include "str.h"
#include "memgive.h"

int a2err;			// 0 = a2l() succeeded, 1 = a2l() failed (hit non-digit as stored in a2err_char), 2 = failed with empty i/p string
int	a2err_char;

long rnd_seed;
short rnd(short lo,short hi)
{
if(hi<=lo)return lo;
rnd_seed = 1664525L*rnd_seed + 907612489L;
return((short)((double)((rnd_seed>>1)&0x7fffffff)/2147483648.0*(hi-lo+1)+lo));
}


int short_overlap(short *a, short *b)
{
if (*((int*)a) && *((int*)b))					// If EITHER range is 0-0 it MUST be an overlap!
	if (a[0]>b[1] || a[1]<b[0]) return(NO);		// (else it's a pair of scalar ranges to be checked)
return(YES);
}


bool in_rng(short val, short*rng)
{
return(val>=rng[0] && val<=rng[1]);
}

int force_into_range(int value, int min, int max)
{
if (max<min) max=min;
if (value<min) value=min;
if (value>max) value=max;
return(value);
}

void set_maxc(char *hi, int v)
{if (v>*hi) *hi=(char)v;}

int  highest_code_in_string(char *p)
{
int i,h;
for (h=i=0;p[i];i++) if (p[i]>h) h=p[i];
return(h);
}


void set_maxc_str(char *hi, char *str)
{set_maxc(hi,strlen(str));}

// Return number of chars needed to store value 'v',
// including one extra if negative
int  digits(double v)
{
long	d;
int i=(v<0);
do	{
	d=(long)(v/=10.0);
	i++;
	} while (d!=0);
return(i);
}

void bit8_set_one_bit(char *mask, int bitnum)
{
BIT8_SET0(mask);
bit_set(mask,bitnum);
}

// If bitmap 'm' has only ONE bit set, return that subscript, else return NOTFND
int bit_only1(char *m, int bit_ct)
{
int i,on=NOTFND;
for (i=0;i<bit_ct;i++)
	if (bit_test(m,i))
		{
		if (on==NOTFND) on=i;
		else return(NOTFND);
		}
return(on);
}

void	bit_set(char *m,int b) {m[b>>3]|=(1<<(b&7));}
void	bit_unset(char *m,int b) {m[b>>3]&=~(1<<(b&7));}
int		bit_test(const char* m, int b) {return((m[b>>3]&(1<<(b&7)))!=0);}

BitMap::BitMap(int _ct)
{bm=(char*)memgive(((ct=_ct)+7)/8);};

BitMap::~BitMap()
{memtake(bm);};

void BitMap::clear_all(void)
{memset(bm,0,(ct+7)/8);};

void BitMap::set(int b)
{bit_set(bm,b);};

void BitMap::unset(int b)
{bit_unset(bm,b);};

int BitMap::is_on(int b)
{return(bit_test(bm, b));};

int BitMap::first(int on_wanted)
{
if (on_wanted) on_wanted=true;
for (int b=0; b<ct ; b++)			// Either we found a 4-byte int containing req'd bit setting, or looking at last 'int'
	if (is_on(b)==on_wanted)
		return(b);	// (if there's a bit with the setting we want, return the subscript)
return(NOTFND);	// If we get here, no bits have the wanted setting
};

int BitMap::Bit_only1(void)
{return(bit_only1(bm,ct));}


// As strancpy(), but discards characters from MIDDLE rather than END if src is too long
char *strancpy_outer(char *dst, const char *src, int bufsz)
{
int len=strlen(src);
if (len<bufsz || bufsz<7) return(strancpy(dst,src,bufsz));
bufsz-=6;				// Allow for "[...]" replacing non-copied chars in middle of o/p, PLUS EOS
int	part1=(bufsz+1)/2;	// (can't be less than 1 char to be copied to 'part1' of output string)
memcpy(dst,src,part1);
strcpy(&dst[part1],"[...]");
return(strcat(dst,&src[len-(bufsz-part1)]));	// (append at least EOS to o/p, more if there's room)
}


/*
extern	int dbsafe;
void m_finish(const char *fmt,...)		// run debugger to HERE if untrapped error
{
va_list va;
va_start(va,fmt);
seterr(fmt,va);
dbsafe=NOTFND;				// Stops dbsafeclose() from marking databases as 'clean' when closing
throw(SE_USER_CANCEL);
}
*/

// This is NOT the same as the ANSI standard strncpy(), which DOESN'T
// put a null-byte at the end of destination string if source is too long
char	*strancpy(char *dst, const char *src, int bufsz)
{
int i;
for (i=0; i<bufsz-1 && src[i]; i++)
	dst[i]=src[i];
dst[i]=0;
return(dst);
}

// Copy from 'src' to 'dst' up to, but not including, the character 'up2',
// or all chars if 'up2' not present. Return 'dst' terminated with a nullbyte
char *stracpy2(char *dst,const char *src, char up2)
{
int i=0,c;
while ((c=src[i])!=0 && c!=up2) dst[i++]=(char)c;
dst[i]=0;
return(dst);
}



char *strcpy_trim(char *op, const char *ip, int maxlen)
{
int i, pos, c;
for (i=pos=0;i<maxlen;i++)
	if ((c=ip[i])>' ' || pos)
		op[pos++]=(char)c;
do op[pos--]=0;
	while (pos>=0 && op[pos]<=' ');
return(op);
}


// Return pointer to element 'n' in comma-separated list, or an empty string if there aren't that many elements
const char *str_field(const char *str, int n)	// NOTE - n=1 for the first item!
{
int	comma;
const char	*s;
for (s=str;--n;s=&s[comma+1])
	if ((comma=stridxc(COMMA,s))==NOTFND) return("");
return(s);
}


// get/put _binary124 use Intel "backwords" native binary storage mode
Ulong get_binary124(const void *addr, int len, int sub)
{
switch (len)
	{
	case 1: return(((char*)addr)[sub]);
	case 2: return(((ushort*)addr)[sub]);
	default:return(((long*)addr)[sub]);
	}
}

void put_binary124(void *addr, Ulong value, int len, int sub)
{
switch (len)
	{
	case 1: ((char*)addr)[sub]=(char)value; break;
	case 2: ((ushort*)addr)[sub]=(ushort)value; break;
	default: ((Ulong*)addr)[sub]=value;
	}
}

ushort r2i(const char *s)
{
return((ushort) ((((ushort)s[0])<<8)|s[1]));
}

short *r2i2(short *i, const char *r)
{
i[0]=r2i(r);
i[1]=r2i(&r[2]);
return(i);
}


Ulong r2l(const char *s)
{
return ((((long)s[0])<< 24) | (((long)s[1])<< 16) | (((short)s[2])<<8) | s[3]);
}

Ulong r2long(char *ad, int len)
{
switch (len)
	{
	case 1: return(*ad);
	case 2: return(r2i(ad));
	default:return(r2l(ad));
	}
}


Ulong a2l(const char *s,int ct)
{
int i,c;
Ulong result;
result = i = 0;
if (!ct) ct = strlen(s);
while (s[i]==' ' && ct) {ct--;i++;}
a2err = 2; a2err_char=0;
while (s[i] && ct--)
	{
	if ((a2err = !(((c = s[i++]-'0') >= 0 ) && (c <=9 ))) == 0)
		result = result*10 + c;
	else
		{
		a2err_char=c+'0';
		break;
		}
	}
return(result);
}


int a2l_signed(const char *str, int len)
{
if (str[0]=='-')
	return(-(int)a2l(&str[1],len?(len-1):0));
return(a2l(str,len));
}

ushort a2i(const char *s,int ct){return (ushort)a2l(s,ct);}

char *l2r(Ulong n)
{
static char c[4];
c[0] = (char)(n >> 24);
c[1] = (char)(n >> 16);
c[2] = (char)(n >> 8);
c[3] = (char)n ;
return c;
}

char *i2r(ushort n)
{
static char c[2];
c[0] = (char)(n >> 8);
c[1] = (char)n ;
return c;
}


char *_strfmt(char*s1 ,const char*s2,va_list va)
{
static char w[512];
if (!s1) s1=w;
vsprintf(s1,s2,va);
return s1;
}

char *_strnfmt(char*s1,int n, const char*s2, va_list va)
{
static char w[512];
if (!s1) s1=w;
vsnprintf(s1,n,s2,va);
return s1;
}

char *strfmt(char *str ,const char *fmt,...)
{
va_list va;
va_start(va,fmt);
return _strfmt(str,fmt,va);
}

char *strnfmt(char *str, int n, const char *fmt,...)
{
va_list va;
va_start(va,fmt);
return _strnfmt(str,n,fmt,va);
}


int stridxc(char c,const char *s)
{
const char *p=strchr(s,c);
return(p?p-s:NOTFND);
}

int strridxc(char c,const char *s)	// Offset of RIGHTMOST (if any) instance of 'c' in 's'
{
const char *p=strrchr(s,c);
return(p?p-s:NOTFND);
}


char	*strxlt(char *str, char from, char to)
{
int i;
while ((i=stridxc(from,str))!=NOTFND) str[i]=to;
return(str);
}

char *strend(const char *s)
{
return (char *)&s[strlen(s)];
}


char *stradup(const char *s)
{
return (strcpy((char*)memgive(strlen(s)+1),s));
}

char *strinsc(char *str,int chr)	// Insert chr at start of str
{
memmove(str+1,str,(strlen(str)+1));
*str=(char)chr;
return(str);
}

int strinspn(const char *sub, const char *str)
{
int i;
for(i = 0; str[i];i++)	
	if(stridxc(str[i],sub)!=NOTFND)
		return i;
return NOTFND;
}

char *strins(char *str, const char *ins)	// Insert ins at start of str
{
int	inslen=strlen(ins);	/* length of insert string */
if	(inslen > 0) 
	{
	memmove(str+inslen,str,(strlen(str)+1));/* move original */
	/* moved an extra byte to make sure 3rd arg non-zero */
	memmove(str,ins,inslen); /* move the insert */
	}
return(str);
}


// why not test the whole of 'sub'? !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
int	stridxs(const char *sub, const char *str)	// Find offset of sub in str
{
int	offset,sp=0;
while	( (offset=stridxc(*sub,&str[sp])) != NOTFND )
	if (!memcmp(&sub[1],&str[sp+=offset+1],strlen(&sub[1]))) return(sp-1);
return(NOTFND);											// 0 or >0 if rest matches
}

char *strdel(char *str, int ct)
{
int len=strlen(str);
if (ct>len) ct=len;
return (char *)(memmove(str,&str[ct],len-ct+1));
}


void strendfmt(char *s, const char *ctl,...)
{va_list va; va_start(va,ctl); _strfmt(strend(s),ctl,va);}

static char *strpwhit(char* str)
{
while((*str == TAB)||(*str == SPACE))str++;
return str;
}

static char *strrtrim(char *s)
{
int i,c;
for (i=strlen(s);--i>=0;s[i]=0)
	if ((c=s[i])!=TAB && c!=SPACE)
		break;
return s;
}

char *strtrim(char *str)				// Remove leading & trailing white space
{
char *sss=strrtrim(strpwhit(str));
memmove(str, sss, strlen(sss)+1 );
return(str);
}


short	memidxw(short val,const short *ary,short tsize)
{
for (int i=0; i<tsize; i++) if (ary[i]==val) return(i);
return(NOTFND);
}



// Create a null-terminated string with all chars set to 'fillchr' 
char	*strfill(char *str,int width,int fillchr)
{
str[width]=0;
while (width--)
	str[width]=(char)fillchr;
return(str);
}

// left/right/centre justify a string in situ, using 'chr' to pad as required
char	*strjust(char *s,int wid,int typ,int chr)	// TRUNCATE THE STRING IF IT'S LONGER THAN 'WIDTH'
{
int sw;
for (sw=s[wid]=0;strlen(s)<(unsigned)wid;sw=!sw)
	if (typ==STR_LJUST || (typ==STR_CENTER && sw)) strcat(s,(char*)&chr);
	else strinsc(s,chr);
return(s);
}

// right-justify with prepended blanks			// DOESN'T TRUNCATE THE STRING IF IT'S LONGER THAN 'WIDTH'
char	*strrjust(char *str, int width)
{
while (strlen(str)<(unsigned)width) strinsc(str,' ');
return (str);
}

static int next_delimiter(int chr, const char *str)
{
int p;
if ((p=stridxc((char)chr,str))==NOTFND)
	{
	m_finish("Internal error 800 finding CHR(%d)",chr);
	}
return(p);
}

// Return pointer to null-terminated static copy of \t-delimited field 'n' within VB text-format record structure 'rec'
char *vb_field(const char *rec, int n)
{
static char fld[200];
const char *prv=rec;
int len=0;
while (n>=0)
	{
	len=next_delimiter('\t',rec);
	if (len>=sizeof(fld)) m_finish("Internal error 801 field too long [%s]",prv);
	if (n--) rec+=(len+1); else break;
	if (!*rec) return(0);	// Added check to avoid going past end, 7/2/19
//	if (!*rec) {len=0;break;}	// Feb 2022 - return empty string, not nullptr
	}
if (len) memmove(fld,rec,len);
fld[len]=0;
return(fld);
}


