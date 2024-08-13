#ifndef PDEF_H
#define PDEF_H

#include <stdint.h>

#define _cdecl
#define _stdcall

typedef unsigned char uchar;


#define Xecho printf
#define stricmp strcasecmp
#define SetErrorText printf

#define	FA_ORDINARY	0x00	// regular file
#define	FA_DIR		0x10	// Directory
#define	FA_SPECIAL	0x40	// NOT regular file / directory (Hidden?, Label?, System?,...)

//typedef long long int64_t;
typedef	unsigned int Uint;
//typedef	unsigned long ulong___;
typedef	uint32_t Ulong;
typedef	unsigned short ushort;

typedef	char		*HDL;	// abstract data type handle
typedef	uint32_t	RHDL;	// database record handle WAS 'long''
// All 'long' (or Ulong) fields in DB file-based structures to be redefined as int32_t

#define YES     1
#define NO      0
#define NOTFND  -1

//	Some useful macros
//	General defines used almost everywhere

#ifndef ABS
#define	ABS(x) 	((x)<0?-(x):(x))
#endif

#define	BKSP	8
#define	BIGLONG	0x7fffffff
#define	SPACE	' '

#define	CHR_HASH	35
//#define	CHR_QT1	39
#define	CHR_QTSINGLE	39
#define	CHR_QTDOUBLE	34
#define	COLON				58
#define	BACKSLASH		92
#define	AMPERSAND		38

#define  EQUALS '='
#define	COMMA	','
#define	SEMICOLON	';'
#define	PLUS	'+'
#define	CPMEOF  0x1A		// <ctl-z> byte for eof in text files
#define	CRET    '\r'		// 13
#define	DIV2(x)	(((unsigned)(x))>>1)
#define	EQ      0
#define	ESC		27
#define FNAMSIZ 256				// Max fully-qualified path is 255 chars + nullbyte
#define	GT      1
//#define	INRANGE(a,b,c)	(((b)<(a))?(a):(((b)<(c))?(b):(c)))
#define	ISALPHA(c)	((('a'<=(c))&&((c)<='z')) || (('A'<=(c))&&((c)<='Z')))
#define	ISHEX(c)	((('0'<=(c))&&((c)<='9')) || (('A'<=(c))&&((c)<='F')))
#define	ISDIGIT(d)	(('0'<=(d))&&((d)<='9'))
#define	LNFEED   '\n'		// 10
#define	LT       (-1)

#ifndef MAX
#define	MAX(a,b) (((a)>(b))?(a):(b))
#endif
#ifndef MIN
#define	MIN(a,b) (((a)<=(b))?(a):(b))
#endif


#define	NULLPTR	((void *)0)		// use to pass null pointer to funcs
#define	NULLHDL	((HDL)0)
#define	NULLRHDL	((RHDL)0)
#define	TAB			'\t'
#define	TOLOWER(c)	((('A' <= c) && (c <= 'Z')) ? ((c)+('a'-'A')):(c))
#define	TOUPPER(c)	((('a' <= c) && (c <= 'z')) ? ((c)-('a'-'A')):(c))
#define	ISLOWER(c) ((c)>='a' && (c)<='z')
#define	ISUPPER(c) ((c)>='A' && (c)<='Z')
#define	ISALPHANUMERIC(c) (ISDIGIT(c) || ISALPHA(c))

//	Reserved Error Codes
#define ERR_ARC_NOCOPY	(1)		// DllCopyAll failed to write anything to o/p Ticket Archive
#define	ERR_ARC_MISSING	(3)		// ArchiveCopy source file doesn't exist
#define ERR_ARC_DUPLICATES (99)	// ArchiveCopy duplicate discs warning
#define SE_FLDID		(92)	// Unknown Field-ID

#define	SE_USER_CANCEL	(-200)	// Processing aborted by user or untrapped error

//	-257 to -299	Miscellaneous
#define	SE_OUTOFHEAP	(-258)	// No memory for memgive()
#define	SE_BADALLOC		(-261)	// Attempt to memgive() 0 bytes
#define	SE_MEMBOUNDS	(-262)	// Write before/after memory block
#define	SE_MEMCHK		(-263)	// MEMCHK tables are corrupt!
#define	SE_MEMDLR		(-264)	// Dynamic linked list pointer error
#define	SE_MEMBAD		(-266)	// Memory buffer under/overwrite
#define	SE_MEMPTR		(-267)	// Attempt to release unallocated pointer
#define SE_BUFF64K		(-268)	// 64 Kb api buffer overwrite (pre 05/12/06 was -581)
#define SE_MEMTBL		(-269)	// Error merging memory tables
#define SE_DATE			(-280)	// Date/Day/TermType error

//	-300 to -320	OS high-level filing system
#define	SE_ERR_PATH		(-300)	// Error accessing directory
#define	SE_ERR_FILE		(-301)	// Error accessing file
#define	SE_ARC_EXISTS	(-302)	// File exists that logically should not (pre 05/12/06 was -201)
#define	SE_ARC_MISSING	(-303)	// File missing that logically must exist (pre 05/12/06 was -202)
#define	SE_RENAME		(-304)	// OS error renaming file (pre 05/12/06 was -202, -1)
#define SE_MOD_UPD		(-305)	// Error applying updates from file into Ticket Archive

//	-321 to -349	OS low-level IO system
#define SE_BADWRITE		(-321)	// write error
#define SE_BADREAD		(-322)	// read error
#define SE_BADSEEK		(-323)
#define SE_SJHLOG		(-324)
#define SE_RD_ONLY		(-325)	// Tried to write to file opened as ReadOnly
#define SE_TMPCREATE	(-330)	// Couldn't open TMP_DB

// Watch out for MOVE3BYTES - It evaluates 'a' twice!! (usually harmless, but keep an eye on it!)
#define MOVE8BYTES(a,b) (*(int64_t*)(a)=*(int64_t*)(b))
#define MOVE4BYTES(a,b) (*(int32_t*)(a)=*(int32_t*)(b))
#define SAME4BYTES(a,b) (*(int32_t*)(a)==*(int32_t*)(b))
#define MOVE3BYTES(a,b) (*(int32_t*)(a)=(*(int32_t*)(b)&0xffffff)|(*(int32_t*)(a)&0xff000000))
#define SAME3BYTES(a,b) (((*(int32_t*)(a))&0xffffff)==((*(long*)(b))&0xffffff))
#define MOVE2BYTES(a,b) (*(short*)(a)=*(short*)(b))	// move 2 bytes to a(dst) from b(src)
#define SAME2BYTES(a,b) (*(short*)(a)==*(short*)(b))


//typedef int	(_cdecl *PFI_v_v)(const void*, const void*);
typedef int	(_cdecl *PFIi)(int);
typedef int	(_cdecl *PFI_i)(int*);
typedef int	(_cdecl *PFI_v)(const void*);
typedef int	(_cdecl *PFI_v_v)(const void*, const void*);
typedef void(_cdecl *PFV_v_c)(const void*, char*);
typedef int	(_cdecl *PFIv)(void);
typedef HDL	(_cdecl *PFHi_v)(int,void*);
typedef void(_cdecl *PFVh)(HDL);
typedef void(_cdecl *PFVv)(void);
typedef void(_cdecl *PFV_v)(void*);
typedef int	(_cdecl *PFIi_v)(int,void*);
typedef HDL	(_cdecl *PFIh_v)(HDL,void*);

void	m_finish(const char *fmt,...);

int		bit_test(const char* m, int b);
void	bit8_set_one_bit(char *mask, int bitnum);
void	bit_set(char *m,int b);
void	bit_unset(char *m,int b);
int		bit_only1(char *m, int bit_ct);

//char    *SetErrorText(const char *fmt,...);  // not yet written!


#define	DB_DIRTYPOS	38		// Offset of "dirty" flag byte database header
#define	DB_IS_CLEAN	'('		// Value in header while database is CLEAN
#define	DB_IS_DIRTY	'['		// Value in header while database is DIRTY


int		cp_bytes(const void *p1, const void *p2);
int		cp_strn_ks0(const void *p1, const void *p2);
int		cp_mem1(const void* p1, const void* p2);
int		cp_mem2(const void* p1, const void* p2);
int		cp_mem3(const void* p1, const void* p2);
int		cp_mem4(const void* p1, const void* p2);
int		cp_mem6(const void* p1, const void* p2);
int		cp_mem8(const void* p1, const void* p2);
int		cp_short(const void *p1, const void *p2);
int _cdecl cp_short2(const void *a, const void *b);
int		cp_ushort(const void *p1, const void *p2);		// the values as parameters

int		cp_long(const void *p1, const void *p2);
int		cp_int32_t(const void *p1, const void *p2);
int		cp_int86_t(const void *p1, const void *p2);

int		cp_ulong(const void *p1, const void *p2);
int _cdecl cp_ulong2(const void *a, const void *b);
int _cdecl cp_ulong4(const void *a, const void *b);


int		cp_short_v(short a, short b);					// These 4 take the ACTUAL
int		cp_ushort_v(ushort a, ushort b);				// values as parameters
int		cp_long_v(int32_t a, int32_t b);
int		cp_ulong_v(Ulong a, Ulong b);



#endif
