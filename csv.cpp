#include <cstdio>
#include <string.h>

#include "pdef.h"
#include "memgive.h"
#include "str.h"
#include "flopen.h"
#include "smdb.h"
#include "csv.h"
#include "cal.h"
#include "log.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>


struct NAMP1 {char *name, len;};
struct NAMPTR {NAMP1 fore, last;};

static char *comma_or_eos(char *a)
{
while (*a) if (*a==COMMA) break; else a++;
return(a);
}

static void sjhlog_fixlen(const char *txt, const char *n, int l)
{
char w[20];
strfmt(w,"%s[%c%d.%ds]",txt,'%',l,l);
sjhlog(w,n);
}

// Passed NAMPTR last.name="(voice)"  Step it back to immediately preceding REAL surname
static void step_back_over_voice(NAMPTR *n)
{
char *f=n->fore.name;			// ptr -> forename
char *s=&f[n->fore.len-1];	// initially ptr -> forename last char, will become ptr -> surname
while (*s==SPACE) s--;			// step back over any trailing spaces
char *e=s;							// ptr -> last char of forename (will become last char of lastname)
while (s!=f && *(s-1)!=SPACE) s--;	// step back to start of 'real' surname
if (s!=f)
	{
	int l=s-f;
	while (l>0 && f[l-1]==SPACE) l--;
	n->fore.len=l;
	}
n->last.name=s;
n->last.len=e-s+1;
//sjhlog("flen=%d slen=%d",n->fore.len,n->last.len);
//sjhlog_fixlen("f",n->fore.name,n->fore.len);
//sjhlog_fixlen("s",n->last.name,n->last.len);
}

// Fill in passed NAMPTR with up to 5 forename+lastnane  pointer pairs
static void fill_np(char *a, NAMPTR *np)
{
//char *aa=a;
//static int again=NO;

int n,e, sp;
for (n=0;n<5;n++)
    {
    np[n].fore.name=a;          // we're looking at the forename first
    sp = e = (comma_or_eos(a) - a);  // points to first (unwanted) char after lastname
    while (sp>0 && a[sp]!=SPACE) sp--;
    np[n].last.name=&a[sp+(a[sp]==SPACE)];
    np[n].last.len = e-sp;
    if (np[n].last.len) np[n].last.len--;
    np[n].fore.len = sp;
    a=np[n].last.name + np[n].last.len;
    while (*a==COMMA || *a==SPACE) a++;

	if (np[n].last.len==7 && memcmp(np[n].last.name,"(voice)",7)==0)
		{
//		sjhlog("aa[%s]",aa);
//		sjhlog_fixlen("F",np[n].fore.name,np[n].fore.len);
		step_back_over_voice(&np[n]);
		}

    }
}

static int cp_namp1(NAMP1 *a, NAMP1 *b)
{
if (a->len==0 && b->len==0) return(0);
int len=MIN(a->len,b->len), cmp=0;
if (len) cmp=memcmp(a->name,b->name,len);
if (!cmp) cmp=memcmp(&a->len,&b->len,1);
return(cmp);
}

int cp_name(char *a, char *b)
{
NAMPTR anp[5], bnp[5];
fill_np(a,anp);
fill_np(b,bnp);
int cmp,n;
for (n=cmp=0; n<5 && !cmp; n++)
    if ((cmp=cp_namp1(&anp[n].last, &bnp[n].last))==0)
        cmp=cp_namp1(&anp[n].fore, &bnp[n].fore);
return(cmp);
}



long get_filedate(const char *filename)
{
struct stat attrib;
stat(filename, &attrib);
return(attrib.st_mtime);
}

#ifdef unused
CSV_READER::CSV_READER(const char *fn)
{
	csv_title_fldno = csv_year_fldno = csv_emdb_num_fldno = NOTFND;
	csv_imno_fldno = csv_rating_fldno = csv_director_fldno = NOTFND;
    csv_added_fldno = csv_seen_fldno = csv_filesz_fldno = csv_cast_fldno = csv_runtime_fldno = NOTFND;
    starring=0;	// Table not created unless fill_starring() is called
	f = flopen(fn, "R");
	if (!f)
	{
	sjhlog("Can't open %s", fn);
		return;
	}
	readline();
	if (parse_first_csv_line() != 0)
		m_finish("bad csv file");
	filedate=short_bd(get_filedate(fn));
}

int CSV_READER::parse_first_csv_line(void)
{
	int i;
	char *s;
	for (i = 0; (s = vb_field(buf, i)) != NULLPTR; i++)
		{
		if (!strcmp(s, "Title"))
			csv_title_fldno = i;
		if (!strcmp(s, "Year"))
			csv_year_fldno = i;
		if (!strcmp(s, "No."))
			csv_emdb_num_fldno = i;
		if (!strcmp(s, "IMDb Id"))
			csv_imno_fldno = i;
		if (!strcmp(s, "User Rating"))
			csv_rating_fldno = i;
		if (!strcmp(s, "Director(s)"))
			csv_director_fldno = i;
		if (!stricmp(s, "Date added"))
			csv_added_fldno = i;
		if (!strcmp(s, "Seen"))
			csv_seen_fldno = i;
		if (!stricmp(s, "File size"))
			csv_filesz_fldno = i;
        if (!stricmp(s, "Cast"))
            csv_cast_fldno = i;
        if (!stricmp(s, "Runtime"))
            csv_runtime_fldno = i;
        }
	if (csv_title_fldno == NOTFND || csv_year_fldno == NOTFND || csv_emdb_num_fldno == NOTFND || csv_imno_fldno == NOTFND || csv_rating_fldno == NOTFND)
		return (NOTFND);
	return (0); // no error
}

// change all occurences of "from" (i.e. - colon from emdb.csv) to "to" (TAB = standard vb_field separator)
// Return the number of changes made, which should be ONE LESS THAN TOTAL FIELDS in *.CSV (no separator after last one)
static void replace_separators(char *str, char from, char to)
{
	int i, sepct;
	if ((i = str[strlen(str) - 1]) != to && i != from)
		strendfmt(str, "%c", to); // Make sure the LAST field is tab-terminated
	for (sepct = 0; (i = stridxc(from, str)) != NOTFND; sepct++)
		str[i] = to; // so we can parse with vb_str() WATCH OUT FOR SEMICOLON INSTEAD OF COLON!!! (set this in EMDB config)
	if (sepct < 5)
		m_finish("Too few separators!"); // 16/10/20 - don't care if there are more fields
}

int CSV_READER::readline(void)
{
int sz;
if ((sz = flgetln(buf, 500, f)) < 10) return (NO);
if (*(strend(buf)-1) != TAB) strendfmt(buf,"%c",TAB);
return (YES);
}

int CSV_READER::fill_tbl(DYNTBL *emk) // Table recsiz may include 'director'
{
while (readline())
	{
	EM_KEY1 e;
	if (str2EM_KEY(&e))
		{
		sjhlog("Bad Movie Name %s\r\n", buf);
		return (0);
		}
	if (emk->in(&e) != NOTFND)
		{
		sjhlog("Duplicate movie (IMDb number %d) %s %4d\r\n", e.e.imno, e.e.nam, e.e.year);
		return (0);
		}
	emk->put(&e);
	}
return (emk->ct);
}

CSV_READER::~CSV_READER()
{
	flclose(f);
};

static int csv_date(char *buf, int fldno, int filedate)
{
char w[32];
strtrim(strcpy(w, vb_field(buf, fldno)));
if (!stricmp(w,"Today")) return(filedate);
if (!stricmp(w,"Yesterday")) return(filedate-1);
if (w[0]=='Y') return(1);	// Dummy 'seen'. value to be replaced by 'added'
return(cal_parse(w));
}

static int csv_filesz(char *buf, int fldno)
{
	int g10 = 0;
	char w[32];
	strtrim(strcpy(w, vb_field(buf, fldno)));
	if (stridxs("MB", w) != NOTFND)
		g10 = a2i(w, 0) / 100;
	else if (stridxs("GB", w) != NOTFND)
	{
		g10 = a2i(w, 0) * 10;
		int i = stridxc('.', w);
		if (i != NOTFND)
			g10 += a2i(&w[i + 1], 1); // only first digit after decimal point
	}
	return (g10);
}

#include <ctype.h>

static char *force_ansi(char *s)
{
int i;
for (i=0; s[i]; i++)
	{
if (s[i]==-61 && s[i+1]==-87) strdel(&s[i],1);
	switch (s[i])
		{
		case -59: case -63: case -64: case -65: s[i]='A'; break;
		case -27: case -28: case -32: case -31: s[i]='a'; break;
		case -33: s[i]='b'; break;
		case -25: s[i]='c'; break;
		case -55: s[i]='E'; break;
		case -87: case -21: case -24: case -23: case -26: s[i]='e'; break;
		case -20: case -19: case -18: case -17: s[i]='i'; break;
		case -15: s[i]='n'; break;
		case -45: case -44: case -42: s[i]='O'; break;
		case -30: case -8:  case -12: case -13: case -10: case -16: s[i]='o'; break;
		case -7:  case -6:  case -5:  case -4:  s[i]='u'; break;
        case -36: s[i]='U'; break;
        case -3: s[i]='y'; break;
		case -110: s[i]=39; break;
		}
	if (s[i]<0)
		sjhlog("[%s] contains unexpected character [%c] CHR(%d)", s,s[i],s[i]);
	}
return(s);
}

static char *complete_names_only(char *w,int sz)
{
int i;
while ((i = strlen(strtrim(w))) >= sz)
	{
	int c=strridxc(COMMA,w);
	if (c==NOTFND) c=i-1;	// If no comma, remove just LAST ONE char from right-hand
	w[c] = 0;					// If there WAS a comma (multiple names) delete all of rightmost name
	}
return(w);	// Complete names only - as many as will fit into 'sz' buffer with EOS nullbyte
}

int CSV_READER::str2EM_KEY(EM_KEY1 *e)
{
	char w[200];
	int i, len;
	memset(e, 0, sizeof(EM_KEY));
	e->e.emdb_num = a2i(vb_field(buf, csv_emdb_num_fldno), 0);
	e->e.imno = a2l(vb_field(buf, csv_imno_fldno), 0);
	strtrim(strcpy(w, vb_field(buf, csv_title_fldno)));
	e->e.year = a2i(vb_field(buf, csv_year_fldno), 0);
	strcpy(e->e.nam, force_ansi(w));
	if (csv_director_fldno != NOTFND)
		strcpy(e->director, complete_names_only(force_ansi(strcpy(w, vb_field(buf, csv_director_fldno))),30));
	if (csv_added_fldno != NOTFND)
		e->added = csv_date(buf, csv_added_fldno, filedate);
	if (csv_seen_fldno != NOTFND)
		e->seen = csv_date(buf, csv_seen_fldno, filedate);

	if (csv_filesz_fldno != NOTFND)
		e->filesz = csv_filesz(buf, csv_filesz_fldno);

	strtrim(strcpy(w, vb_field(buf, csv_rating_fldno)));
	i = stridxc('.', w);
	if (i != NOTFND)
		strdel(&w[i], 1);
	e->e.rating = a2i(w, 0);
	if (csv_cast_fldno != NOTFND)
		{
		char *cc=vb_field(buf, csv_cast_fldno);
		if (cc==0) e->cast[0]=0;
		else
			{
			force_ansi(strcpy(w, cc));
            if (starring) biggest_stars_first(w);
			strcpy(e->cast, complete_names_only(w,60));
			}
		}
	if (csv_runtime_fldno != NOTFND)
		e->runtime = a2i(vb_field(buf, csv_runtime_fldno), 0);

	if (e->seen<=1 && e->e.rating!=0) e->seen=e->added;	// Assume watched when added to library, if undated rating
	return (NO); // No error
}

static char *add_actor(DYNTBL *a, char *n)
{
int i, len, comma=NO;
NAMCT w, *_w;
for (i=len=0;n[i];i++)
    {
    comma=(n[i]==COMMA);
    if (comma) break;
    if (i>=29) m_finish("Name %s too long",n);
    w.nam[len++]=n[i];
    }
w.nam[len]=w.ct=0;
if (len) ((NAMCT*)a->get(a->in_or_add(&w)))->ct++;
n=&n[len+comma];   // default return = empty string UNLESS this name ended with a comma
while (*n==SPACE) n++;
return(n);
}

void CSV_READER::str2starring(DYNTBL *a)
{
char w[200];
char *cc=vb_field(buf, csv_cast_fldno);
if (cc==0 || !*cc) return;
force_ansi(strcpy(w, cc));
for (cc=w; *cc!=0; cc=add_actor(a,cc)) {;}
}


int _cdecl cp_namct(NAMCT *a, NAMCT *b) {return(cp_short(&a->ct, &b->ct));}
int _cdecl cp_str(char *a, char *b)
{
int cmp=strcmp(a,b);
if (cmp<0) return(-1);
if (cmp>0) return(1);
return(0);
}

void CSV_READER::biggest_stars_first(char *w)
{
int i,j, k;
DYNTBL a(sizeof(NAMCT),(PFI_v_v)cp_str);	// Just the actors in THIS movie
for (char *cc=w; *cc!=0; cc=add_actor(&a,cc)) {;}

if (!a.ct) {sjhlog("[%s]\r\nNo actors!",buf); return;}

int sz=a.ct*sizeof(NAMCT);
NAMCT *nc1=(NAMCT*)memgive(sz);
k=0;
for (j=a.ct; j--; )
	{
	char *ky=(char*)a.get(j);
	i=starring->in_GE(ky);
	if (i<0) {sjhlog("Lookup failed on key [%s]", ky); throw(98);}
	memcpy(&nc1[k],starring->get(i),sizeof(NAMCT));
	k++;
	}
qsort(nc1,a.ct,sizeof(NAMCT),(PFI_v_v)cp_namct);
int more=w[0]=0;
for (j=a.ct; j--; more=YES) strendfmt(w,"%s%s",more?", ":"", nc1[j].nam);
}

void CSV_READER::fill_starring(void) // Table recsiz may include 'director'
{
starring = new DYNTBL (sizeof(NAMCT),(PFI_v_v)cp_str);
while (readline()) str2starring(starring);

// reposition input file back to second line, ready for main 'load' process...
strcpy(buf,flnam(f));
flclose(f);
f = flopen(buf, "R");
readline();
}
#endif
