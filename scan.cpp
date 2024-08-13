#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdef.h"
#include "memgive.h"
#include "str.h"
#include "imdb.h"
#include "imdbf.h"
#include "scan.h"

SCAN_ALL::SCAN_ALL(void)	// Class to handle 'cached' values gotten by api calls
{									// Comstructor uses IMDB_FLD object to load EVERYTHING into memory
int i;							// app then calls get(imno, fld_id, op_buf) for each specific values
for (fct=0;idx_fid[fct]!=0;fct++) *(short*)&idx[fct]=idx_fid[fct];
_ff=(FCTL*)memgive(fct*sizeof(FCTL));
for (i=0;i<fct;i++)
	{
	ff=&_ff[i];
	ff->id=idx_fid[i];
	}
IMDB_FLD ia;
for (i=0;i<fct;i++) ia.load_fctl(&_ff[i]);
mct=ia.recct();
}

int32_t SCAN_ALL::scan_all(int *subscript)	// subscript set to NOTFND by caller to start the loop
{															// successively return next imdb number until EOF = NOTFND
if (++subscript[0]>=mct) return(NOTFND);
int32_t imno=((IMN2XLT*)_ff->ph)[subscript[0]].imno;	// _ff EQV ff[0] (the FID_IMDB_NUM entry)
return(imno);
}

char*  SCAN_ALL::get(int32_t imno, char fld_id, char *buf)
{
if (fld_id==FID_IMDB_NUM) return(strfmt(buf,"%d",imno));
*buf=0;
int fx=stridxc(fld_id,idx);		// If stridxc returns NOTFND (unknown ID) we're fucked anyway
if (fx==NOTFND) return(buf);
ff=&_ff[fx];
IMN2XLT *im=(IMN2XLT*)_ff->ph;	// _ff EQV ff[0] = FCTL for FLD_IMDB_NUM mastertable
int mx=in_table(NULL,&imno,im,mct,sizeof(IMN2XLT),cp_long);
if (mx==NOTFND) return(buf);	// THIS movies not indexed in imdb.dbf, so return empty string

int xlt=((IMN2XLT*)_ff->ph)[mx].xlt;
if (fld_id==FID_YEAR)	// For these, 'ph' = 1-byte uchar Year - 1900
	return(strfmt(buf,"%d", ((char*)ff->ph)[xlt] + 1900));

if (fld_id==FID_RUNTIME)	// For these, 'ph' = 2-byte short runtime in minutes
	{int m=((short*)ff->ph)[xlt]; return(strfmt(buf,"%d:%02d", m/60, m%60));}

char *av;
if (fld_id==FID_GENRE || fld_id==FID_TITLE)	// For these, 'ph' = DYNAG(2) of short ptrs into 'av'
	{
	av=(char*)((DYNAG*)ff->pa)->get(((short*)ff->ph)[xlt]);   // subscript into allv for this movie's value
	if (fld_id==FID_GENRE) genre_uncompress(av,buf);
	else strcpy(buf,av);
	return(buf);
	}
if (fld_id==FID_CAST || fld_id==FID_DIRECTOR)	// For these, 'ph' = DYNAG(0) and each vari-length entry
	{															// is multiple 2-byte Z2i elements, each ptr->ONE person
	for (av=(char*)((DYNAG*)ff->ph)->get(xlt); *av; av+=2)
  		strendfmt(buf,"%s%s",buf[0]?", ":"", (char*)((DYNAG*)ff->pa)->get(Z2i((uchar*)av)));
	return(buf);
	}
m_finish("unknown fid");
return(0);
}

SCAN_ALL::~SCAN_ALL(void)
{
for (int i=0;i<fct;i++)
	{
	ff=&_ff[i];
	if (ff->id==FID_IMDB_NUM) {memtake(ff->ph); continue;}
	delete ((DYNAG*)ff->pa);
	if (ff->id==FID_GENRE || ff->id==FID_TITLE || ff->id==FID_YEAR || ff->id==FID_RUNTIME) memtake(ff->ph);
	else delete ((DYNAG*)ff->ph);
	}
memtake(_ff);
}
