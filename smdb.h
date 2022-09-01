#ifndef SMDB_H
#define SMDB_H

#include <cstdint>

//#define APIKEY "66e03abd"
#define APIKEY "c9497777"

#define CACHE_FILE "/home/steve/emdb.txt"
#define CSV_FILE "/home/steve/Cloud/OneDrive/emdb/emdb.csv"

struct	EM_KEY {char nam[60]; short year, emdb_num; int32_t imdb_num; char rating;}; // Each BTR entry includes RHDL (maybe NULL) to 'mct' MEDIA entries
struct	EM_KEY1 {EM_KEY e; char director[30], cast[60]; short added, seen, filesz, runtime;};

#pragma pack(push, 1)
struct	EM_MEDIA {char locn, fct, dct, dvd; int64_t sz;};		// The (variable length) array of media locations is in no particular sequence!!!
struct	IM_SIZE {int32_t imdb_num; int64_t sz;};			// 'sz' = BIGGEST, if multiple copies
#pragma pack(pop)

#define MAX_EMM 3

//int _cdecl cp_em_key(EM_KEY *a, EM_KEY *b);	// sort by year within name
int cp_em_key(EM_KEY *a, EM_KEY *b);	// sort by year within name

struct ULOCN {char locn; long dttm;};


class SMDB {
public:
SMDB(const char *path);						// Constructor used by app progs
~SMDB();
void	update_all(int slot, DYNAG *em);
int	scan_all(EM_KEY *e, int reverse_order, int *again);
void	locn_upd(char locn);
void	locn_list(void);
void	list_slots(void);
void	list_numcopies(void);
void	list_selection(void);
void	list_no_media(void);
void	josh_copy(DYNTBL *wanted_imdb_numbers);
void	prepare_copy(void);
void	find_movie_by_imdb_num(int emdb_num);
int	find_movie_by_imdb_num2(EM_KEY *e, EM_MEDIA *m);	// NOTFND = Error, 0-3 "No of copies in 'm'"
void	test1(void);
void	test2(void);
void	test3(void);
void	purge(char *movie_folder_pth);
int	add_or_upd(EM_KEY *e, EM_MEDIA *m);	// Add or update the KEY + RECORD with passed key using passed buffers
void	remove_unmatched(DYNTBL *emk);
int	return_all_keys(DYNTBL *emk);	// Populate passed table wth all key values in *.dbf
int   get(EM_KEY *e, const char *nam=NULL, int year=0);
int   get_emk_by_imdb_num(EM_KEY *e);
void	sz_by_imdb(DYNTBL *tbl);

private:
int		ct;
HDL		db,em_btr;
RHDL	rhdl;
struct	{char ver, fill[3]; RHDL em_rhdl, locn_upd_rhdl; char fill2[20];} hdr;
bool	dbactivated;	// If dbstart didn't do anything in constructor, we won't dbSTOP in destructor
void	db_open(char *fn);
RHDL	add_or_upd_media(RHDL rh, EM_MEDIA *m, EM_MEDIA *mm, int ct); // 'ct' = number of entries in 'mm' ('m' is just a ptr to a single instance)
void	list_media(EM_KEY *e, RHDL rh, int dets);	// If dets=True, list all slot folder+size entries
};

#endif
