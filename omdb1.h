#ifndef OMDB1_H
#define OMDB1_H

#include <cstdint>
#include <string>

#include "db.h"
#include "parm.h"

extern int user_not_steve;


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
OMDB1(bool _master);						// Constructor used by app progs
~OMDB1();
int	recct(void) {return(btrnkeys(om_btr));};
int get_rating(int32_t imno);
void put_rating(int32_t imno, int rating, OM1_KEY *omzk);
std::string get_notes(int32_t imno);
bool upd(OM1_KEY *k);		// update initially only to fix missing date added
void put(OM1_KEY *k);
void put_notes(int32_t imno, const char *txt);
bool get_om1(int32_t imno, OM1_KEY *k);
bool get_ge(OM1_KEY *k);
bool scan_all(OM1_KEY *k, bool *again);
bool del(int32_t imno);		// Return Yes/No for Success/Failure
RHDL str2rh(const char *str) {return(zrecadd(db,str,strlen(str)+1));};
char *rh2str(RHDL rh, char *buf) {if (rh!=0 && zrecget(db,rh,buf,0)) return(buf); return(0);};
void list_missing(void);

private:
void	db_open(char *fn);
HDL	om_btr;
struct	{char ver, fill[3]; RHDL om_rhdl, Xnames_rhdl; char fill2[20];} hdr;
bool master;
char	bk_fn[256];
};

class USRTXT {	// Get/Put User Notes (mine in main smdb.mst, others in their personal smdb.usr)
public:
    USRTXT(int32_t num) {imno=num; om=new OMDB1(!user_not_steve);};
    std::string get(void) {return(om->get_notes(imno));};
    void put(const char *t) {om->put_notes(imno,t);};
	 DYNAG *extract(const char *subrec_name);
	 void insert(DYNAG *subrec_tbl);
    ~USRTXT() {delete om;};
private:
OMDB1 *om;
int32_t imno;
};

char *fix_colon(char *nam);	// Change any ":" in MovieName to " -" and delete any '?'
char *strquote(const char *buf, const char *id);  // used by usherette
int32_t tt_number_from_str(const char *s);
//bool call_api_with_number(int32_t imno, char *buf, int mxsz);
//void get_conf_path(PARM *pm, char *fn, const char *ext);

#endif // OMDB1_H
