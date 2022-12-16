#ifndef OMDB_H
#define OMDB_H

#include <cstdint>

extern int omdb_read_only;

struct RHNM {RHDL rh; char *nm;};

#pragma pack(push, 1)
struct	OM_KEY
	{
	RHDL		nam,					// locates a single record (TODO - 2 tab-separated names = MINE + IMDB's)
				director, cast,	// locates an ARRAY of rhdl's - each locating a single 'person-name' record
				plot, notes;		// 'plot' UNUSED
	int32_t		imdb_num; 
	short		year, emdb_num, added, seen, filesz, runtime;
	char		rating;
	uchar		seen_hr, unused[2];
	};
#pragma pack(pop)


class OMDB {
public:
OMDB(void);						// Constructor used by app progs
~OMDB();
DYNTBL *dyntbl_out(void);			// returns OUTPUT table from data stored in omddb.dbf
bool get(EM_KEY1 *e);				// If e->imdb_num key exists, populate 'e' and return TRUE, else FALSE
bool put(EM_KEY1 *e);				// If e->imdb_num key exists return FALSE, else add this movie
bool upd(EM_KEY1 *e);				// ONLY update moviename + rating + seen + emdb_num + filesz
bool del(int32_t imdb_num);		// Return Yes/No for Success/Failure
const char *get_notes(int32_t imdb_num);
void put_notes(int32_t imdb_num, const char *txt);
void restore(void);
void backup_if_needed(void);
//void fix(int32_t imdb_num, short eno);
//void fix2(void);
//void test(void);

private:
void backup(void);
void	db_open(char *fn);
void	em2om(OM_KEY *om, EM_KEY1 *em);
void	om2em(EM_KEY1 *em, OM_KEY *om);
RHDL	people2rhdl(char *str);	// RHDL -> LIST of rhdl's (of each comma-separated name in 'str')
char	*rhdl2people(char *str, RHDL rh, int sz);	// Reverse the above (copy csv list into 'str')
RHDL	find_or_add_person(char *str);	// rhdl of person's name - newly created if necessary
void	create_rhnm(void);	// Instantiate DYNTBL of names (and populate if *.dbf contains any data)
void	scrap_rhnm(void);
void  add_names_to_rhnm(RHDL rh_list, DYNTBL *rhall);		// rh_list is director or cast, rhall is all previously-loaded rhdl's
int	namsub(RHDL rh_list, DYNAG *d);	// return number of namesubscripts added to passed tbl (of shorts)
RHDL	namadd(int ct, HDL f);		// Construct, write, & return "rh_list" rhdl (director or cast)
HDL	bck_open(const char *mode);
void	rename_backups(int32_t dttm);
HDL	db,om_btr;
struct	{char ver, fill[3]; RHDL om_rhdl, Xnames_rhdl; char fill2[20];} hdr;
DYNTBL *rhnm=NULL;
bool	dbactivated;	// If dbstart didn't do anything in constructor, we won't dbSTOP in destructor
char	bk_fn[256];
};

struct	OM1_KEY
	{
	int32_t	imdb_num; 
	RHDL		notes;
	char		rating;
	};
class OMDB1 {
public:
OMDB1(void);						// Constructor used by app progs
~OMDB1();
int get_rating(int32_t imdb_num);
void put_rating(int32_t imdb_num, int rating);
const char *get_notes(int32_t imdb_num);
void put_notes(int32_t imdb_num, const char *txt);
bool get_om1(int32_t imdb_num, OM1_KEY *om1);
private:
void	db_open(char *fn);
HDL	db,om_btr;
struct	{char ver, fill[3]; RHDL om_rhdl, Xnames_rhdl; char fill2[20];} hdr;
bool	dbactivated;	// If dbstart didn't do anything in constructor, we won't dbSTOP in destructor
};


class USRTXT {	// Get/Put User Notes (steve's in main OMDB.dbf, others in their personal OMDB.db1)
public:
    USRTXT(int32_t imdb_num);
    const char *get(void);
    void put(const char *t);
    ~USRTXT() {memtake(txt);};
private:
int32_t imdb_num;
const char *txt=0;
};



DYNTBL *make_emk();	// load new (from OMDB)

#endif // OMDB_H
