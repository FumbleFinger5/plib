#ifndef OMDB1_H
#define OMDB1_H

#include <cstdint>
#include <string>

#include "db.h"
#include "parm.h"

extern int user_not_steve, running_live;


struct	OM1_KEY
	{
	int32_t	imno; 
	RHDL		notes;
	uchar		rating, partwatch;
	ushort	sz;
	RHDL		mytitle;
	int32_t	seen;
	ushort	added;
	uchar		tv, unused;
	int32_t 	tmno;
	};

struct OMZ {OM1_KEY k; char title[64]; short year;};

class OMDB1 : public DBX {				// The main movie library database smdb.mst - 1 record per movie
public:
OMDB1(bool _master, bool create=false);						// Constructor used by app progs
void  backup(void);
bool  del(int32_t imno);		// Return Yes/No for Success/Failure
uchar get_rating(int32_t imno);
bool  get_om1(int32_t imno, OM1_KEY *k);
bool  get_ge(OM1_KEY *k);
std::string get_notes(int32_t imno);         // was char *get_notes();
void  put(OM1_KEY *k);
void  put_notes(int32_t imno, const char *txt);
void  put_rating(int32_t imno, int rating, OM1_KEY *omzk);
int	recct(void) {return(btrnkeys(om_btr));};
void  restore(void);
char  *rh2str(RHDL rh, char *buf) {if (rh!=0 && zrecget(db,rh,buf,0)) return(buf); return(0);};
bool  scan_all(OM1_KEY *k, bool *again);
RHDL  str2rh(const char *str) {return(zrecadd(db,str,strlen(str)+1));};
bool  upd(OM1_KEY *k);		// update initially only to fix missing date added
bool  upd_title(int32_t imno, const char *title);

private:
void	db_open(char *fn);
HDL	flopen_bak(const char *mode);
HDL	om_btr;
struct	{char ver, fill[3]; RHDL om_rhdl, Xnames_rhdl; char fill2[20];} hdr;
bool master;
};

class USRTXT {	// Get/Put User Notes (mine in main smdb.mst, others in their personal smdb.usr)
public:
USRTXT(int32_t num, OMDB1 *_om=NULL);
std::string get(void);
void put(const char *t);
DYNAG *extract(const char *subrec_name);
void insert(DYNAG *subrec_tbl);
~USRTXT();
private:
bool om_passed;      // TRUE if address of existing OMDB1 passed to constructor (so don't create / release it)
OMDB1 *om;
int32_t imno;
};

#endif // OMDB1_H
