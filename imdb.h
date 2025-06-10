#ifndef IMDB_H
#define IMDB_H

#include "db.h"
#include "qblob.h"

#define FID_IMDB_NUM 'I'
#define FID_GENRE 'G'
#define FID_RUNTIME 'R'
#define FID_CAST 'C'
#define FID_DIRECTOR 'D'
#define FID_TITLE 'T'
#define FID_YEAR 'Y'

#define NUMCOLS 17

#define COL_RECENT 0
#define COL_TITLE 1 
#define COL_YEAR 2
#define COL_RATING 3 
#define COL_TMDB 4 
#define COL_IMDB 5 
#define COL_DIRECTOR 6 
#define COL_ADDED 7 
#define COL_SEEN 8 
#define COL_GB 9 
#define COL_CAST 10 
#define COL_RUNTIME 11 
#define COL_NOTES 12 
#define COL_NOTES1 13 
#define COL_RATING1 14 
#define COL_GENRE 15
#define COL_MYSEEN 16
 
struct FLDX
{
	const char	*fnm,		// fixed internal 'system' name of field
					*name;	// user-displayed column heading of field
	int align;				// left - centre - right
	char fid;				// fixed internal systemm ID of field
};
extern FLDX fld[NUMCOLS];

struct FCTL
{
	char	id;
	void	*ph,			// (always present) Array of actual values, OR some kind of indirection into 'pa' 
			*pa;			// (only if used for any given 'id' fieldtype) Some kind of table of "all values" for id
};

extern char idx_fid[];

class IMDB_API : public DBX
{
public:
	IMDB_API(const char *fn=NULL); // Constructor used by app progs
	~IMDB_API();
	const char *get(int32_t imno, const char *name); // return text value of 'name' metric from api call
	void put(int32_t imno, const char *buf);			 /// add or update with the entire 'buf' returned by api call
	bool del(int32_t imno);
	DYNAG *get_tbl(void); // Return list of all imno's (in ascending order, as it happens)
	void log(void);
private:
	void db_open(char *fn);
	HDL im_btr;
	struct IA_HDR
		{
		char ver, fill[3];
		RHDL im_rhdl;
		char fill2[24];
		} hdr;
	struct IA_KEY
		{
		int32_t imno;
		short bd;
		RHDL rh2;		// not currently used
		} kyx;
	char cachebuf[8192];
   JSS4 *jss4;
};

struct IMN2XLT
{
	int32_t imno;
	short xlt;
};
struct KYX
{
	char fid, fill[3];
	RHDL rhav;
};


#define STD_ERRNULL "2>/dev/null"

bool omdb_all_from_number(int32_t imno, char *buf8k);
bool omdb_all_from_name(const char *name, int year, char *buf8k);


#endif // IMDB_H
