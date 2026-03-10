#ifndef IMDB_H
#define IMDB_H

#include "db.h"
#include "memgive.h"
#include "qblob.h"

class IMDB_API : public DBX		// accesses "imdb.api" database of results from api calls
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
