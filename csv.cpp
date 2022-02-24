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

long get_filedate(const char *filename)
{
struct stat attrib;
stat(filename, &attrib);
return(attrib.st_mtime);
}

CSV_READER::CSV_READER(const char *fn)
{
	csv_title_fldno = csv_year_fldno = csv_emdb_num_fldno = NOTFND;
	csv_imdb_num_fldno = csv_rating_fldno = csv_director_fldno = NOTFND;
	csv_added_fldno = csv_seen_fldno = csv_filesz_fldno = csv_cast_fldno = NOTFND;
	f = flopen(fn, "R");
	if (!f)
	{
		Xecho("Can't open %s\r\n", fn);
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
			csv_imdb_num_fldno = i;
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
		}
	if (csv_title_fldno == NOTFND || csv_year_fldno == NOTFND || csv_emdb_num_fldno == NOTFND || csv_imdb_num_fldno == NOTFND || csv_rating_fldno == NOTFND)
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
	if ((sz = flgetln(buf, 500, f)) < 10)
		return (0);
	replace_separators(buf, ';', TAB);
	return (sz);
}

int CSV_READER::fill_tbl(DYNTBL *emk) // Table recsiz may include 'director'
{
	while (readline())
	{
		EM_KEY1 e;
		if (str2EM_KEY(&e))
		{
			Xecho("Bad Movie Name %s\r\n", buf);
			return (0);
		}
		if (emk->in(&e) != NOTFND)
		{
			Xecho("Dup Movie Name %s\r\n", buf);
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

static void force_ansi(char *s)
{
int i;
for (i=0; s[i]; i++)
	{
	switch (s[i])
		{
		case -59: case -63: case -64: case -65: s[i]='A'; break;
		case -27: case -28: case -32: case -31: s[i]='a'; break;
		case -33: s[i]='b'; break;
		case -25: s[i]='c'; break;
		case -55: s[i]='E'; break;
		case -21: case -24: case -23: case -26: s[i]='e'; break;
		case -20: case -19: case -18: case -17: s[i]='i'; break;
		case -15: s[i]='n'; break;
		case -44: case -42: s[i]='O'; break;
		case -30: case -8:  case -12: case -13: case -10: case -16: s[i]='o'; break;
		case -36: s[i]='U'; break;
		case -7:  case -6:  case -5:  case -4:  s[i]='u'; break;
		case -3: s[i]='y'; break;
		case -110: s[i]=39; break;
		}
	if (s[i]<0)
		sjhlog("[%s] contains unexpected character [%c] CHR(%d)", s,s[i],s[i]);
	}
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
	e->e.imdb_num = a2l(vb_field(buf, csv_imdb_num_fldno), 0);
	strtrim(strcpy(w, vb_field(buf, csv_title_fldno)));
//	if ((len = strlen(w)) >= 60)
//		return (3); // ERROR! - Only allowed 60 chars (incl terminating \0) for name (without year)
	e->e.year = a2i(vb_field(buf, csv_year_fldno), 0);
	strcpy(e->e.nam, w);
	if (csv_director_fldno != NOTFND)
		strcpy(e->director, complete_names_only(strcpy(w, vb_field(buf, csv_director_fldno)),30));
	if (csv_added_fldno != NOTFND)
		{
		e->added = csv_date(buf, csv_added_fldno, filedate);
		}
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
		if (cc==0) e->cast[0]=0; else
		strcpy(e->cast, complete_names_only(strcpy(w, cc),60));
		}
force_ansi(e->e.nam);
force_ansi(e->director);
force_ansi(e->cast);
if (e->seen<=1 && e->e.rating!=0) e->seen=e->added;	// Assume watched when added to library, if undated rating
	return (NO); // No error
}

