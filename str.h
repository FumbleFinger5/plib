#define	STR_LJUST 	0			//ustification types for strjust()
#define	STR_RJUST 	1
#define	STR_CENTER 	2


char	*strancpy(char *dst, const char *src, int bufsz);
char  *stradup(const char *s);
char  *strfmt(char *str ,const char *fmt,...);
char  *_strfmt(char*s1 ,const char*s2,va_list va);
char	*_strfmt(char *str,const char *fmt,va_list);
void	strendfmt(char *str, const char *fmt,...);
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
void strip(char *str, char chr); // remove any & all occurences of 'chr' in 'str'
char	*strjust(char* str, int width, int type, int fillchr);
char	*strnfmt(char*, int len, const char*,...);
char	*strrjust(char* str, int width);
int	strtoken(char*,char*,char*);
char	*strtrim(char*);
char	*strxlt(char *str, char from, char to);

char *vb_field(const char *rec, int n);


ushort	a2i(const char *str, int len);
Ulong	a2l(const char *str, int len);
int		a2l_signed(const char *str, int len);
extern	int a2err, a2err_char;			// Error on last call to a2l() (0=OK, 1=Fail, 2=EmptyString. If 1, a2err_char is the non-digit we found



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
