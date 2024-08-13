#ifndef FLOPEN_H
#define FLOPEN_H

#define	FIL_EOF		1
#define	FIL_STDIO	2
#define	FIL_DBF		4			// Is this a Database file?
#define	FIL_DIRTY	8			// Only applies to Database files
#define	FIL_LOG		16			// Only set for SJH.LOG (don't log file operations on THIS one!)

#define FBUFSZ 2048

// For a DBF, the first time we write to it, set the 'dirty' byte at offset
// Db_DIRTYPOS to DB_IS_DIRTY instead of DB_IS_CLEAN, so DB.C can check the safety of the database.


struct PF_FIL	{
	char	*prv,*nxt;
	short	fd,	mode,	flag,	ungot_c,	at_byte,	lst_rd_byte;
	char	*fbuf,fnam[1];
	};

#define	H_FL	((PF_FIL *)h_fl)

#define	FROMBEG		0
#define	FROMEND		2
#define	READ		O_RDONLY
#define	WRITE		O_WRONLY
#define	READWRITE	O_RDWR

HDL 	flopen(const char*,const char*);		// r=ReadWrite, R=ReadOnly, a=Append, w=Write
HDL     flopen_trap(const char*,const char*);
void	flclose(HDL);
int	    flcloseall(void);
void	flckpt(HDL);
int 	fleof(HDL);
ushort	flget(void*,ushort,HDL);
int 	flgetln(char*,int,HDL);
char	*flnam(HDL h_fl);
int 	flgetc(HDL fl);
ushort	flgetat(void *buffer,ushort bytes,long pos,HDL fl);
void	flput(const void*,int,HDL);
void	flputc(int,HDL);
void	flputat(const void *buffer, int bytes, long pos, HDL fl);
void	flputs(const void *s, HDL f);
void	flputln(const void* line, HDL fl);
void	flsafe(HDL);					// set hdr flagbyte to say "database is safe"
long	flseek(HDL,long pos,int mode);	// mode 0/1/2 = from BEG/CUR/END
//void	flnam_env(char *p, const char *fn, const char *envstr);

int flgets(char *str, int bufsiz, HDL h);	// bufsiz must be big enough for terminating NULL
int flgetstr(char *str, int bufsiz, HDL h);	// bufsiz must be big enough for terminating NULL

#endif