#include <stdio.h>

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <cstdarg>

#include "pdef.h"
#include "memgive.h"
#include "flopen.h"
#include "str.h"
#include "log.h"

static DYNTBL *open_file_handles;

int open_file_handles_ct(void)
{
if (!open_file_handles) return(NOTFND);
return(open_file_handles->ct);
}

static int  dosread(void *buf, ushort n, PF_FIL *pf)
{
int result;
result = read(pf->fd,buf,n);
if	(result < 0)
	{
	char txt[128];
	strfmt(txt,"attempting to READ %d bytes",n);
	if (pf->fbuf) strendfmt(txt," from offset %d",pf->at_byte);
	throw(SE_BADREAD);
	}
return(result);
}

static long   dosseek(int fd, long lpos, int mode)
{
long result = lseek(fd,lpos,mode);
if	(result < 0) throw(SE_BADSEEK);
return(result);
}

static void  doswrite(const void *buf,ushort n, PF_FIL *pf)
{
if	(write(pf->fd,buf,n) < 0)
	{
	SetErrorText("WRITE error. File %s",pf->fnam);
	throw(SE_BADWRITE);
	}
}



char *flnam(HDL h_fl) {return(H_FL->fnam);}

int fleof(HDL h_fl) {return(H_FL->flag&FIL_EOF);}

					// This flag controls whether to log extended error if flopen fails
int flopen_status;	// 0 = Default = Log all except "File not found"
					// 1 = Called thru flopen_trap = Log ALL errors
					// 2 = Attempting to open sjh.log = NEVER log!


static char *txtmode(const char *mode)
{
static char txt[32];
strfmt(txt,"OPEN Mode:%c",mode[0]);
if (mode[1])
	{
	strendfmt(txt,"%c",mode[1]);
	if (mode[2]) strendfmt(txt," (safe=%d)",mode[2]);
	}
return(txt);
}

HDL flopen(const char *fnam, const char *mode)		// r=ReadWrite, R=ReadOnly, a=Append, w=Write
{
int _flopen_status=flopen_status;
flopen_status=0;
int	f_mv_eof, fd, iomode, f_new, f_buf, f_new_if_missing, flag;
HDL	h_fl;
f_new=f_new_if_missing=f_buf=f_mv_eof=flag=NO;
//_fmode=_O_BINARY;
iomode=READ;			// Assume 'r' (ReadWrite), or 'R' (ReadOnly)
//if (_flopen_status!=2) sjhlog("DB31 mode=[%s]",mode);
switch(*mode)			// maybe it's worth using TOLOWER(*mode) here...
	{
	case 'a': iomode=WRITE; f_mv_eof=f_new_if_missing=YES; break;
	case 'w': iomode=WRITE; f_new=f_new_if_missing=YES; break;
	}
if	(mode[1]=='+')
	{
	iomode=(*mode=='R')?READ:READWRITE;
	if (mode[2]==DB_IS_DIRTY) flag=FIL_DBF;
	}
else
	f_buf=YES;

mode_t mode_permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;

fd = f_new?open(fnam, O_WRONLY | O_CREAT | O_TRUNC,mode_permission):open(fnam,iomode);
if	(fd==NOTFND && f_new_if_missing) fd=open(fnam, O_WRONLY | O_CREAT | O_TRUNC, mode_permission);

if (_flopen_status==2) flag |= FIL_LOG;

if	(fd==NOTFND)
	{
/*
	if (_flopen_status==1) sjhlog_file_error(fnam, txtmode(mode));			// (Always log these)
	else if (_flopen_status==0) sjhlog_file_error(fnam, txtmode(mode), 2);	// DON'T log these if "not found"
*/
	return(NULLHDL);
	}
h_fl=(char *)memgive(sizeof(PF_FIL)+strlen(fnam));
H_FL->fd = (short)fd;
H_FL->mode = (short)iomode;
H_FL->flag = (short)flag;
strcpy(H_FL->fnam,fnam);
H_FL->lst_rd_byte = NOTFND;
if	(f_buf) H_FL->fbuf=(char *)memgive(FBUFSZ);
if (f_mv_eof) dosseek(H_FL->fd, 0L, FROMEND);

if (_flopen_status!=2)
	{
	if (!open_file_handles) open_file_handles=new DYNTBL(sizeof(HDL),(PFI_v_v)cp_ulong);
	open_file_handles->put(&h_fl);
	}

return(h_fl);
}


HDL flopen_trap(const char *fnam, const char *mode)
{
flopen_status=1;
HDL f=flopen(fnam,mode);
if (!f) m_finish("Error opening %s  Mode:%s",fnam,mode);
return(f);
}

static void  flflush(HDL h_fl)
{
if	(H_FL->fbuf && H_FL->at_byte>0 && H_FL->mode==WRITE)
	{
	doswrite(H_FL->fbuf,H_FL->at_byte,H_FL);
	H_FL->at_byte=0;
	}
}


void flclose(HDL h_fl)
{
flflush(h_fl);
if (!(H_FL->flag & FIL_STDIO))
	close(H_FL->fd);
if (H_FL->fbuf)
	memtake(H_FL->fbuf);

if (open_file_handles)
	{
	int n=open_file_handles->in(&h_fl);
	if (n!=NOTFND)
		{
		open_file_handles->del(n);
		if (open_file_handles->ct==0)
			SCRAP(open_file_handles);
		}
	}
memtake(h_fl);
}

int flcloseall(void)
{
int left_open=0;
if (open_file_handles)
	{
	left_open=open_file_handles->ct;
	while (open_file_handles && open_file_handles->ct)
		{
		PF_FIL *f=*(PF_FIL**)open_file_handles->get(0);
//sjhloG("%s still open!",f->fnam);
//char *fn=f->fnam;
		f->flag&=~FIL_DBF;	// Turn off 'is_a_database' bit to stop the dirty flag being unset by flclose()
		flclose((HDL)f);	// (this routine shouldn't be called before explicitly closing all databases)
		}
	SCRAP(open_file_handles);
	}
return(left_open);
}


void flckpt(HDL h_fl)
{
flflush(h_fl);
//_commit(H_FL->fd);	// Shouldn't be needed - see https://stackoverflow.com/questions/259355/
}   

ushort flgetat(void *data, ushort n, long lpos, HDL h_fl)
{
flseek(h_fl, lpos, FROMBEG);
return(flget(data,n,h_fl));
}


// FlGet() returns the actual number of characters read, or 0 on fail.
// See FlGetLn() for a different perpective...
ushort flget(void *data, ushort n, HDL h_fl)
{
PF_FIL *pf=(PF_FIL*)h_fl;
int	ct=0,ct2;
if	(pf->mode == WRITE) 
	throw SE_BADREAD;
if ((pf->flag & FIL_EOF) || !n) return(0);
if	(pf->fbuf)
	{
	if	(pf->lst_rd_byte <= pf->at_byte)
		{   
		pf->at_byte = 0;
		if ((pf->lst_rd_byte=(short)(ct=dosread(pf->fbuf,FBUFSZ,pf)))>0)	// DosRead can only return 0 or >0, not -ve
			ct=flget(data, n, h_fl);
		}
	else
		{
		if ((ct=pf->lst_rd_byte - pf->at_byte)>n) ct=n;
		memmove(data, pf->fbuf+pf->at_byte,ct);
		pf->at_byte = (short)(pf->at_byte + ct);
		if	(ct<n && (ct2=flget((char*)data+ct,(ushort)(n-ct),h_fl))>=0) ct+=ct2;
		}
	}
else ct = dosread(data,n,pf);				// file isn't buffered
if	(!ct) pf->flag|=FIL_EOF;
return((ushort)ct);
}

// Return the Ascii code of character read, or -1 if EOF or a ^Z
int flgetc(HDL h_fl)
{
int	chr=H_FL->ungot_c;
if (chr) H_FL->ungot_c = 0; else if (!flget(&chr, 1, h_fl)) chr=CPMEOF;
if	(chr==CPMEOF) {H_FL->flag|=FIL_EOF; chr=NOTFND;}
return(chr);
}

//#define CHECK_FOR_UNICODE 1
// FlGetLn() returns the length of the string (line) read, excluding
// any CR/LF characters. Thus a blank line returns 0, so we have to return
// -1 when it really IS the end-of-file (or if we read a ^Z character).
// See FlGet() for a different perpective...
int flgetln(char *str, int bufsiz, HDL h_fl)	// NOTE! - bufsiz has to be big enough to include terminating NULL
{
PF_FIL *pf=(PF_FIL*)h_fl;
int		len,cr,c, eof=YES;
len = cr = NO;
#ifdef CHECK_FOR_UNICODE
int		check_for_unicode=(pf->lst_rd_byte==NOTFND);	// If this is the first call to read the file...
#endif
while (!cr)
  	{
	/* if Buffer is filled up, then check if we have read 	*/
	/* up-to an end-of-line (cret-lnfeed).  If so, then 	*/
	/* read the cret and line feed before returning 	*/
	c=flgetc(h_fl);
	if	(len>=bufsiz-1)
		{
		if	(c!=NOTFND && ((pf->ungot_c=(char)c)==LNFEED || c==CRET))
			len+=flgetln((char*)&c,2,h_fl);
		break;
		}
  	if (c==NOTFND) {if (!eof) pf->ungot_c=CPMEOF; break;}
	eof=NO;
	switch (c)
    	{
  		case CRET:
			cr=YES;			// and drop thru
    	case LNFEED:					// (cr ends up 1 if LF, 2 if CR)
    		if ((c=flgetc(h_fl)) != (cr++?LNFEED:CRET))
				pf->ungot_c=(char)c;
	    	break;
    	default:
			str[len++] = (char)c;
#ifdef CHECK_FOR_UNICODE
			if (check_for_unicode && len==2 && str[0]==0xFF && str[1]==0xFE) m_finish("File %s appears to be UniCode!",pf->fnam);
#endif
    	}
	}
str[len] = 0;	    
if (len) pf->flag&=~FIL_EOF;	// If we got some chars, we can't be at eof
return((pf->flag&FIL_EOF) ? -1 : len);
}

// (might not want this one any more)
int flgets(char *str, int bufsiz, HDL h_fl)	// NOTE! - bufsiz has to be big enough to include terminating NULL
{
int	len, c;
for (len=0; len<bufsiz; str[len++]=(char)c)
  	{
	c=flgetc(h_fl);
  	if (!c || c==NOTFND) break;
	if (len==1 && ((unsigned char*)str)[0]==0xFF && c==0xFE) throw(95);	// looks like Unicode!
	}
if (c) {if (len>=bufsiz) throw(94); else str[len++]=0;}
return(strlen(str));
}

// 01/07/22 - now returns total bytes read, INCLUDING eos nullbyte (which MUST be present)
int flgetstr(char *str, int bufsiz, HDL h_fl)	// NOTE! - bufsiz has to be big enough to include terminating NULL
{
int	len=0, c;
while (len<bufsiz)
  	{
	c=flgetc(h_fl);
  	if (c==NOTFND) c=0;	// 10/03/24 if EOF before reading string EOS byte, just silently assume it was present
	str[len++]=(char)c;
	if (c==0) break;
//  	if (c==NOTFND) throw(66);	// Don't expect to hit EOF without reading string EOS byte
	}
if (str[len-1]!=0) {Sjhlog("String [%s] - no EOS nullbyte!", str); throw(67);}
return(len);
}


void flput(const void *data, int n, HDL h_fl)
{
PF_FIL *pf=(PF_FIL*)h_fl;
if	(pf->mode==READ)
	{
	SetErrorText("Illegal WRITE. ReadOnly file %s",pf->fnam);
	throw SE_RD_ONLY;
	}
if	(pf->fbuf)
	{
	int   ct, remain;
	if ((remain=FBUFSZ - pf->at_byte) <= 0)
		{doswrite(pf->fbuf,(ushort)(remain=FBUFSZ),pf); pf->at_byte=0;}
	memmove(pf->fbuf+pf->at_byte,data,ct=MIN(remain,n));
	pf->at_byte = (short)(pf->at_byte + ct);
	if	(ct < n) {flput((char*)data+ct,n-ct,h_fl); ct=n;}
	}
else doswrite(data,(ushort)n,pf);
if	((pf->flag&(FIL_DBF|FIL_DIRTY))==FIL_DBF)		// IF this is the first 'write' to a DBF file...
	{
	pf->flag|=FIL_DIRTY;				// Flag as 'dirty' to prevent recursive loop on next call!
	char dirty=DB_IS_DIRTY;
	flputat(&dirty,1,DB_DIRTYPOS,h_fl);	// Update the 'marker' byte from DB_IS_CLEAN to DB_IS_DIRTY
	flckpt(h_fl);						// Force DOS to flush the file Buffers
	}
}

void flputat(const void *data, int n, long lpos, HDL h_fl)
{
flseek(h_fl, lpos, FROMBEG);
flput(data,n,h_fl);
}

void flputc(int ci, HDL h_fl)
{flput((const char*)&ci, 1, h_fl);}

void flsafe(HDL h_fl)
{
if (H_FL->flag&FIL_DIRTY)
	{
	char clean=DB_IS_CLEAN;
	flputat(&clean,1,DB_DIRTYPOS,h_fl);
	}
}

void flputs(const void *str, HDL f)	// if eos is non-zero, include EOS null-byte in o/p
{
if (*((char*)str)) flput((char*)str,strlen((char*)str),f);
}

void flputln(const void *s, HDL f)
{
flputs(s,f);
//flput("\r\n", 2, f);
flput("\n", 1, f);
}

long flseek(HDL h_fl, long lpos, int mode)	// mode 0/1/2 = from BEG/CUR/END
{
if (H_FL->fbuf) 
	throw SE_BADSEEK;	/* can't seek in a Buffered file */
H_FL->flag&=~FIL_EOF;
return(dosseek(H_FL->fd, lpos, mode));
}
