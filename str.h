#ifndef STR_H
#define STR_H

#define	STR_LJUST 	0			//ustification types for strjust()
#define	STR_RJUST 	1
#define	STR_CENTER 	2


char	*strancpy(char *dst, const char *src, int bufsz); // NE Ansi - mine ALWAYS appends eos null
char  *stradup(const char *s);	// NE Ansi - mine uses memgive() memory allocator
char  *strfmt(char *str ,const char *fmt,...);
//char    *_strfmt(char*s1 ,const char*s2,va_list va);
char	*_strfmt(char *str,const char *fmt,va_list);
char	*strendfmt(char *str, const char *fmt,...);   // append to, but return START of, passed string
char	*_strnfmt(char*,int, const char*, va_list);
char	*strend(const char *str);
char	*strdel(char *str, int ct);				// delete 'ct' chars from front of 'str'
char	*strfill(char *str, int ct, int chr);	// (sets str[ct]=0)
char	*strins(char *str, const char *ins);	// add 'ins' to front of 'str'
char	*strinsc(char *str, int ins);				// add char 'ins' to front of 'str'
int   stridxc(char c,const char *s);	// Offset of LEFTMOST (if any) instance of 'c' in 's'
int   strridxc(char c,const char *s);	// Offset of RIGHTMOST (if any) instance of 'c' in 's'
int	stridxs(const char *substr, const char *str);	// return offset of substr in str, or NOTFND
int 	strinspn(const char *any_of_these, const char *in_this_string);
char  *strip(char *str, char chr); // remove any & all occurences of 'chr' in 'str'
char	*strjust(char* str, int width, int type, int fillchr);
char	*strnfmt(char*, int len, const char*,...);
char	*strrjust(char* str, int width);
int	strtoken(char*,char*,char*);
char	*strtrim(char*);				// Remove leading AND trailing white space
char	*strxlt(char *str, char from, char to);
char	*strcommas2tabs(char *csvbuf);   // convert field-separating (NOT within quoted strings) commas to tabs
//char  *Strlast4(const char *str);      // return ptr-> last 4 chars (useful for checking file extensions]
//char  *str_filesize(int64_t sz);        // return ptr-> static buffer formatted as <n> "bytes, Kb, Mb, Gb,..."
char *str_size64(int64_t size);
void str_slash2dash(char *title);      /// change any slashes to dashes in titles that can be file/foldername



bool  strget_yn(char *buf, int bufsz, const char *prompt=NULL);
bool  strget_yn_loop(const char *prompt);       // only returns when either 'y' or 'n' keyed


bool	same_alnum(const char *a, const char *b);	// Are strings "the same" ignoring any non-alphanumeric chars?

char	*vb_field(const char *rec, int n);
char  *tabify(char *str); // Convert non-quotated commas to tabs

int     dot2i(const char *s);    // if s contains 2 digits separated by '.' return 10x the number, else NOTFND

ushort	a2i(const char *str, int len);
Ulong	a2l(const char *str, int len);
int64_t a264(const char *s);    // convert 3-digit (up to 1 decimal place) value + T/G/M/K/B to int64
int		a2l_signed(const char *str, int len);
extern	int a2err, a2err_char;			// Error on last call to a2l() (0=OK, 1=Fail, 2=EmptyString. If 1, a2err_char is the non-digit we found

uchar		*i2Z(ushort i);	// store ushort in 2-byte 'radix 255 string' NOT CONTAINING NULLS
ushort	Z2i(const uchar *s);		// retrieve ushort from 2-byte 'radix 255 string' NOT CONTAINING NULLS
int      x2l(const char *s, int len); // hex to int - len must be even, non-zero

// #define BIT8_AND(a,b) ((*(long*)(a)&*(long*)(b)) || (*(long*)(&((char*)(a))[4])&*(long*)(&((char*)(b))[4])))
#define BIT8_AND(a,b)	(  ((*(int64_t*)(a)) & (*(int64_t*)(b)))  !=0)	// TRUE if any bit is on in BOTH 8-byte flags
#define BIT8_ANDAND(a,b)	(*(int64_t*)(a)) &= (*(int64_t*)(b))		// Logical AND all 64 bits in 'b' into 'a'
#define BIT8_OR(a,b)	(*(int64_t*)(a)) |= (*(int64_t*)(b))			// Logical OR all 64 bits in 'b' into 'a'
#define BIT8_XOR(a,b)	(*(int64_t*)(a)) ^= (*(int64_t*)(b))			// Logical XOR all 64 bits in 'b' into 'a'
#define BIT8_ON(a)		(*(int64_t*)(a) !=-0)							// TRUE if any bit is on in this 8-byte flag
#define BIT8_SET0(a)	(*(int64_t*)(a)=0)								// Set all 64 bitflags in 8-byte area to zero

class BitMap
{
public:
BitMap(int _ct);
~BitMap();
void	clear_all(void);
void	set(int b);
void	unset(int b);
int		is_on(int b);
int		first(int on_wanted);	// return subscript of first bit that's ON (or first that's OFF if on_wanted=False)
int		Bit_only1(void);
private:
int		ct;
char	*bm;
};

#endif
