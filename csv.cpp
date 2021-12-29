#include <cstdio>
#include <string.h>

#include "pdef.h"
#include "memgive.h"
#include "str.h"
#include "flopen.h"
#include "smdb.h"
#include "csv.h"

CSV_READER::CSV_READER(const char *fn)
{
csv_title_fldno=csv_year_fldno=csv_emdb_num_fldno=csv_imdb_num_fldno=csv_rating_fldno=NOTFND;
f=flopen(fn,"R");
if (!f)
	{
	Xecho("Can't open %s\r\n", buf);
	return;
	}
readline();
if (parse_first_csv_line()) m_finish("bad csv file");
}



int CSV_READER::parse_first_csv_line(void)
{
int i;
char *s;
for (i=0;(s=vb_field(buf,i))!=NULLPTR;i++)
	{
	if (!strcmp(s,"Title")) csv_title_fldno=i;
	if (!strcmp(s,"Year")) csv_year_fldno=i;
	if (!strcmp(s,"No.")) csv_emdb_num_fldno=i;
	if (!strcmp(s,"IMDb Id")) csv_imdb_num_fldno=i;
	if (!strcmp(s,"User Rating")) csv_rating_fldno=i;
	}
if (csv_title_fldno==NOTFND || csv_year_fldno==NOTFND || csv_emdb_num_fldno==NOTFND || csv_imdb_num_fldno==NOTFND || csv_rating_fldno==NOTFND)
	return(NOTFND);
return(0); // no error
}

// change all occurences of "from" (i.e. - colon from emdb.csv) to "to" (TAB = standard vb_field separator)
// Return the number of changes made, which should be ONE LESS THAN TOTAL FIELDS in *.CSV (no separator after last one)
static void replace_separators(char *str, char from, char to)
{
int i, sepct;
if ((i=str[strlen(str)-1])!=to && i!=from) strendfmt(str,"%c",to);	// Make sure the LAST field is tab-terminated
for (sepct=0;(i=stridxc(from,str))!=NOTFND;sepct++) str[i]=to; // so we can parse with vb_str() WATCH OUT FOR SEMICOLON INSTEAD OF COLON!!! (set this in EMDB config)
if (sepct<5) m_finish("Too many separators!"); // 16/10/20 - don't care if there are more fields
}


int CSV_READER::readline(void)
{
int sz;
if ((sz=flgetln(buf,500,f)) < 10) return(0);
replace_separators(buf, ';', TAB);
return(sz);
}

int CSV_READER::fill_tbl(DYNTBL *emk)
{
while (readline())
	{
	EM_KEY e;
//if (SAME4BYTES(buf,"188 ")) bugger(ct);
	if (str2EM_KEY(&e))
		{Xecho("Bad Movie Name %s\r\n",buf);return(0);}
	if (emk->in(&e)!=NOTFND)
		{Xecho("Dup Movie Name %s\r\n",buf);return(0);}
	emk->put(&e);
	}
return(emk->ct);
}

CSV_READER::~CSV_READER()
{
flclose(f);
};


// MovieNo:Title:Year:UserRating
int CSV_READER::str2EM_KEY(EM_KEY *e)
{
char w[100];
int i,len;
memset(e,0,sizeof(EM_KEY));
e->emdb_num=a2i(vb_field(buf,csv_emdb_num_fldno),0);
e->imdb_num=a2l(vb_field(buf,csv_imdb_num_fldno),0);
strtrim(strcpy(w,vb_field(buf,csv_title_fldno)));
if ((len=strlen(w))>=60) return(3);	// ERROR! - Only allowed 60 chars (incl terminating \0) for name (without year)
e->year=a2i(vb_field(buf,csv_year_fldno),0);
strcpy(e->nam,w);
char *p=vb_field(buf,csv_rating_fldno);
if (p)	// If last field (rating) = NULLPTR, it's not present. Skip decoding and leave it initialised to 0
	{
	i=stridxc('.',p); if (i!=NOTFND) strdel(&p[i],1);
	e->rating=a2i(p,0);
	}
return(NO); // No error
}


